// CBD-005 (issue #7): memória auxiliar do sort-merge e do partition-hash, medida por alocações
// reais, em sort externo com muitas runs, hash recursivo, skew e grupos de duplicatas.
//
// Contrato (decisão 173, CBD-005):
// - dentro de M: o rascunho das runs e do build (com o diretório), o grupo de duplicatas e a lista
//   de runs, que cresce com b/m (desc_frames molduras);
// - fora de M, declarado: os descritores das partições do hash (TempFile e a entrada na tabela de
//   arquivos da camada 4, que faz o papel do sistema de arquivos), até 2(m − 1) por nível e até
//   kMaxHashPasses níveis, um limite que não depende de b; e o controle O(m).
// A tabela de arquivos guarda só os temporários vivos: as entradas dos apagados são reaproveitadas.
#include "ambiente.hpp"
#include "mini_test.hpp"

#include <iostream>

using namespace cbd;
using namespace memoria;

namespace {

constexpr std::size_t kMetaArquivo = 160;      // entrada na tabela de arquivos por temporário (medido: ~100 B)
constexpr std::size_t kMaxNiveisHash = 6;      // kMaxHashPasses (171)
std::size_t controle(std::size_t m) { return 256 * m + 4096; }

struct Execucao {
    JoinPlan plano;
    std::size_t pico = 0;
    std::uint64_t linhas = 0;
    bool inviavel = false;
};

Execucao executa(Ambiente& amb, JoinAlgorithm alg, std::size_t m, JoinParams jp = {}) {
    const JoinInput pe{amb.scan(amb.mon, amb.me), 0, 0}, pi{amb.scan(amb.mon, amb.mi), 0, 1};
    Execucao x;
    x.plano = std::get<JoinPlan>(amb.mon.plan_join(alg, pe, pi, m, jp));
    const JoinInput se{amb.scan(amb.sm, amb.se), 0, 0}, si{amb.scan(amb.sm, amb.si), 0, 1};
    amb.sm.flush_all();
    amb.sm.invalidate_all();
    auto res = amb.sm.reserve(m);
    ResultSink sink(ResultShape{}, x.plano.swapped);
    PhaseRecorder ph(amb.sm);
    {
        contador::Escopo e;
        try {
            if (alg == JoinAlgorithm::SortMerge) sort_merge_join(amb.sm, se, si, x.plano, res, sink, ph);
            else hash_join(amb.sm, se, si, x.plano, res, sink, ph);
        } catch (const DoesNotFit&) {
            x.inviavel = true;
        }
        x.pico = e.pico();
    }
    x.linhas = sink.rows();
    return x;
}

// Inscrição em miniatura: 5 inteiros e uma observação VarChar, chave com repetições.
Caso inscricoes(std::int64_t n, std::int64_t chaves, std::uint64_t semente) {
    Caso c{"inscrições", make_schema({{"dre", FieldType::Int32}, {"turma", FieldType::Int32}, {"freq", FieldType::Int32},
                                      {"media", FieldType::Int32}, {"sem", FieldType::Int32},
                                      {"obs", FieldType::VarChar, 40, false, true}}),
           {}, {}};
    std::uint64_t s = semente;
    for (std::int64_t i = 0; i < n; ++i)
        c.externa.push_back({std::int64_t(lcg(s) % chaves), i, i % 100, i % 11, 20261 + i % 4,
                             std::string(lcg(s) % 40, 'o')});
    for (std::int64_t i = 0; i < chaves; ++i) c.interna.push_back({i, i, 0, 0, 0, Value{}});
    return c;
}

}  // namespace

// Sort externo com muitas runs: a lista de runs fica nas desc_frames molduras do plano, e o heap do
// operador fica dentro do rascunho + lista, mais os metadados de arquivo de cada temporário e o
// controle O(m).
TEST(MemoriaAuxiliar, SortMergeListaDeRunsDentroDeM) {
    Caso c = inscricoes(60000, 20000, 7);
    Ambiente amb("mem_sm", c);
    const std::size_t bs = amb.sm.block_size();
    // com m = 8, as 900 páginas dariam centenas de runs, e a lista não caberia: o plano é inviável
    const JoinInput pe{amb.scan(amb.mon, amb.me), 0, 0}, pi{amb.scan(amb.mon, amb.mi), 0, 1};
    const auto pequeno = amb.mon.plan_join(JoinAlgorithm::SortMerge, pe, pi, 8, JoinParams{});
    ASSERT_TRUE(std::holds_alternative<NotApplicable>(pequeno));
    EXPECT_TRUE(std::get<NotApplicable>(pequeno).kind == NotApplicable::Kind::Inviavel);
    for (std::size_t m : {std::size_t{16}, std::size_t{32}}) {
        const Execucao x = executa(amb, JoinAlgorithm::SortMerge, m);
        const JoinPlan& p = x.plano;
        ASSERT_FALSE(x.inviavel);
        EXPECT_EQ(x.linhas, pares_esperados(c.externa, c.interna));
        EXPECT_LE(p.frames_used(), m);
        EXPECT_GE(p.desc_frames, 1u);
        const std::size_t lista = p.runs_outer + p.runs_inner + (p.runs_outer + p.fan_in - 1) / p.fan_in +
                                  (p.runs_inner + p.fan_in - 1) / p.fan_in;
        EXPECT_LE(lista * kRunDescriptorBytes, p.desc_frames * bs);
        const std::size_t temporarios = lista;   // runs e saídas de um passe vivas ao mesmo tempo
        const std::size_t limite = (p.run_pages + p.group_pages + p.desc_frames) * bs + temporarios * kMetaArquivo + controle(m);
        std::cout << "  [memoria] sort-merge, m = " << m << ": " << p.runs_outer << " + " << p.runs_inner
                  << " runs, run_pages " << p.run_pages << ", lista em " << p.desc_frames << " moldura(s) ("
                  << lista * kRunDescriptorBytes << " B); pico do heap " << x.pico << " B (limite " << limite << " B)\n";
        EXPECT_LE(x.pico, limite);
    }
}

// Hash join com partições e recursão (m pequeno) e com skew (uma chave domina, bailout): os
// descritores das partições não dependem de b, e a recursão para em kMaxHashPasses passes.
TEST(MemoriaAuxiliar, HashDescritoresDePartesLimitadosPorM) {
    Caso c = inscricoes(30000, 6000, 9);
    Caso skew = inscricoes(8000, 400, 11);
    for (auto& r : skew.externa)
        if (std::get<std::int64_t>(r[0]) % 4 != 0) r[0] = std::int64_t{7};   // 3/4 das linhas com a chave 7
    int n = 0;
    for (Caso* cc : {&c, &skew}) {
        Ambiente amb("mem_hash_" + std::to_string(n++), *cc);
        const std::size_t bs = amb.sm.block_size();
        for (std::size_t m : {std::size_t{4}, std::size_t{8}, std::size_t{32}}) {
            const Execucao x = executa(amb, JoinAlgorithm::Hash, m);
            const JoinPlan& p = x.plano;
            ASSERT_FALSE(x.inviavel);
            EXPECT_EQ(x.linhas, pares_esperados(cc->externa, cc->interna));
            const std::size_t partes = 2 * (m - 1) * kMaxNiveisHash;   // limite dos temporários vivos
            const std::size_t limite = (p.build_pages + 1) * bs + partes * (kRunDescriptorBytes + kMetaArquivo) + controle(m);
            std::cout << "  [memoria] hash " << (cc == &c ? "uniforme" : "com skew") << ", m = " << m << ": "
                      << p.partitions << " partições no 1º passe, " << p.levels << " nível(is) previsto(s); pico do heap "
                      << x.pico << " B (limite " << limite << " B)\n";
            EXPECT_LE(x.pico, limite);
        }
    }
}

// Grupo de duplicatas maior que group_pages: o sort-merge para com DoesNotFit (célula inviável),
// sem passar das molduras do grupo.
TEST(MemoriaAuxiliar, SortMergeGrupoGrandeFicaInviavel) {
    Caso c = inscricoes(4000, 2, 13);   // dois valores de chave: grupos de ~2.000 registros
    for (auto& r : c.interna) r[0] = std::int64_t{1};   // a interna repete a chave 1 duas vezes
    c.interna.push_back({std::int64_t{1}, std::int64_t{9}, 0, 0, 0, Value{}});
    std::swap(c.externa, c.interna);   // o grupo grande fica na interna
    Ambiente amb("mem_sm_grupo", c);
    JoinParams jp;
    jp.group_pages = 1;
    const Execucao x = executa(amb, JoinAlgorithm::SortMerge, 8, jp);
    std::cout << "  [memoria] sort-merge com grupo de ~2.000 registros e group_pages = 1: "
              << (x.inviavel ? "DoesNotFit (inviável)" : "executou") << ", pico " << x.pico << " B\n";
    EXPECT_TRUE(x.inviavel);
}

// O pior caso de recursão da bancada: Inscrição (cerca de 1.700 páginas) como build, M = 32, dois
// níveis de particionamento. O pico do heap inclui o diretório do build (dentro de M) e os
// descritores das partições vivas (fora de M, declarados).
TEST(MemoriaAuxiliar, HashInscricaoComoBuildEmM32) {
    Caso c = inscricoes(110000, 20000, 9);
    Ambiente amb("mem_hash_grande", c);
    const Execucao x = executa(amb, JoinAlgorithm::Hash, 32);
    ASSERT_FALSE(x.inviavel);
    EXPECT_EQ(x.linhas, pares_esperados(c.externa, c.interna));
    const std::size_t bs = amb.sm.block_size();
    const std::size_t vivas = 2 * 31 * 2;   // dois níveis, as duas entradas, 31 partições cada
    const std::size_t descritores = vivas * (kRunDescriptorBytes + kMetaArquivo);
    std::cout << "  [memoria] hash, Inscrição como build, m = 32: " << x.plano.partitions << " partições, "
              << x.plano.levels << " níveis; pico do heap " << x.pico << " B; descritores de partição vivos até "
              << descritores << " B (" << (descritores + bs - 1) / bs << " molduras, fora de M)\n";
    EXPECT_GE(x.plano.levels, 2u);
    EXPECT_LE(x.pico, (x.plano.build_pages + 1) * bs + descritores + controle(32));
}

// A tabela de arquivos da camada 4 não cresce com os temporários já apagados: antes, cada um
// deixava a entrada com o caminho completo, e a memória crescia com o total de runs e partições
// criadas desde o início do processo. O teste compara 50 e 500 temporários criados e apagados:
// o que fica vivo e o pico não podem crescer com o número deles. Não há limite absoluto, porque
// as estruturas de uso único e as alocações passageiras dependem da biblioteca: no Linux, a
// libstdc++ aloca o vetor de buckets da tabela de páginas na 1ª inserção e monta cada caminho
// com uma lista de componentes (o 1º CI deste teste mediu 104 B vivos e 1.243 B de pico).
TEST(MemoriaAuxiliar, TabelaDeArquivosReaproveitaTemporariosApagados) {
    StorageManager sm(test_config("mem_tabela_arquivos"));
    const auto s = std::make_shared<const Schema>(make_schema({{"k", FieldType::Int32}}));
    auto res = sm.reserve(2);
    const auto rec = encode_record(*s, {std::int64_t{1}});
    auto ciclo = [&] {   // cria, grava uma página, sela e apaga
        TempFile t = sm.create_temp(s, res);
        t.append(detail::make_view(rec, s.get()));
        t.seal();
    };
    ciclo();   // aquecimento: as estruturas de uso único (tabela de arquivos, tabela de páginas) crescem aqui
    auto mede = [&](int n) {
        contador::Escopo e;
        for (int i = 0; i < n; ++i) ciclo();
        return std::pair<std::size_t, std::size_t>{e.vivos(), e.pico()};
    };
    const auto [vivos50, pico50] = mede(50);
    const auto [vivos500, pico500] = mede(500);
    std::cout << "  [memoria] temporários criados e apagados: 50 deixam " << vivos50 << " B vivos (pico " << pico50
              << " B); 500 deixam " << vivos500 << " B vivos (pico " << pico500 << " B)\n";
    EXPECT_LE(vivos500, vivos50 + 64);   // nada acumula por temporário (8 B cada já dariam 3.600 B)
    EXPECT_LE(pico500, pico50 + 64);     // um temporário vivo de cada vez
}

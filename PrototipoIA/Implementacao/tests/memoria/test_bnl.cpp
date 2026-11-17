// CBD-004 (issue #6): memória auxiliar do Block Nested-Loop medida por alocações reais, com M
// mínimo e registros densos e variáveis. Pela decisão 173, o diretório do chunk (um RecordView por
// registro da externa) ocupa as m − 1 − chunk molduras que o plano reserva para ele. O resto do
// heap durante a junção é controle O(m), que não cresce com os dados: o vetor das páginas do chunk,
// os metadados do buffer pool por moldura usada, o buffer da tupla do ResultSink e as chaves
// VarChar decodificadas em std::string temporárias.
#include "contador.hpp"
#include "helpers.hpp"
#include "layer2/algorithms.hpp"
#include "mini_test.hpp"

#include <iostream>
#include <map>
#include <string>

using namespace cbd;

namespace {

std::uint64_t lcg(std::uint64_t& s) { return s = s * 6364136223846793005ull + 1442695040888963407ull, s >> 33; }

class Linhas final : public RecordStream {
public:
    Linhas(const StorageManager& sm, TableId t, const std::vector<std::vector<Value>>& rows) : sm_(sm), t_(t), rows_(rows) {}
    std::optional<std::span<const std::byte>> next() override {
        if (i_ >= rows_.size()) return std::nullopt;
        buf_ = sm_.encode(t_, rows_[i_++]);
        return std::span<const std::byte>(buf_);
    }
private:
    const StorageManager& sm_;
    TableId t_;
    const std::vector<std::vector<Value>>& rows_;
    std::size_t i_ = 0;
    std::vector<std::byte> buf_;
};

struct Caso {
    const char* nome;
    Schema schema;
    std::vector<std::vector<Value>> externa, interna;
};

// Carrega as duas tabelas (heap) num StorageManager e num Monitor com os mesmos dados: o Monitor
// planeja, e o bnl_join roda direto no StorageManager, com o contador armado só nele.
struct Ambiente {
    Monitor mon;
    StorageManager sm;
    TableId me, mi, se, si;
    Ambiente(const std::string& nome, const Caso& c)
        : mon(test_config(nome + "_mon")), sm(test_config(nome + "_sm")) {
        me = mon.create_table("externa", c.schema, PrimarySpec{});
        mi = mon.create_table("interna", c.schema, PrimarySpec{});
        se = sm.create_table("externa", c.schema, PrimarySpec{});
        si = sm.create_table("interna", c.schema, PrimarySpec{});
        struct S final : RecordStream {
            S(const Monitor& m, TableId t, const std::vector<std::vector<Value>>& r) : m_(m), t_(t), r_(r) {}
            std::optional<std::span<const std::byte>> next() override {
                if (i_ >= r_.size()) return std::nullopt;
                buf_ = m_.encode(t_, r_[i_++]);
                return std::span<const std::byte>(buf_);
            }
            const Monitor& m_; TableId t_; const std::vector<std::vector<Value>>& r_; std::size_t i_ = 0; std::vector<std::byte> buf_;
        };
        S a(mon, me, c.externa), b(mon, mi, c.interna);
        mon.bulk_load(me, a);
        mon.bulk_load(mi, b);
        Linhas x(sm, se, c.externa), y(sm, si, c.interna);
        sm.bulk_load(se, x);
        sm.bulk_load(si, y);
        mon.flush_all();
        sm.flush_all();
    }
    AccessPath scan(Monitor& m, TableId t) {
        for (const auto& p : m.access_paths(t)) if (p.kind == PathKind::Scan) return p;
        throw std::logic_error("sem varredura");
    }
    AccessPath scan(StorageManager& s, TableId t) {
        for (const auto& p : s.access_paths(t)) if (p.kind == PathKind::Scan) return p;
        throw std::logic_error("sem varredura");
    }
};

// Controle O(m) declarado: até 128 B por moldura (cursor do chunk e metadados do pool) mais 1 KB
// fixo (buffer da tupla, nome da fase, três chaves VarChar temporárias).
std::size_t controle(std::size_t m) { return 128 * m + 1024; }

struct Execucao {
    JoinPlan plano;
    std::size_t pico = 0;
    std::uint64_t linhas = 0;
};

Execucao executa(Ambiente& amb, std::size_t m) {
    const JoinInput pe{amb.scan(amb.mon, amb.me), 0, 0}, pi{amb.scan(amb.mon, amb.mi), 0, 1};
    Execucao x;
    x.plano = std::get<JoinPlan>(amb.mon.plan_join(JoinAlgorithm::Bnl, pe, pi, m, JoinParams{}));
    const JoinInput se{amb.scan(amb.sm, amb.se), 0, 0}, si{amb.scan(amb.sm, amb.si), 0, 1};
    amb.sm.flush_all();
    amb.sm.invalidate_all();
    auto res = amb.sm.reserve(m);
    ResultSink sink(ResultShape{}, x.plano.swapped);
    PhaseRecorder ph(amb.sm);
    {
        contador::Escopo e;
        bnl_join(amb.sm, se, si, x.plano, res, sink, ph);
        x.pico = e.pico();
    }
    x.linhas = sink.rows();
    return x;
}

std::uint64_t pares_esperados(const std::vector<std::vector<Value>>& a, const std::vector<std::vector<Value>>& b) {
    std::map<std::string, std::uint64_t> ca, cb;
    auto chave = [](const Value& v) {
        if (auto s = std::get_if<std::string>(&v)) return "s" + *s;
        return "i" + std::to_string(std::get<std::int64_t>(v));
    };
    for (const auto& r : a) ++ca[chave(r[0])];
    for (const auto& r : b) ++cb[chave(r[0])];
    std::uint64_t n = 0;
    for (const auto& [k, x] : ca) n += cb.count(k) ? x * cb[k] : 0;
    return n;
}

}  // namespace

TEST(MemoriaAuxiliar, BnlDiretorioDentroDasMolduras) {
    std::uint64_t seed = 5;
    std::vector<Caso> casos;
    {   // densos: registros de 8 B, cerca de 340 por página, como Turma no extremo
        Caso c{"densos (Int32, Int32)", make_schema({{"k", FieldType::Int32}, {"v", FieldType::Int32}}), {}, {}};
        for (std::int64_t i = 0; i < 6000; ++i) c.externa.push_back({i % 97, i});
        for (std::int64_t i = 0; i < 300; ++i) c.interna.push_back({i % 120, i});
        casos.push_back(std::move(c));
    }
    {   // variáveis: chave VarChar de 1 a 24 caracteres, misturados em todas as páginas
        Caso c{"variáveis (VarChar 1..24, Int32)", make_schema({{"k", FieldType::VarChar, 24}, {"v", FieldType::Int32}}), {}, {}};
        auto texto = [&](std::uint64_t i) { return std::string(1 + lcg(seed) % 24, static_cast<char>('a' + i % 26)); };
        for (std::int64_t i = 0; i < 6000; ++i) c.externa.push_back({texto(i), i});
        for (std::int64_t i = 0; i < 300; ++i) c.interna.push_back({c.externa[(i * 13) % 6000][0], i});
        casos.push_back(std::move(c));
    }
    int n = 0;
    for (const auto& c : casos) {
        Ambiente amb("mem_bnl_" + std::to_string(n++), c);
        const JoinInput pe{amb.scan(amb.mon, amb.me), 0, 0}, pi{amb.scan(amb.mon, amb.mi), 0, 1};
        std::size_t m = 3;   // M mínimo: o menor m em que o diretório do chunk cabe nas molduras
        while (!std::holds_alternative<JoinPlan>(amb.mon.plan_join(JoinAlgorithm::Bnl, pe, pi, m, JoinParams{}))) ++m;
        ASSERT_TRUE(m > 3);
        const auto abaixo = amb.mon.plan_join(JoinAlgorithm::Bnl, pe, pi, m - 1, JoinParams{});
        EXPECT_TRUE(std::get<NotApplicable>(abaixo).kind == NotApplicable::Kind::Inviavel);   // abaixo: controlado
        for (std::size_t mm : {m, m + 4, std::size_t{32}}) {
            const Execucao x = executa(amb, mm);
            const std::size_t bs = amb.sm.block_size();
            const std::size_t diretorio = x.plano.dir_entries * sizeof(RecordView);
            EXPECT_LE(diretorio, x.plano.dir_frames * bs);
            EXPECT_EQ(x.linhas, pares_esperados(c.externa, c.interna));
            const std::size_t resto = x.pico > diretorio ? x.pico - diretorio : 0;
            std::cout << "  [memoria] BNL " << c.nome << ", m = " << mm << ": chunk " << x.plano.chunk << ", diretório "
                      << diretorio << " B em " << x.plano.dir_frames << " molduras (" << x.plano.dir_frames * bs
                      << " B); pico do heap " << x.pico << " B, dos quais " << resto << " B de controle (limite "
                      << controle(mm) << " B)\n";
            EXPECT_LE(x.pico, diretorio + controle(mm));
        }
    }
}

// O controle fora do diretório não cresce com os dados: dobrar a externa, com o mesmo m, não muda
// o heap além do diretório. Ele inclui o índice da política de substituição do subpool, com uma
// entrada por moldura ocupada; por isso os dois tamanhos passam de m páginas e ocupam as m molduras.
TEST(MemoriaAuxiliar, BnlControleNaoCresceComOsDados) {
    std::vector<std::size_t> resto;
    for (std::int64_t linhas : {12000, 24000}) {
        Caso c{"densos", make_schema({{"k", FieldType::Int32}, {"v", FieldType::Int32}}), {}, {}};
        for (std::int64_t i = 0; i < linhas; ++i) c.externa.push_back({i % 97, i});
        for (std::int64_t i = 0; i < 300; ++i) c.interna.push_back({i % 120, i});
        Ambiente amb("mem_bnl_dobro_" + std::to_string(linhas), c);
        const Execucao x = executa(amb, 32);
        EXPECT_EQ(x.linhas, pares_esperados(c.externa, c.interna));
        resto.push_back(x.pico - x.plano.dir_entries * sizeof(RecordView));
    }
    std::cout << "  [memoria] BNL controle com 12.000 e 24.000 registros, m = 32: " << resto[0] << " B e " << resto[1] << " B\n";
    EXPECT_LE(resto[1], resto[0] + 256);
}

// Registros variáveis com páginas desiguais: o plano estima registros por página pela média
// (n / b), e uma página acima da média enchia o diretório; antes, o BNL parava com DoesNotFit
// mesmo com o plano dizendo que cabia. Agora a rodada fecha quando o diretório enche, e o resto
// da página fica para a seguinte: o resultado sai certo e o heap continua dentro do limite.
TEST(MemoriaAuxiliar, BnlPaginasDesiguaisNaoPassamDoDiretorio) {
    Caso c{"páginas desiguais", make_schema({{"k", FieldType::VarChar, 60}, {"v", FieldType::Int32}}), {}, {}};
    for (std::int64_t i = 0; i < 3000; ++i) c.externa.push_back({std::string(1, static_cast<char>('a' + i % 26)), i});
    for (std::int64_t i = 0; i < 1500; ++i) c.externa.push_back({std::string(60, static_cast<char>('a' + i % 26)), i});
    for (std::int64_t i = 0; i < 52; ++i) c.interna.push_back({std::string(1, static_cast<char>('a' + i % 26)), i});
    Ambiente amb("mem_bnl_desiguais", c);
    const JoinInput pe{amb.scan(amb.mon, amb.me), 0, 0}, pi{amb.scan(amb.mon, amb.mi), 0, 1};
    std::size_t m = 3;
    while (!std::holds_alternative<JoinPlan>(amb.mon.plan_join(JoinAlgorithm::Bnl, pe, pi, m, JoinParams{}))) ++m;
    for (std::size_t mm : {m, m + 1, std::size_t{32}}) {
        const Execucao x = executa(amb, mm);
        const std::size_t diretorio = x.plano.dir_entries * sizeof(RecordView);
        std::cout << "  [memoria] BNL páginas desiguais, m = " << mm << ": chunk " << x.plano.chunk << ", pico "
                  << x.pico << " B (diretório " << diretorio << " B)\n";
        EXPECT_EQ(x.linhas, pares_esperados(c.externa, c.interna));
        EXPECT_LE(x.pico, diretorio + controle(mm));
    }
}

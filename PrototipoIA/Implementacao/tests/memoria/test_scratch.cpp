// CBD-003 (issue #5): memória auxiliar do ScratchSpace medida por alocações reais, não pela
// capacidade dos vetores. O orçamento é o da decisão 173: os registros ocupam as páginas da
// reserva, e o diretório (slots, cadeias e cabeças do hash) é cobrado delas. Então, em qualquer
// momento, bytes dos registros + heap do operador ≤ páginas × bloco.
//
// Na auditoria (E03, commit b61b87b), 2 páginas de registros de um Int32 guardavam 1.364
// registros e o diretório chegava a 54.560 B de heap, fora das 8.192 B da reserva.
#include "contador.hpp"
#include "helpers.hpp"
#include "layer3/storage_manager.hpp"
#include "mini_test.hpp"

#include <iostream>
#include <string>

using namespace cbd;

namespace {

std::uint64_t lcg(std::uint64_t& s) { return s = s * 6364136223846793005ull + 1442695040888963407ull, s >> 33; }

struct Medida {
    std::size_t registros = 0, bytes_registros = 0, pico = 0, orcamento = 0;
};

void relata(const std::string& caso, const Medida& m) {
    std::cout << "  [memoria] " << caso << ": " << m.registros << " registros, " << m.bytes_registros
              << " B de registros + pico de " << m.pico << " B de heap; orçamento " << m.orcamento << " B\n";
}

// Enche o espaço, ordena, monta o hash e sonda. O contador fica armado só nas chamadas do operador;
// a entrada (registros codificados, chaves da sonda) e as conferências do teste ficam fora.
Medida exercita(StorageManager& sm, const Schema& s, std::size_t pages,
                const std::function<std::vector<Value>(std::uint64_t&)>& linha) {
    auto r = sm.reserve(pages);
    ScratchSpace sc = sm.create_scratch(s, r, pages);
    std::vector<std::vector<std::byte>> lote;
    std::uint64_t seed = 11;
    for (std::size_t i = 0; i < 20000; ++i) lote.push_back(encode_record(s, linha(seed)));
    std::vector<Key> sondas;
    for (std::size_t i = 0; i < 200; ++i) sondas.push_back(detail::make_view(lote[i * 7], &s).key(0));
    Medida m;
    std::size_t achados = 0;
    {
        contador::Escopo e;
        for (const auto& bytes : lote)
            if (!sc.add(detail::make_view(bytes, &s))) break;
        sc.sort_by(0);
        sc.build_hash(0, 3);
        for (const Key& k : sondas)
            for (auto it = sc.probe(k); it.next();) ++achados;
        m.pico = e.pico();
    }
    for (std::size_t i = 1; i < sc.size(); ++i) EXPECT_TRUE(compare_keys(sc.at(i - 1).key(0), sc.at(i).key(0)) <= 0);
    std::size_t esperado = 0;   // oráculo da sonda: varredura do espaço
    for (const Key& k : sondas)
        for (std::size_t i = 0; i < sc.size(); ++i) esperado += compare_keys(sc.at(i).key(0), k) == 0 ? 1 : 0;
    EXPECT_EQ(achados, esperado);
    m.registros = sc.size();
    m.bytes_registros = sc.record_bytes();
    m.orcamento = pages * sm.block_size();
    return m;
}

// Comparar e sondar chaves VarChar decodifica cada chave numa std::string temporária: no máximo
// três vivas ao mesmo tempo (as duas comparadas e a cópia guardada pela sonda). É overhead O(1),
// declarado na decisão 173; chaves numéricas não alocam.
std::size_t folga_de_chaves(const Schema& s) {
    const FieldDef& f = s.field(0);
    return f.type == FieldType::VarChar ? 3 * (static_cast<std::size_t>(f.size) + 1 + 32) : 0;
}

}  // namespace

TEST(MemoriaAuxiliar, ScratchSpaceDentroDasPaginas) {
    StorageManager sm(test_config("mem_scratch"));
    const Schema so_int = make_schema({{"k", FieldType::Int32}});
    const Schema largo = make_schema({{"k", FieldType::Int32}, {"pad", FieldType::Char, 200}});
    const Schema texto = make_schema({{"nome", FieldType::VarChar, 24}, {"v", FieldType::Int32}});
    auto inteiro = [](std::uint64_t& s) { return std::vector<Value>{std::int64_t(lcg(s) % 1000)}; };
    auto com_pad = [](std::uint64_t& s) { return std::vector<Value>{std::int64_t(lcg(s) % 1000), std::string("x")}; };
    auto nome = [](std::uint64_t& s) {   // chaves de 1 a 24 caracteres: parte delas passa do SSO
        const std::size_t n = 1 + lcg(s) % 24;
        return std::vector<Value>{std::string(n, static_cast<char>('a' + lcg(s) % 26)), std::int64_t(lcg(s) % 50)};
    };
    struct Caso { const char* nome; const Schema* s; std::size_t pages; std::function<std::vector<Value>(std::uint64_t&)> f; };
    const std::vector<Caso> casos{{"E03: Int32, 2 páginas", &so_int, 2, inteiro},
                                  {"Int32, 1 página", &so_int, 1, inteiro},
                                  {"registro de 204 B, 2 páginas", &largo, 2, com_pad},
                                  {"VarChar como chave, 3 páginas", &texto, 3, nome}};
    for (const auto& c : casos) {
        const Medida m = exercita(sm, *c.s, c.pages, c.f);
        relata(c.nome, m);
        EXPECT_GT(m.registros, 0u);
        EXPECT_LE(m.bytes_registros + m.pico, m.orcamento + folga_de_chaves(*c.s));
    }
}

TEST(MemoriaAuxiliar, OrdenarRidsDentroDaPagina) {
    StorageManager sm(test_config("mem_rids"));
    auto r = sm.reserve(1);
    ScratchSpace sc = sm.create_rid_scratch(r, 1);
    std::uint64_t seed = 3;
    std::size_t pico = 0;
    {
        contador::Escopo e;
        while (sc.add(RID{static_cast<std::uint32_t>(1 + lcg(seed) % 500), static_cast<std::uint16_t>(lcg(seed) % 30)})) {}
        sc.sort_rids();
        pico = e.pico();
    }
    for (std::size_t i = 1; i < sc.size(); ++i) {
        const RID a = sc.rid_at(i - 1), b = sc.rid_at(i);
        EXPECT_TRUE(a.block < b.block || (a.block == b.block && a.slot <= b.slot));
    }
    relata("RIDs, 1 página", Medida{sc.size(), sc.record_bytes(), pico, sm.block_size()});
    EXPECT_LE(sc.record_bytes() + pico, sm.block_size());
}

// O próprio contador. Usa ::operator new direto: o clang pode suprimir a alocação de um vetor que
// não é usado, mas não uma chamada explícita à função de alocação.
TEST(MemoriaAuxiliar, ContadorSoContaOEscopo) {
    void* antes = ::operator new(1000);   // alocado antes do escopo: liberar não desconta
    contador::Escopo e;
    ::operator delete(antes);
    EXPECT_EQ(e.vivos(), 0u);
    void* dentro = ::operator new(400);
    EXPECT_EQ(e.vivos(), 400u);
    EXPECT_EQ(e.pico(), 400u);
    ::operator delete(dentro);
    EXPECT_EQ(e.vivos(), 0u);
    EXPECT_EQ(e.pico(), 400u);
}

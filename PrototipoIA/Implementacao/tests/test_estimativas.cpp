// BENCH-008 (issue #17): os estimadores com valores calculados à mão. Cada caso monta o plano e as
// estatísticas direto, sem tabela, e confere a fórmula do livro com os parâmetros da bancada:
// b para ler a estrutura, d = ⌈n / bfr_medio⌉ (blocos densos) para gravar e reler runs e partições.
#include "layer2/algorithms.hpp"
#include "mini_test.hpp"

#include <cmath>

using namespace cbd;

namespace {

TableStats tabela(std::uint64_t n, std::uint32_t b, std::uint32_t bfr, std::uint32_t bfr_medio = 0) {
    TableStats t;
    t.n = n;
    t.b = b;
    t.bfr = bfr;
    t.bfr_medio = bfr_medio;
    return t;
}

bool perto(double a, double b) { return std::fabs(a - b) < 1e-9; }

}  // namespace

TEST(Estimativas, BlocosDensosUsamABlocagemMedia) {
    EXPECT_EQ(dense_blocks(tabela(2000, 130, 20)), 100u);       // ⌈2000 / 20⌉
    EXPECT_EQ(dense_blocks(tabela(2000, 130, 20, 25)), 80u);    // VarChar: média de 25 por bloco
}

TEST(Estimativas, Bnl) {
    JoinPlan p;
    p.alg = JoinAlgorithm::Bnl;
    p.m = 32;
    p.chunk = 16;
    // b_r + ⌈100 / 16⌉ · b_s = 100 + 7 · 40 = 380; seeks = 2 · 7
    const Cost c = estimate_join(p, tabela(2000, 100, 20), tabela(800, 40, 20));
    EXPECT_TRUE(perto(c.transfers, 380));
    EXPECT_TRUE(perto(c.seeks, 14));
    // O diretório limita a rodada a 100 registros: 2000 / 100 = 20 rodadas,
    // mesmo que o chunk nominal precisasse de apenas 7. Custo = 100 + 20 * 40 = 900.
    p.dir_entries = 100;
    const Cost limited = estimate_join(p, tabela(2000, 100, 20), tabela(800, 40, 20));
    EXPECT_TRUE(perto(limited.transfers, 900));
    EXPECT_TRUE(perto(limited.seeks, 40));
}

TEST(Estimativas, SortMergeComUmPasseEEntradaOrdenada) {
    JoinPlan p;
    p.alg = JoinAlgorithm::SortMerge;
    p.m = 32;
    p.sort_outer = true;
    p.sort_inner = false;   // interna já ordenada pela estrutura (164)
    p.runs_outer = 10;
    p.merge_passes_outer = 1;
    // externa: b + d (grava runs) + 2 · d · passes + d (lê no final) = 100 + 100 + 200 + 100 = 500
    // interna: lida em ordem uma vez = 40. Total 540.
    // seeks: 2 · runs + 2 · d · passes = 20 + 200; interna 1; leitura final d_ext + b_int = 100 + 40
    const Cost c = estimate_join(p, tabela(2000, 100, 20), tabela(800, 40, 20));
    EXPECT_TRUE(perto(c.transfers, 540));
    EXPECT_TRUE(perto(c.seeks, 361));
    // com blocagem média de 25 (VarChar), d_ext = 80: 100 + 80 + 160 + 80 + 40 = 460 (P08)
    const Cost v = estimate_join(p, tabela(2000, 100, 20, 25), tabela(800, 40, 20));
    EXPECT_TRUE(perto(v.transfers, 460));
}

TEST(Estimativas, HashSemPartesComUmEComDoisNiveis) {
    JoinPlan p;
    p.alg = JoinAlgorithm::Hash;
    p.m = 32;
    p.partitions = 1;   // o build cabe: b_r + b_s
    EXPECT_TRUE(perto(estimate_join(p, tabela(2000, 100, 20), tabela(800, 40, 20)).transfers, 140));
    p.partitions = 31;
    p.levels = 1;
    // (b_r + 2 · d_r) + (b_s + 2 · d_s) + 4 · n_h, n_h = 31: 300 + 120 + 124 = 544
    const Cost um = estimate_join(p, tabela(2000, 100, 20), tabela(800, 40, 20));
    EXPECT_TRUE(perto(um.transfers, 544));
    EXPECT_TRUE(perto(um.seeks, 2 * 140 + 2 * 31));
    p.levels = 2;
    // dois passes: (100 + 4 · 100) + (40 + 4 · 40) + 4 · 31 · 31 = 500 + 200 + 3844 = 4544. O termo
    // 4 · n_h (blocos parcialmente cheios) cresce com as 961 subpartições: é o desvio de +29% no hash
    // com Inscrição como build (README, §4.5).
    EXPECT_TRUE(perto(estimate_join(p, tabela(2000, 100, 20), tabela(800, 40, 20)).transfers, 4544));
    p.levels = 1;
    p.prepartitioned_inner = true;   // a interna pré-particionada pula o 1º passe: 300 + 40 + 124
    EXPECT_TRUE(perto(estimate_join(p, tabela(2000, 100, 20), tabela(800, 40, 20)).transfers, 464));
}

TEST(Estimativas, SelecaoPelaBPrimaria) {
    TableStats t = tabela(20000, 952, 21);
    StructureStats bt;
    bt.kind = StructureKind::BTree;
    bt.height = 3;
    t.structures.push_back(bt);
    SelectPlan p;
    p.path = PathKind::PrimaryBTree;
    p.batch = 1000;
    p.expected_rows = 1000;
    p.unique_field = true;
    // igualdade em UNIQUE: h + 1 por chave, com h = altura − 1 níveis internos: 1000 · (2 + 1)
    Cost c = estimate_select(p, t, &t.structures.front());
    EXPECT_TRUE(perto(c.transfers, 3000));
    EXPECT_TRUE(perto(c.seeks, 3000));
    // faixa de 1.000 linhas em 20.000: fração 0,05 das 952 folhas = ⌈47,6⌉ = 48, mais 2 níveis = 50
    p.range = true;
    p.batch = 1;
    p.unique_field = false;
    c = estimate_select(p, t, &t.structures.front());
    EXPECT_TRUE(perto(c.transfers, 50));
    EXPECT_TRUE(perto(c.seeks, 3));
}

TEST(Estimativas, ClusteringSobreHash) {
    TableStats t = tabela(20000, 300, 70);
    StructureStats hash, idx;
    hash.kind = StructureKind::Hash;
    hash.chain_avg = 1.5;
    idx.kind = StructureKind::SecBTree;
    idx.height = 2;
    idx.distinct = 200;
    t.structures = {hash, idx};
    SelectPlan p;
    p.path = PathKind::SecondaryBTreeClustering;
    p.range = true;
    p.expected_rows = 10000;   // 50%
    // 0,5 · 200 = 100 valores, cada um lê a cadeia do seu bucket (1,5): 150, mais 1 nível interno = 151.
    // Valores que caem no mesmo bucket releem a cadeia, o que a fórmula não modela (−44%, §4.5).
    const Cost c = estimate_select(p, t, &t.structures[1]);
    EXPECT_TRUE(perto(c.transfers, 151));
    EXPECT_TRUE(perto(c.seeks, 2));
}

TEST(Estimativas, HashesEBMaisSecundaria) {
    TableStats t = tabela(20000, 950, 21);
    StructureStats s;
    s.chain_avg = 1.2;
    s.height = 3;
    SelectPlan p;
    p.batch = 1000;
    p.expected_rows = 1000;
    p.path = PathKind::PrimaryHash;   // a cadeia por chave: 1000 · 1,2
    EXPECT_TRUE(perto(estimate_select(p, t, &s).transfers, 1200));
    p.path = PathKind::SecondaryHash;   // cadeia do índice + 1 bloco de dados: 1000 · 2,2
    EXPECT_TRUE(perto(estimate_select(p, t, &s).transfers, 2200));
    p.path = PathKind::SecondaryBTreeNonClustering;   // faixa de 400 linhas: 2 níveis + folha + 400 RIDs
    p.range = true;
    p.batch = 1;
    p.expected_rows = 400;
    EXPECT_TRUE(perto(estimate_select(p, t, &s).transfers, 403));
}

TEST(Estimativas, Varredura) {
    const TableStats t = tabela(20000, 650, 31);
    SelectPlan p;
    p.path = PathKind::Scan;
    p.batch = 1000;
    p.unique_field = true;   // para no 1º registro: metade do arquivo em média, por chave
    EXPECT_TRUE(perto(estimate_select(p, t, nullptr).transfers, 1000 * 325.0));
    p.unique_field = false;
    EXPECT_TRUE(perto(estimate_select(p, t, nullptr).transfers, 1000 * 650.0));
    p.range = true;   // uma passada
    EXPECT_TRUE(perto(estimate_select(p, t, nullptr).transfers, 650));
}

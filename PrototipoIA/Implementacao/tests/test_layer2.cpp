// Testes da camada 2: junções, seleções, inaplicáveis, políticas, orçamento e partições (§10, 151, 152, 164).
#include "helpers.hpp"
#include "layer2/monitor.hpp"
#include "layer3/page.hpp"
#include "mini_test.hpp"

#include <functional>
#include <iostream>
#include <map>

using namespace cbd;

namespace {

class RowsStream final : public RecordStream {
public:
    RowsStream(Monitor& m, TableId t, std::vector<std::vector<Value>> rows) : m_(m), t_(t), rows_(std::move(rows)) {}
    std::optional<std::span<const std::byte>> next() override {
        if (i_ >= rows_.size()) return std::nullopt;
        buf_ = m_.encode(t_, rows_[i_++]);
        return std::span<const std::byte>(buf_);
    }
private:
    Monitor& m_;
    TableId t_;
    std::vector<std::vector<Value>> rows_;
    std::size_t i_ = 0;
    std::vector<std::byte> buf_;
};

// A(id único, grp, pad) e B(aid, val, pad): 4 linhas de B por id de A; C(g com 2 valores) para duplicatas.
struct Db {
    std::map<std::string, TableId> t;
};

Db build(Monitor& m) {
    Db db;
    const Schema sa = make_schema({{"id", FieldType::Int32, 0, true}, {"grp", FieldType::Int32}, {"pad", FieldType::Char, 60}});
    const Schema sb = make_schema({{"aid", FieldType::Int32}, {"val", FieldType::Int32}, {"pad", FieldType::Char, 60}});
    std::vector<std::vector<Value>> ra, rb, rc;
    for (std::int64_t i = 0; i < 300; ++i) ra.push_back({(i * 37) % 300, i % 7, std::string("a")});
    for (std::int64_t i = 0; i < 1200; ++i) rb.push_back({(i * 53) % 300, i, std::string("b")});
    for (std::int64_t i = 0; i < 1200; ++i) rc.push_back({i % 2, i, std::string("c")});
    auto sorted = [](std::vector<std::vector<Value>> v) {
        std::stable_sort(v.begin(), v.end(), [](const auto& x, const auto& y) {
            return std::get<std::int64_t>(x[0]) < std::get<std::int64_t>(y[0]);
        });
        return v;
    };
    auto add = [&](const std::string& name, const Schema& s, PrimarySpec p, std::vector<std::vector<Value>> rows) {
        const TableId t = m.create_table(name, s, p);
        RowsStream rs(m, t, std::move(rows));
        m.bulk_load(t, rs);
        db.t[name] = t;
    };
    add("a_heap", sa, PrimarySpec{}, ra);
    add("a_bt", sa, PrimarySpec{StructureKind::BTree, 0, 0.7}, sorted(ra));
    add("a_hash", sa, PrimarySpec{StructureKind::Hash, 0, 0.7, m.suggest_buckets(sa, 300, 300, 0.8)}, ra);
    add("b_heap", sb, PrimarySpec{}, rb);
    add("b_bt", sb, PrimarySpec{StructureKind::BTree, 0, 0.7}, sorted(rb));
    add("b_hash", sb, PrimarySpec{StructureKind::Hash, 0, 0.7, m.suggest_buckets(sb, 1200, 300, 0.8)}, rb);
    add("c_heap", sb, PrimarySpec{}, rc);
    m.create_index(db.t["b_heap"], IndexSpec{StructureKind::SecBTree, 0, false});
    m.flush_all();
    return db;
}

AccessPath path(Monitor& m, TableId t, PathKind k) {
    for (const auto& p : m.access_paths(t))
        if (p.kind == k) return p;
    throw std::logic_error("caminho ausente");
}

OpResult ok(const Outcome& o) {
    if (auto na = std::get_if<NotApplicable>(&o)) throw std::runtime_error("inaplicável inesperado: " + na->reason);
    return std::get<OpResult>(o);
}

Outcome join(Monitor& m, Db& db, JoinAlgorithm alg, const std::string& a, PathKind ka, const std::string& b, PathKind kb,
             std::size_t mm, bool a_outer = true, JoinParams jp = {}) {
    JoinInput ia{path(m, db.t[a], ka), 0, 0}, ib{path(m, db.t[b], kb), 0, 1};
    m.flush_all();
    m.invalidate_all();
    return a_outer ? m.join(alg, ia, ib, mm, ResultShape{}, jp) : m.join(alg, ib, ia, mm, ResultShape{}, jp);
}

}  // namespace

TEST(Monitor, TresAlgoritmosConcordamEmTodasAsOrganizacoes) {
    Monitor m(test_config("mon_concordam"));
    Db db = build(m);
    std::optional<std::uint64_t> sum;
    const std::vector<std::pair<std::string, PathKind>> as{{"a_heap", PathKind::Scan}, {"a_bt", PathKind::PrimaryBTree}, {"a_hash", PathKind::PrimaryHash}};
    const std::vector<std::pair<std::string, PathKind>> bs{{"b_heap", PathKind::Scan}, {"b_bt", PathKind::PrimaryBTree}, {"b_hash", PathKind::PrimaryHash}};
    for (const auto& [a, ka] : as)
        for (const auto& [b, kb] : bs)
            for (auto alg : {JoinAlgorithm::Bnl, JoinAlgorithm::SortMerge, JoinAlgorithm::Hash})
                for (std::size_t mm : {std::size_t{6}, std::size_t{32}}) {
                    const OpResult r = ok(join(m, db, alg, a, ka, b, kb, mm));
                    EXPECT_EQ(r.rows, 1200u);
                    if (!sum) sum = r.checksum;
                    EXPECT_EQ(r.checksum, *sum);
                    EXPECT_EQ(r.flush_writes, 0u);
                    EXPECT_EQ(r.delta.global.requests, r.delta.global.hits + r.delta.global.misses);
                    EXPECT_EQ(r.delta.global.data_area_requests, 0u);
                }
}

TEST(Monitor, PapeisTrocadosNaoMudamOChecksum) {   // (152)
    Monitor m(test_config("mon_papeis"));
    Db db = build(m);
    for (auto alg : {JoinAlgorithm::Bnl, JoinAlgorithm::SortMerge, JoinAlgorithm::Hash}) {
        const OpResult x = ok(join(m, db, alg, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 16, true));
        const OpResult y = ok(join(m, db, alg, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 16, false));
        EXPECT_EQ(x.checksum, y.checksum);
        EXPECT_EQ(x.rows, y.rows);
    }
}

// P03 da auditoria de 10/11: com registros pequenos (como Turma, 145 por página), o diretório
// do BNL com chunk de m/2 páginas guardava cerca de 56 B por registro fora das molduras. Agora
// ele cabe nas m − 1 − chunk molduras restantes, e o chunk diminui quando não cabe (decisão 173).
TEST(Monitor, DiretorioDoBnlCabeEmM) {
    Monitor m(test_config("mon_bnl_dir"));
    const Schema s = make_schema({{"k", FieldType::Int32}, {"v", FieldType::Int32}});
    std::vector<std::vector<Value>> rows;
    for (std::int64_t i = 0; i < 40000; ++i) rows.push_back({i % 500, i});
    const TableId t = m.create_table("pequenos", s, PrimarySpec{});
    RowsStream rs(m, t, rows);
    m.bulk_load(t, rs);
    const TableStats st = m.stats(t);
    ASSERT_TRUE(st.b + 2 > 32);   // não cabe em M: o BNL usa chunk
    const JoinInput in0{path(m, t, PathKind::Scan), 0, 0}, in1{path(m, t, PathKind::Scan), 0, 1};
    const auto planned = m.plan_join(JoinAlgorithm::Bnl, in0, in1, 32, JoinParams{});
    const JoinPlan& p = std::get<JoinPlan>(planned);
    const std::size_t por_pagina = (st.n + st.b - 1) / st.b;
    EXPECT_LE(p.frames_used(), 32u);
    EXPECT_LE(p.chunk * por_pagina, p.dir_entries);
    EXPECT_LE(p.dir_entries * sizeof(RecordView), p.dir_frames * 4096);
    EXPECT_LT(p.chunk, 16u);                  // m/2 não deixaria espaço para o diretório
    m.flush_all();
    m.invalidate_all();
    const OpResult r = ok(m.join(JoinAlgorithm::Bnl, in0, in1, 32, ResultShape{}));
    EXPECT_EQ(r.rows, 40000u * 80u);          // 500 grupos de 80 registros: 80 × 80 pares por grupo
}

// CBD-004: as molduras do diretório do BNL são retidas durante a junção, e o subpool não as usa
// como cache. M = 8, chunk = 2 (externa de 4 páginas: 2 rodadas), diretório em 1 moldura: sobram 5
// molduras para as 6 páginas da interna. Com MRU, a 1ª rodada lê as 6 (B6 expulsa B5); na 2ª, A3 e
// A4 expulsam A2 e A1, e só B5 é relida: 4 + 6 + 1 = 11. Antes, com o diretório só cobrado no plano,
// as 6 páginas cabiam nas 8 − 2 molduras, e a 2ª rodada não lia nada (10).
TEST(Monitor, BnlRetemAsMoldurasDoDiretorio) {
    Monitor m(test_config("mon_bnl_retem", Policy::Mru));
    const Schema s = make_schema({{"k", FieldType::Int32}, {"pad", FieldType::Char, 100}});
    const std::size_t por_pagina = sp_capacity(4096, s.record_size);
    auto add = [&](const std::string& name, std::size_t pages) {
        std::vector<std::vector<Value>> rows;
        for (std::size_t i = 0; i < pages * por_pagina; ++i) rows.push_back({std::int64_t(i % 50), std::string("p")});
        const TableId t = m.create_table(name, s, PrimarySpec{});
        RowsStream rs(m, t, rows);
        m.bulk_load(t, rs);
        return t;
    };
    const TableId ext = add("externa", 4), inn = add("interna", 6);
    ASSERT_EQ(m.stats(ext).b, 4u);
    ASSERT_EQ(m.stats(inn).b, 6u);
    const JoinInput o{path(m, ext, PathKind::Scan), 0, 0}, i{path(m, inn, PathKind::Scan), 0, 1};
    JoinParams jp;
    jp.chunk = 2;
    const JoinPlan p = std::get<JoinPlan>(m.plan_join(JoinAlgorithm::Bnl, o, i, 8, jp));
    ASSERT_EQ(p.chunk, 2u);
    ASSERT_EQ(p.dir_frames, 1u);
    m.flush_all();
    m.invalidate_all();
    const OpResult r = ok(m.join(JoinAlgorithm::Bnl, o, i, 8, ResultShape{}, jp));
    EXPECT_EQ(r.delta.global.reads(), 4u + 6u + 1u);
}

TEST(Monitor, BnlDividePaginaDensaComTresMolduras) {
    for (const Policy policy : {Policy::Lru, Policy::Mru}) {
        Monitor m(test_config(policy == Policy::Lru ? "mon_bnl_denso_lru" : "mon_bnl_denso_mru", policy));
        const Schema schema = make_schema({{"k", FieldType::Int32, 0, true}});
        const auto a = m.create_table("a", schema, {}), b = m.create_table("b", schema, {});
        std::vector<std::vector<Value>> rows;
        for (std::int64_t k = 0; k < 400; ++k) rows.push_back({k});
        RowsStream ra(m, a, rows), rb(m, b, rows);
        m.bulk_load(a, ra);
        m.bulk_load(b, rb);
        const JoinInput outer{path(m, a, PathKind::Scan), 0, 0}, inner{path(m, b, PathKind::Scan), 0, 1};
        const auto planned = m.plan_join(JoinAlgorithm::Bnl, outer, inner, 3, {});
        ASSERT_TRUE(std::holds_alternative<JoinPlan>(planned));
        EXPECT_LE(std::get<JoinPlan>(planned).frames_used(), 3u);
        m.flush_all();
        m.invalidate_all();
        const auto result = ok(m.join(JoinAlgorithm::Bnl, outer, inner, 3, {}));
        EXPECT_EQ(result.rows, 400u);
        // 400 registros cabem numa página, mas o diretório de 1 moldura exige três rodadas.
        EXPECT_EQ(result.delta.global.requests, 4u);   // uma externa + três varreduras de uma página
        EXPECT_EQ(result.delta.global.reads(), 2u);    // a página interna fica em cache entre rodadas
        EXPECT_EQ(result.flush_writes, 0u);
    }
}

TEST(Monitor, BnlDiretorioExatamenteCheioNaoRevarreInternaVazia) {
    Monitor m(test_config("mon_bnl_limite"));
    const Schema schema = make_schema({{"k", FieldType::Int32, 0, true}});
    for (const std::size_t count : {169, 170, 171}) {
        const auto a = m.create_table("a" + std::to_string(count), schema, {});
        const auto b = m.create_table("b" + std::to_string(count), schema, {});
        std::vector<std::vector<Value>> outer_rows, inner_rows;
        for (std::size_t k = 0; k < count; ++k) outer_rows.push_back({static_cast<std::int64_t>(k)});
        for (std::int64_t k = 0; k < 600; ++k) inner_rows.push_back({k});
        RowsStream ra(m, a, outer_rows), rb(m, b, inner_rows);
        m.bulk_load(a, ra);
        m.bulk_load(b, rb);
        const JoinInput outer{path(m, a, PathKind::Scan), 0, 0}, inner{path(m, b, PathKind::Scan), 0, 1};
        m.flush_all();
        m.invalidate_all();
        const auto result = ok(m.join(JoinAlgorithm::Bnl, outer, inner, 3, {}));
        EXPECT_EQ(result.rows, count);
        // ABI desta bancada: diretório de 4096 B comporta 170 RecordViews de 24 B.
        // A externa tem uma página e a interna duas; 171 linhas externas exigem duas rodadas.
        EXPECT_EQ(result.delta.global.requests, count <= 170 ? 3u : 5u);
        EXPECT_EQ(result.estimate.transfers, count <= 170 ? 3.0 : 5.0);
        std::cout << "  [bnl] externa=" << count << " pares=" << result.rows
                  << " requisicoes=" << result.delta.global.requests << " leituras=" << result.delta.global.reads()
                  << " gravacoes=" << result.delta.global.writes() << '\n';
    }
}

// P12 da auditoria de 10/11: com M pequeno, o sort-merge grava runs e o hash grava partições, e
// páginas sujas de temporários são expulsas durante a operação. Toda essa E/S precisa cair dentro
// do delta medido: o delta é a soma das fases, nada fica sujo depois (flush final grava 0), e as
// expulsões sujas e as gravações de temporários estão contadas nas gravações do operador.
// M = 5: com a lista de runs cobrada em M (CBD-005), o sort-merge com grupo de duplicatas precisa de
// uma moldura para a lista, duas para o grupo e duas para a intercalação final.
TEST(Monitor, EsDosTemporariosFicaDentroDaMedicao) {
    Monitor m(test_config("mon_fronteira"));
    Db db = build(m);
    std::uint64_t expulsoes = 0;
    for (auto alg : {JoinAlgorithm::SortMerge, JoinAlgorithm::Hash}) {
        const OpResult r = ok(join(m, db, alg, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 5));
        expulsoes += r.delta.global.dirty_evictions;
        EXPECT_EQ(r.rows, 1200u);
        const auto& g = r.delta.global;
        const auto& tmp = r.delta.per_kind[static_cast<std::size_t>(FileKind::Temp)];
        std::uint64_t fases_l = 0, fases_g = 0;
        for (const auto& ph : r.phases) {
            fases_l += ph.delta.global.reads();
            fases_g += ph.delta.global.writes();
        }
        EXPECT_GT(tmp.writes(), 0u);                    // houve runs ou partições gravadas
        EXPECT_EQ(g.writes(), tmp.writes());            // só temporários são gravados numa junção
        EXPECT_LE(g.dirty_evictions, g.writes());       // expulsões sujas já estão nas gravações
        EXPECT_EQ(fases_l, g.reads());
        EXPECT_EQ(fases_g, g.writes());
        EXPECT_EQ(r.flush_writes, 0u);
        EXPECT_EQ(m.flush_all(), 0u);                   // nada do operador ficou para depois
    }
    EXPECT_GT(expulsoes, 0u);                           // o cenário força expulsões de páginas sujas
}

namespace {
// tabelas de páginas cheias com registros de 204 B: 19 por página (sp_capacity(4096, 204))
TableId tabela_paginas(Monitor& m, const std::string& name, const Schema& s, std::size_t pages,
                       const std::function<std::int64_t(std::size_t)>& key) {
    std::vector<std::vector<Value>> rows;
    for (std::size_t i = 0; i < pages * sp_capacity(4096, s.record_size); ++i) rows.push_back({key(i), std::string("p")});
    const TableId t = m.create_table(name, s, PrimarySpec{});
    RowsStream rs(m, t, rows);
    m.bulk_load(t, rs);
    return t;
}
std::uint64_t fase(const OpResult& r, const std::string& nome, bool escritas) {
    for (const auto& ph : r.phases)
        if (ph.name == nome) return escritas ? ph.delta.global.writes() : ph.delta.global.reads();
    return 0;
}
}  // namespace

// CBD-010 (issue #9): sequências pequenas, a frio, com a E/S calculada à mão.
TEST(Monitor, SequenciaManualDeEs) {
    Monitor m(test_config("mon_manual"));
    const Schema s = make_schema({{"k", FieldType::Int32}, {"pad", FieldType::Char, 200}});
    const Schema su = make_schema({{"k", FieldType::Int32, 0, true}, {"pad", FieldType::Char, 200}});
    const std::size_t pp = sp_capacity(4096, s.record_size);
    ASSERT_EQ(pp, 19u);
    const TableId a = tabela_paginas(m, "a", s, 3, [](std::size_t i) { return std::int64_t(i % 30); });
    const TableId b = tabela_paginas(m, "b", s, 2, [](std::size_t i) { return std::int64_t(i % 30); });
    const TableId c = tabela_paginas(m, "c", s, 4, [](std::size_t i) { return std::int64_t(i % 19); });
    const TableId u = tabela_paginas(m, "u", su, 1, [](std::size_t i) { return std::int64_t(i); });
    m.flush_all();
    auto in = [&](TableId t, std::uint8_t op) { return JoinInput{path(m, t, PathKind::Scan), 0, op}; };

    // BNL, M = 4, LRU: chunk = ⌊4/2⌋ = 2 (3 + 2 > 4), diretório em 1 moldura (2 × 19 × 24 B < 4 KB),
    // 1 moldura para a interna. Rodadas: ⌈3/2⌉ = 2, e a interna é relida em cada uma:
    // leituras = b_r + 2 · b_s = 3 + 2 · 2 = 7, todas faltas; nenhuma gravação.
    m.flush_all();
    m.invalidate_all();
    const OpResult bnl = ok(m.join(JoinAlgorithm::Bnl, in(a, 0), in(b, 1), 4, ResultShape{}));
    EXPECT_EQ(bnl.delta.global.reads(), 7u);
    EXPECT_EQ(bnl.delta.global.requests, 7u);
    EXPECT_EQ(bnl.delta.global.misses, 7u);
    EXPECT_EQ(bnl.delta.global.writes(), 0u);
    EXPECT_EQ(bnl.flush_writes, 0u);
    EXPECT_EQ(bnl.prep_writes, 0u);

    // Sort-merge, M = 5, interna UNIQUE (sem grupo): lista de runs em 1 moldura (6 descritores de
    // 88 B), run_pages = 5 − 2 − 1 = 2, final_fan_in = 5 − 0 − 1 = 4. A externa (76 registros)
    // vira ⌈76 / cap⌉ runs, a interna (19) uma run; cada run de r registros ocupa ⌈r / 19⌉ páginas.
    const JoinPlan p = std::get<JoinPlan>(m.plan_join(JoinAlgorithm::SortMerge, in(c, 0), in(u, 1), 5, JoinParams{}));
    ASSERT_EQ(p.desc_frames, 1u);
    ASSERT_EQ(p.run_pages, 2u);
    ASSERT_EQ(p.group_pages, 0u);
    const std::size_t cap = ScratchSpace::capacity_for(s, 2, 4096);
    const std::size_t runs_c = (76 + cap - 1) / cap;
    ASSERT_EQ(p.runs_outer, runs_c);
    ASSERT_TRUE(runs_c + 1 <= p.final_fan_in);   // sem passe intermediário
    std::size_t w_c = 0;
    for (std::size_t left = 76; left > 0; left -= std::min(left, cap)) w_c += (std::min(left, cap) + pp - 1) / pp;
    m.flush_all();
    m.invalidate_all();
    const OpResult sm = ok(m.join(JoinAlgorithm::SortMerge, in(c, 0), in(u, 1), 5, ResultShape{}));
    EXPECT_EQ(sm.rows, 76u);
    EXPECT_EQ(fase(sm, "runs_externa", false), 4u);        // lê C uma vez
    EXPECT_EQ(fase(sm, "runs_externa", true), w_c);        // grava as runs de C
    EXPECT_EQ(fase(sm, "runs_interna", false), 1u);
    EXPECT_EQ(fase(sm, "runs_interna", true), 1u);
    EXPECT_EQ(fase(sm, "intercalacao_final", false), w_c + 1);   // relê cada página de run uma vez
    EXPECT_EQ(fase(sm, "intercalacao_final", true), 0u);
    EXPECT_EQ(sm.delta.global.reads(), 4u + 1u + w_c + 1u);
    EXPECT_EQ(sm.delta.global.writes(), w_c + 1u);
    EXPECT_EQ(sm.flush_writes, 0u);
}

// CBD-010: preparo, operação e flush de fim ficam separados. O que a carga deixou sujo é gravado
// antes do 1º snapshot e contado em prep_writes; não cai no delta (se a operação expulsasse a
// página) nem no flush de fim (se ela continuasse suja).
TEST(Monitor, PreparoFlushEOperacaoSeparados) {
    Monitor m(test_config("mon_preparo"));
    const Schema s = make_schema({{"k", FieldType::Int32}, {"pad", FieldType::Char, 200}});
    const TableId a = tabela_paginas(m, "a", s, 3, [](std::size_t i) { return std::int64_t(i % 30); });
    const TableId b = tabela_paginas(m, "b", s, 2, [](std::size_t i) { return std::int64_t(i % 30); });
    // sem flush: a carga deixa páginas sujas no pool
    const JoinInput ia{path(m, a, PathKind::Scan), 0, 0}, ib{path(m, b, PathKind::Scan), 0, 1};
    const OpResult r = ok(m.join(JoinAlgorithm::Bnl, ia, ib, 4, ResultShape{}));
    EXPECT_GT(r.prep_writes, 0u);
    EXPECT_EQ(r.delta.global.writes(), 0u);
    EXPECT_EQ(r.delta.global.dirty_evictions, 0u);
    EXPECT_EQ(r.flush_writes, 0u);
    const OpResult again = ok(m.join(JoinAlgorithm::Bnl, ia, ib, 4, ResultShape{}));
    EXPECT_EQ(again.prep_writes, 0u);
    EXPECT_EQ(again.delta.global.writes(), 0u);
}

// P02 da auditoria de 10/11 (decisão 172): M conta também as páginas de índice. Numa seleção pela
// B+ primária com M = 2, cada descida pede os nós pela reserva, e não há mais o cache da área de
// índices. Um caminho com índice e dados juntos não cabe em M = 1.
TEST(Monitor, SelecaoComIndicesDentroDeM) {
    Monitor m(test_config("mon_indices_m"));
    Db db = build(m);
    const AccessPath bt = path(m, db.t["a_bt"], PathKind::PrimaryBTree);
    std::vector<Point> pts;
    for (std::int32_t k = 0; k < 300; k += 3) pts.push_back(Point{0, Key{k}});
    m.flush_all();
    m.invalidate_all();
    SelectParams sp;
    sp.m = 2;
    sp.expected_rows = static_cast<double>(pts.size());
    const OpResult r = ok(m.select_points(bt, pts, ResultShape{}, sp));
    EXPECT_EQ(r.rows, pts.size());
    EXPECT_EQ(r.delta.global.index_area_requests, 0u);
    const TableStats ts = m.stats(db.t["a_bt"]);
    const StructureStats& st = ts.structures.front();
    ASSERT_TRUE(st.height.has_value());
    if (*st.height > 1)   // cada descida pede a raiz pela reserva (pode ser acerto, se ela ainda estiver lá)
        EXPECT_GE(r.delta.per_kind[static_cast<std::size_t>(FileKind::Index)].requests, pts.size());
    const AccessPath sec = path(m, db.t["b_heap"], PathKind::SecondaryBTreeNonClustering);
    SelectParams um;
    um.m = 1;
    const auto plano = m.plan_select(sec, false, 1, um);
    ASSERT_TRUE(std::holds_alternative<NotApplicable>(plano));
    EXPECT_TRUE(std::get<NotApplicable>(plano).kind == NotApplicable::Kind::Inviavel);
}

TEST(Monitor, MruLeMenosQueLruNoBnlComVariasPassadas) {
    std::map<Policy, std::uint64_t> reads;
    for (Policy p : {Policy::Lru, Policy::Mru}) {
        Monitor m(test_config(std::string("mon_pol_") + to_string(p), p));
        Db db = build(m);
        JoinParams jp;
        jp.chunk = 2;   // várias passadas sobre a interna
        const OpResult r = ok(join(m, db, JoinAlgorithm::Bnl, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 16, true, jp));
        reads[p] = r.delta.global.reads();
    }
    EXPECT_LT(reads[Policy::Mru], reads[Policy::Lru]);
}

TEST(Monitor, SemCacheOcultoEntreAreasDeTrabalho) {
    std::vector<std::uint64_t> reads;
    for (std::uint32_t work : {8u, 32u}) {
        auto cfg = test_config("mon_cache_" + std::to_string(work));
        cfg.area_frames[3] = work;
        Monitor m(cfg);
        Db db = build(m);
        reads.push_back(ok(join(m, db, JoinAlgorithm::Bnl, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 8)).delta.global.reads());
    }
    EXPECT_EQ(reads[0], reads[1]);
}

TEST(Monitor, InaplicaveisDevolvemMotivo) {   // (128)
    Monitor m(test_config("mon_na"));
    Db db = build(m);
    const KeyRange r{0, Key{std::int32_t{10}}, Key{std::int32_t{20}}};
    const Outcome hr = m.select_range(path(m, db.t["a_hash"], PathKind::PrimaryHash), r, ResultShape{});
    ASSERT_TRUE(std::holds_alternative<NotApplicable>(hr));
    EXPECT_TRUE(std::get<NotApplicable>(hr).kind == NotApplicable::Kind::NaoAplicavel);   // caso 4 (170)
    const Outcome hj = join(m, db, JoinAlgorithm::Hash, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 2);
    ASSERT_TRUE(std::holds_alternative<NotApplicable>(hj));
    EXPECT_TRUE(std::get<NotApplicable>(hj).kind == NotApplicable::Kind::Inviavel);       // m < 3 (170)
    EXPECT_FALSE(std::get<NotApplicable>(hj).reason.empty());
}

TEST(Monitor, HashJoinRecursivoComMPequeno) {   // (171)
    Monitor m(test_config("mon_hash_rec"));
    Db db = build(m);
    JoinInput ia{path(m, db.t["a_heap"], PathKind::Scan), 0, 0}, ib{path(m, db.t["b_heap"], PathKind::Scan), 0, 1};
    const auto planned = m.plan_join(JoinAlgorithm::Hash, ia, ib, 3, JoinParams{});
    ASSERT_TRUE(std::holds_alternative<JoinPlan>(planned));
    EXPECT_GE(std::get<JoinPlan>(planned).levels, std::size_t{2});   // um passe não basta com m = 3
    const OpResult ref = ok(join(m, db, JoinAlgorithm::Bnl, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 16));
    for (std::size_t mm : {std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
        const OpResult h = ok(join(m, db, JoinAlgorithm::Hash, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, mm));
        EXPECT_EQ(h.rows, ref.rows);
        EXPECT_EQ(h.checksum, ref.checksum);
        EXPECT_EQ(h.flush_writes, 0u);
    }
    // com pré-particionamento pelo arquivo hash e recursão nos grupos de buckets
    const OpResult hp = ok(join(m, db, JoinAlgorithm::Hash, "a_hash", PathKind::PrimaryHash, "b_hash", PathKind::PrimaryHash, 3));
    EXPECT_EQ(hp.checksum, ref.checksum);
}

TEST(Monitor, HashJoinBailoutEmGrupoIndivisivel) {   // (171)
    Monitor m(test_config("mon_hash_bailout"));
    Db db = build(m);
    // c_heap tem 2 valores com 600 linhas cada: nenhum passe divide um grupo de 600 linhas em m − 1 = 3 páginas
    const OpResult h = ok(join(m, db, JoinAlgorithm::Hash, "c_heap", PathKind::Scan, "c_heap", PathKind::Scan, 4));
    const OpResult b = ok(join(m, db, JoinAlgorithm::Bnl, "c_heap", PathKind::Scan, "c_heap", PathKind::Scan, 4));
    EXPECT_EQ(h.rows, 2u * 600u * 600u);
    EXPECT_EQ(h.checksum, b.checksum);
    bool bailed = false;
    for (const auto& ph : h.phases) bailed = bailed || (ph.name.find("bailouts=") != std::string::npos &&
                                                        ph.name.find("bailouts=0") == std::string::npos);
    EXPECT_TRUE(bailed);
}

TEST(Monitor, SelecoesConcordamEntreCaminhos) {
    Monitor m(test_config("mon_sel"));
    Db db = build(m);
    std::vector<Point> batch;
    for (int i = 0; i < 50; ++i) batch.push_back(Point{0, Key{std::int32_t((i * 11) % 300)}});
    std::optional<std::uint64_t> pts;
    for (auto [t, k] : std::vector<std::pair<std::string, PathKind>>{{"a_heap", PathKind::Scan}, {"a_bt", PathKind::PrimaryBTree}, {"a_hash", PathKind::PrimaryHash}}) {
        m.flush_all();
        m.invalidate_all();
        const OpResult r = ok(m.select_points(path(m, db.t[t], k), batch, ResultShape{}));
        EXPECT_EQ(r.rows, 50u);
        if (!pts) pts = r.checksum;
        EXPECT_EQ(r.checksum, *pts);
    }
    const KeyRange range{0, Key{std::int32_t{100}}, Key{std::int32_t{199}}};
    std::optional<std::uint64_t> rg;
    for (auto [t, k] : std::vector<std::pair<std::string, PathKind>>{{"b_heap", PathKind::Scan}, {"b_bt", PathKind::PrimaryBTree}, {"b_heap", PathKind::SecondaryBTreeNonClustering}}) {
        m.flush_all();
        m.invalidate_all();
        const OpResult r = ok(m.select_range(path(m, db.t[t], k), range, ResultShape{}));
        EXPECT_EQ(r.rows, 400u);
        if (!rg) rg = r.checksum;
        EXPECT_EQ(r.checksum, *rg);
    }
    SelectParams sp;
    sp.m = 4;
    sp.sort_rids = true;   // variante da 117: mesmas linhas, menos faltas
    const OpResult s = ok(m.select_range(path(m, db.t["b_heap"], PathKind::SecondaryBTreeNonClustering), range, ResultShape{}, sp));
    EXPECT_EQ(s.checksum, *rg);
}

TEST(Monitor, GrupoDeDuplicatasQueNaoCabeViraInaplicavel) {   // (151)
    Monitor m(test_config("mon_grupo"));
    Db db = build(m);
    JoinParams jp;
    jp.group_pages = 1;
    const Outcome o = join(m, db, JoinAlgorithm::SortMerge, "c_heap", PathKind::Scan, "c_heap", PathKind::Scan, 8, true, jp);
    ASSERT_TRUE(std::holds_alternative<NotApplicable>(o));
    EXPECT_TRUE(std::get<NotApplicable>(o).reason.find("não cabe") != std::string::npos);
}

// O tamanho máximo declarado de VarChar não muda os pares de registros efetivamente armazenados.
TEST(Monitor, SortMergeComVarCharCurtoIndependeDoMaximoDeclarado) {
    std::optional<std::uint64_t> checksum;
    for (const std::uint16_t width : {24, 1000, 4050}) {
        Monitor m(test_config("mon_sm_var_" + std::to_string(width)));
        const Schema schema = make_schema({{"k", FieldType::Int32, 0, true}, {"v", FieldType::VarChar, width}});
        const TableId a = m.create_table("a", schema, {}), b = m.create_table("b", schema, {});
        std::vector<std::vector<Value>> rows;
        for (std::int64_t k = 999; k >= 0; --k) rows.push_back({k, std::string("x")});
        RowsStream ra(m, a, rows), rb(m, b, rows);
        m.bulk_load(a, ra);
        m.bulk_load(b, rb);
        const JoinInput outer{path(m, a, PathKind::Scan), 0, 0}, inner{path(m, b, PathKind::Scan), 0, 1};
        for (const std::size_t memory : {5, 16}) {
            m.flush_all();
            m.invalidate_all();
            const auto planned = m.plan_join(JoinAlgorithm::SortMerge, outer, inner, memory, {});
            EXPECT_TRUE(std::holds_alternative<JoinPlan>(planned));
            const auto outcome = m.join(JoinAlgorithm::SortMerge, outer, inner, memory, {});
            if (const auto* result = std::get_if<OpResult>(&outcome)) {
                EXPECT_EQ(result->rows, 1000u);
                EXPECT_EQ(result->flush_writes, 0u);
                if (!checksum) checksum = result->checksum;
                EXPECT_EQ(result->checksum, *checksum);
            } else {
                EXPECT_TRUE(std::holds_alternative<OpResult>(outcome));
            }
        }
    }
}

TEST(Monitor, SortMergeVarCharAposManutencao) {
    Monitor m(test_config("mon_sm_var_manutencao"));
    const Schema schema = make_schema({{"k", FieldType::Int32, 0, true}, {"v", FieldType::VarChar, 4050}});
    const TableId a = m.create_table("a", schema, {}), b = m.create_table("b", schema, {});
    std::vector<std::vector<Value>> rows;
    for (std::int64_t k = 0; k < 1000; ++k) rows.push_back({k, std::string("x")});
    for (const TableId t : {a, b}) {
        RowsStream stream(m, t, rows);
        m.bulk_load(t, stream);
        m.insert(t, m.encode(t, {std::int64_t{1000}, std::string(200, 'y')}));
        m.reorganize(t);
    }
    const JoinInput outer{path(m, a, PathKind::Scan), 0, 0}, inner{path(m, b, PathKind::Scan), 0, 1};
    EXPECT_EQ(ok(m.join(JoinAlgorithm::SortMerge, outer, inner, 16, {})).rows, 1001u);
    for (const TableId t : {a, b}) {
        EXPECT_EQ(m.erase_key(t, 0, Key{std::int32_t{1000}}), 1u);
        m.reorganize(t);
    }
    EXPECT_EQ(ok(m.join(JoinAlgorithm::SortMerge, outer, inner, 5, {})).rows, 1000u);
}

TEST(Monitor, SortMergeCapacidadeZeroViraInviavelNoPlano) {
    Monitor m(test_config("mon_sm_var_zero"));
    const Schema schema = make_schema({{"k", FieldType::Int32, 0, true}, {"v", FieldType::VarChar, 4050}});
    const TableId a = m.create_table("a", schema, {}), b = m.create_table("b", schema, {});
    for (const TableId t : {a, b}) {
        RowsStream stream(m, t, {{std::int64_t{1}, std::string(4050, 'x')}});
        m.bulk_load(t, stream);
    }
    const JoinInput outer{path(m, a, PathKind::Scan), 0, 0}, inner{path(m, b, PathKind::Scan), 0, 1};
    const auto planned = m.plan_join(JoinAlgorithm::SortMerge, outer, inner, 4, {});
    EXPECT_TRUE(std::holds_alternative<NotApplicable>(planned));
    EXPECT_EQ(ok(m.join(JoinAlgorithm::SortMerge, outer, inner, 5, {})).rows, 1u);
}

// CBD-007 (issue #1): com a interna UNIQUE, o sort-merge usa group_pages = 0 e não guarda o grupo.
// O UNIQUE só é garantido quando uma estrutura indexa o campo (157); num heap ele é só declarado.
// Na base, 10 × 10 registros de mesma chave devolviam 10 pares em vez de 100, sem aviso.
TEST(Monitor, SortMergeRejeitaUniqueViolado) {
    Monitor m(test_config("mon_sm_unique"));
    const Schema su = make_schema({{"k", FieldType::Int32, 0, true}, {"v", FieldType::Int32}});
    const Schema sn = make_schema({{"k", FieldType::Int32}, {"v", FieldType::Int32}});
    std::vector<std::vector<Value>> dez, distintos;
    for (std::int64_t i = 0; i < 10; ++i) {
        dez.push_back({std::int64_t{7}, i});
        distintos.push_back({i, i});
    }
    auto add = [&](const std::string& name, const Schema& s, const std::vector<std::vector<Value>>& rows) {
        const TableId t = m.create_table(name, s, PrimarySpec{});
        RowsStream rs(m, t, rows);
        m.bulk_load(t, rs);
        return t;
    };
    const TableId ext = add("externa", sn, dez);
    const TableId uni = add("interna_unique", su, dez);   // o heap aceita a repetição (157)
    const TableId nun = add("interna_not_unique", sn, dez);
    const TableId uok = add("interna_unique_ok", su, distintos);
    auto input = [&](TableId t, std::uint8_t operand) { return JoinInput{path(m, t, PathKind::Scan), 0, operand}; };
    auto sort_merge = [&](TableId inner) {
        m.flush_all();
        m.invalidate_all();
        return m.join(JoinAlgorithm::SortMerge, input(ext, 0), input(inner, 1), 8, ResultShape{});
    };
    ASSERT_EQ(std::get<JoinPlan>(m.plan_join(JoinAlgorithm::SortMerge, input(ext, 0), input(uni, 1), 8, JoinParams{}))
                  .group_pages, 0u);
    EXPECT_THROW(sort_merge(uni), std::logic_error);   // nunca "sucesso" com 10 pares
    EXPECT_EQ(ok(sort_merge(nun)).rows, 100u);          // NOT UNIQUE: o grupo de duplicatas dá os 100
    const OpResult u = ok(sort_merge(uok));             // UNIQUE válido continua com group_pages = 0
    EXPECT_EQ(u.rows, 10u);
    EXPECT_EQ(u.checksum, ok(m.join(JoinAlgorithm::Bnl, input(ext, 0), input(uok, 1), 8, ResultShape{})).checksum);
}

TEST(Monitor, HashPreParticionadoIncompativelUsaSoOMaior) {   // (164)
    Monitor m(test_config("mon_prepart"));
    Db db = build(m);
    JoinInput ia{path(m, db.t["a_hash"], PathKind::PrimaryHash), 0, 0}, ib{path(m, db.t["b_hash"], PathKind::PrimaryHash), 0, 1};
    const auto planned = m.plan_join(JoinAlgorithm::Hash, ia, ib, 5, JoinParams{});
    ASSERT_TRUE(std::holds_alternative<JoinPlan>(planned));
    const JoinPlan& p = std::get<JoinPlan>(planned);
    EXPECT_TRUE(p.partitions > 1);
    EXPECT_TRUE(p.prepartitioned_outer != p.prepartitioned_inner);   // buckets diferentes: só um aproveitado
    EXPECT_TRUE(p.prepartitioned_inner);                              // o maior (B)
    const OpResult h = ok(join(m, db, JoinAlgorithm::Hash, "a_hash", PathKind::PrimaryHash, "b_hash", PathKind::PrimaryHash, 5));
    const OpResult b = ok(join(m, db, JoinAlgorithm::Bnl, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 5));
    EXPECT_EQ(h.checksum, b.checksum);
}

TEST(Monitor, SortMergeAproveitaEntradaOrdenada) {   // (164)
    Monitor m(test_config("mon_smj"));
    Db db = build(m);
    JoinInput ia{path(m, db.t["a_bt"], PathKind::PrimaryBTree), 0, 0}, ib{path(m, db.t["b_heap"], PathKind::Scan), 0, 1};
    const auto p = std::get<JoinPlan>(m.plan_join(JoinAlgorithm::SortMerge, ib, ia, 8, JoinParams{}));
    EXPECT_FALSE(p.sort_inner);   // a_bt já vem ordenada pela B+
    EXPECT_TRUE(p.sort_outer);
    const OpResult sorted = ok(join(m, db, JoinAlgorithm::SortMerge, "a_bt", PathKind::PrimaryBTree, "b_heap", PathKind::Scan, 8, false));
    const OpResult plain = ok(join(m, db, JoinAlgorithm::SortMerge, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 8, false));
    EXPECT_EQ(sorted.checksum, plain.checksum);
    EXPECT_LT(sorted.delta.global.reads() + sorted.delta.global.writes(), plain.delta.global.reads() + plain.delta.global.writes());
}

// Cada cursor clustering sobre hash mantém uma folha de índice e uma página de dados.
TEST(Monitor, SortMergeClusteringHashContaIndiceEDados) {
    for (const bool unique : {true, false}) {
        Monitor m(test_config(unique ? "mon_sm_clust_unique" : "mon_sm_clust_duplicatas"));
        const Schema schema = make_schema({{"k", FieldType::Int32, 0, unique}});
        const PrimarySpec primary{StructureKind::Hash, 0, 0.7, 3};
        const TableId a = m.create_table("a", schema, primary), b = m.create_table("b", schema, primary);
        std::vector<std::vector<Value>> rows;
        for (std::int64_t i = 0; i < 3; ++i) rows.push_back({unique ? i : std::int64_t{7}});
        for (const TableId t : {a, b}) {
            RowsStream stream(m, t, rows);
            m.bulk_load(t, stream);
            m.create_index(t, {StructureKind::SecBTree, 0, true});
        }
        const JoinInput outer{path(m, a, PathKind::SecondaryBTreeClustering), 0, 0};
        const JoinInput inner{path(m, b, PathKind::SecondaryBTreeClustering), 0, 1};
        JoinParams params;
        params.group_pages = 1;
        const std::size_t minimum = unique ? 4 : 5;   // 2 cursores de 2 páginas, mais grupo se necessário
        const auto rejected = m.plan_join(JoinAlgorithm::SortMerge, outer, inner, minimum - 1, params);
        EXPECT_TRUE(std::holds_alternative<NotApplicable>(rejected));
        EXPECT_NO_THROW({
            const auto outcome = m.join(JoinAlgorithm::SortMerge, outer, inner, minimum - 1, {}, params);
            EXPECT_TRUE(std::holds_alternative<NotApplicable>(outcome));
        });
        const auto planned = m.plan_join(JoinAlgorithm::SortMerge, outer, inner, minimum, params);
        ASSERT_TRUE(std::holds_alternative<JoinPlan>(planned));
        EXPECT_LE(std::get<JoinPlan>(planned).frames_used(), minimum);
        m.flush_all();
        m.invalidate_all();
        const auto result = ok(m.join(JoinAlgorithm::SortMerge, outer, inner, minimum, {}, params));
        EXPECT_EQ(result.rows, unique ? 3u : 9u);
        EXPECT_EQ(result.delta.global.index_area_requests, 0u);
    }
}

TEST(Monitor, SortMergeDistingueClusteringHashDeClusteringBPlus) {
    Monitor m(test_config("mon_sm_clust_fontes"));
    const Schema schema = make_schema({{"k", FieldType::Int32, 0, true}});
    auto add = [&](const std::string& name, PrimarySpec primary) {
        const auto table = m.create_table(name, schema, primary);
        RowsStream stream(m, table, {{std::int64_t{1}}, {std::int64_t{2}}, {std::int64_t{3}}});
        m.bulk_load(table, stream);
        if (primary.kind != StructureKind::Heap) m.create_index(table, {StructureKind::SecBTree, 0, true});
        return table;
    };
    const auto bt = add("bt", {StructureKind::BTree, 0, 0.7});
    const auto hash = add("hash", {StructureKind::Hash, 0, 0.7, 3});
    const auto heap = add("heap", {});
    const JoinInput clustered_bt{path(m, bt, PathKind::SecondaryBTreeClustering), 0, 0};
    const JoinInput primary_bt{path(m, bt, PathKind::PrimaryBTree), 0, 1};
    const JoinInput clustered_hash{path(m, hash, PathKind::SecondaryBTreeClustering), 0, 0};
    const JoinInput unsorted{path(m, heap, PathKind::Scan), 0, 1};
    EXPECT_EQ(ok(m.join(JoinAlgorithm::SortMerge, clustered_bt, primary_bt, 3, {})).rows, 3u);
    EXPECT_EQ(ok(m.join(JoinAlgorithm::SortMerge, clustered_hash, primary_bt, 3, {})).rows, 3u);
    EXPECT_EQ(ok(m.join(JoinAlgorithm::SortMerge, clustered_hash, unsorted, 4, {})).rows, 3u);
    std::vector<std::vector<Value>> extra;
    for (std::int64_t k = 4; k <= 1003; ++k) extra.push_back({k});
    RowsStream stream(m, heap, std::move(extra));
    m.bulk_load(heap, stream);
    // As runs da entrada heap precisam de um passe para deixar espaço ao índice retido.
    EXPECT_EQ(ok(m.join(JoinAlgorithm::SortMerge, clustered_hash, unsorted, 5, {})).rows, 3u);
}

TEST(Monitor, TabelaMenorCabeNumaPassada) {   // emenda da 108: menor é b ≤ m − 2
    Monitor m(test_config("mon_menor"));
    Db db = build(m);
    const auto ba = m.stats(db.t["a_heap"]).b, bb = m.stats(db.t["b_heap"]).b;
    ASSERT_TRUE(ba + 2 <= 16);
    const OpResult bnl = ok(join(m, db, JoinAlgorithm::Bnl, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 16));
    EXPECT_EQ(bnl.delta.global.reads(), std::uint64_t{ba} + bb);   // externa inteira no chunk: uma passada
    JoinInput ia{path(m, db.t["a_heap"], PathKind::Scan), 0, 0}, ib{path(m, db.t["b_heap"], PathKind::Scan), 0, 1};
    const auto hp = std::get<JoinPlan>(m.plan_join(JoinAlgorithm::Hash, ia, ib, 16, JoinParams{}));
    EXPECT_EQ(hp.partitions, std::size_t{1});                      // o build cabe: sem particionar
    const OpResult hj = ok(join(m, db, JoinAlgorithm::Hash, "a_heap", PathKind::Scan, "b_heap", PathKind::Scan, 16));
    EXPECT_EQ(hj.delta.global.writes(), 0u);
    EXPECT_EQ(hj.checksum, bnl.checksum);
}

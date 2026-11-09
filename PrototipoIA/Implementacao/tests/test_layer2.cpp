// Testes da camada 2: junções, seleções, inaplicáveis, políticas, orçamento e partições (§10, 151, 152, 164).
#include "helpers.hpp"
#include "layer2/monitor.hpp"
#include "mini_test.hpp"

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

// Testes da camada 3: página com slots, registro, organizações, índices, RIDs, temporários e memória de trabalho.
#include "helpers.hpp"
#include "layer3/page.hpp"
#include "layer3/record.hpp"
#include "layer3/storage_manager.hpp"
#include "mini_test.hpp"

#include <algorithm>
#include <map>
#include <set>

using namespace cbd;

namespace {

Schema kv_schema(bool unique_k, int pad = 100) {
    return make_schema({{"k", FieldType::Int32, 0, unique_k}, {"g", FieldType::Int32}, {"u", FieldType::Int32, 0, true},
                        {"pad", FieldType::Char, static_cast<std::uint16_t>(pad)}});
}
std::vector<std::byte> kv(const StorageManager& sm, TableId t, std::int64_t k, std::int64_t g, std::int64_t u) {
    return sm.encode(t, {k, g, u, std::string("x")});
}

class VecStream final : public RecordStream {
public:
    explicit VecStream(std::vector<std::vector<std::byte>> v) : v_(std::move(v)) {}
    std::optional<std::span<const std::byte>> next() override {
        if (i_ >= v_.size()) return std::nullopt;
        return std::span<const std::byte>(v_[i_++]);
    }
private:
    std::vector<std::vector<std::byte>> v_;
    std::size_t i_ = 0;
};

AccessPath path_of(const StorageManager& sm, TableId t, PathKind k) {
    for (const auto& p : sm.access_paths(t))
        if (p.kind == k) return p;
    throw std::logic_error("caminho ausente");
}

std::vector<std::int32_t> keys_in_order(StorageManager& sm, TableId t, FieldId f, const Predicate& pred, PathKind kind) {
    auto r = sm.reserve(2);
    SearchCursor c = sm.find(path_of(sm, t, kind), pred, kSeqOnce, r);
    std::vector<std::int32_t> out;
    while (auto rec = c.next()) out.push_back(rec->get<std::int32_t>(f));
    return out;
}

std::uint64_t lcg(std::uint64_t& s) { return s = s * 6364136223846793005ull + 1442695040888963407ull, s >> 33; }

}  // namespace

TEST(SlottedPage, SlotsNaoSaoRenumerados) {
    std::vector<std::byte> page(4096);
    sp_init(page, PageType::Heap);
    std::vector<std::byte> a(50, std::byte{1}), b(300, std::byte{2}), c(70, std::byte{3});
    EXPECT_EQ(*sp_insert(page, a), 0);
    EXPECT_EQ(*sp_insert(page, b), 1);
    EXPECT_EQ(*sp_insert(page, c), 2);
    sp_erase(page, 1);
    sp_compact(page);
    EXPECT_TRUE(sp_record(page, 1).empty());
    EXPECT_EQ(sp_record(page, 2).size(), 70u);
    EXPECT_TRUE(sp_record(page, 2)[0] == std::byte{3});
    EXPECT_EQ(*sp_insert(page, b), 1);   // reaproveita o slot removido
    EXPECT_EQ(read_header(page).live, 3);
}

TEST(Record, NuloVarCharEVerificacaoSempreLigada) {
    const Schema s = make_schema({{"id", FieldType::Int32, 0, true}, {"nome", FieldType::Char, 10, false, true},
                                  {"nota", FieldType::Int64}, {"bio", FieldType::VarChar, 20, false, true},
                                  {"apelido", FieldType::VarChar, 5}});
    EXPECT_EQ(s.null_bytes, 1u);
    const auto bytes = encode_record(s, {std::int64_t{7}, Value{}, std::int64_t{99}, std::string("olá mundo"), std::string("zé")});
    const RecordView v = detail::make_view(bytes, &s);
    EXPECT_EQ(v.get<std::int32_t>(0), 7);
    EXPECT_TRUE(v.is_null(1));
    EXPECT_FALSE(v.is_null(3));
    EXPECT_THROW(v.key(1), std::logic_error);                    // chave NULL (158)
    EXPECT_EQ(std::get<std::string>(v.key(3)), std::string("olá mundo"));
    EXPECT_EQ(std::get<std::string>(v.key(4)), std::string("zé"));
    EXPECT_THROW(v.get<std::int64_t>(0), std::out_of_range);     // tamanho errado lança em release
    EXPECT_THROW(v.get<std::int32_t>(3), std::out_of_range);     // VarChar por get
    auto bad = bytes;
    store<std::uint16_t>(bad.data() + s.fixed_end, 60000);       // offset de VarChar corrompido
    const RecordView w = detail::make_view(bad, &s);
    EXPECT_THROW(w.get_var(3), std::out_of_range);
    EXPECT_THROW(encode_record(s, {Value{}, Value{}, std::int64_t{1}, Value{}, std::string("a")}), std::invalid_argument);
}

TEST(StorageManager, BPlusUnicaContraOraculo) {
    StorageManager sm(test_config("sm_bt_unica"));
    const TableId t = sm.create_table("t", kv_schema(true), PrimarySpec{StructureKind::BTree, 0, 0.7});
    std::vector<std::vector<std::byte>> recs;
    std::set<std::int32_t> oracle;
    for (int i = 0; i < 3000; ++i) {
        recs.push_back(kv(sm, t, 2 * i, i % 10, i));
        oracle.insert(2 * i);
    }
    VecStream vs(std::move(recs));
    sm.bulk_load(t, vs);
    std::uint64_t s = 42;
    for (int i = 0; i < 2000; ++i) {
        const auto k = static_cast<std::int32_t>(2 * (lcg(s) % 4000) + 1);
        if (oracle.count(k)) continue;
        sm.insert(t, kv(sm, t, k, 0, 100000 + i));
        oracle.insert(k);
    }
    EXPECT_THROW(sm.insert(t, kv(sm, t, 0, 0, 1)), DuplicateKey);   // (157)
    for (int i = 0; i < 1500; ++i) {
        const auto k = static_cast<std::int32_t>(lcg(s) % 8000);
        const std::size_t n = sm.erase_key(t, 0, Key{k});
        EXPECT_EQ(n, oracle.erase(k));
    }
    EXPECT_EQ(sm.verify(t), std::string());
    sm.reorganize(t);
    const auto got = keys_in_order(sm, t, 0, KeyRange{0}, PathKind::PrimaryBTree);
    EXPECT_EQ(got.size(), oracle.size());
    EXPECT_TRUE(std::equal(got.begin(), got.end(), oracle.begin(), oracle.end()));
    EXPECT_GE(sm.stats(t).structures.front().height.value_or(0), 2);
}

TEST(StorageManager, BPlusNaoUnicaComDuplicatas) {
    StorageManager sm(test_config("sm_bt_dup"));
    const TableId t = sm.create_table("t", kv_schema(false), PrimarySpec{StructureKind::BTree, 0, 0.7});
    std::uint64_t s = 7;
    std::vector<int> ks;
    for (int k = 0; k < 100; ++k)
        for (int d = 0; d < 40; ++d) ks.push_back(k);
    for (std::size_t i = ks.size(); i > 1; --i) std::swap(ks[i - 1], ks[lcg(s) % i]);
    int u = 0;
    for (int k : ks) sm.insert(t, kv(sm, t, k, 0, u++));
    EXPECT_EQ(sm.verify(t), std::string());
    sm.reorganize(t);
    EXPECT_EQ(keys_in_order(sm, t, 0, Point{0, Key{std::int32_t{37}}}, PathKind::PrimaryBTree).size(), 40u);
    EXPECT_EQ(sm.erase_key(t, 0, Key{std::int32_t{37}}), 40u);
    EXPECT_EQ(sm.verify(t), std::string());
    sm.reorganize(t);
    EXPECT_EQ(keys_in_order(sm, t, 0, Point{0, Key{std::int32_t{37}}}, PathKind::PrimaryBTree).size(), 0u);
    KeyRange r{0, Key{std::int32_t{10}}, Key{std::int32_t{19}}};
    EXPECT_EQ(keys_in_order(sm, t, 0, r, PathKind::PrimaryBTree).size(), 400u);
}

TEST(StorageManager, HashRejeitaDuplicataSoNoUnique) {
    StorageManager sm(test_config("sm_hash_dup"));
    const TableId tu = sm.create_table("u", kv_schema(true), PrimarySpec{StructureKind::Hash, 0, 0.7, 5});
    const TableId tn = sm.create_table("n", kv_schema(false), PrimarySpec{StructureKind::Hash, 0, 0.7, 5});
    sm.insert(tu, kv(sm, tu, 5, 0, 1));
    EXPECT_THROW(sm.insert(tu, kv(sm, tu, 5, 0, 2)), DuplicateKey);   // (157)
    sm.insert(tn, kv(sm, tn, 5, 0, 1));
    EXPECT_NO_THROW(sm.insert(tn, kv(sm, tn, 5, 0, 2)));
    const TableId bn = sm.create_table("bn", kv_schema(false), PrimarySpec{StructureKind::BTree, 0, 0.7});
    sm.insert(bn, kv(sm, bn, 5, 0, 1));
    EXPECT_NO_THROW(sm.insert(bn, kv(sm, bn, 5, 0, 2)));
}

TEST(StorageManager, OverflowDoHashESeguidoELiberado) {
    StorageManager sm(test_config("sm_hash_ovf"));
    const TableId t = sm.create_table("t", kv_schema(true), PrimarySpec{StructureKind::Hash, 0, 0.7, 1});
    for (int i = 0; i < 120; ++i) sm.insert(t, kv(sm, t, i, 0, i));   // 1 bucket: cadeia de overflow
    const auto occ = sm.occupancy(sm.structures(t).front());
    EXPECT_GE(occ.overflow, 2u);
    for (int i = 0; i < 120; i += 17)
        EXPECT_EQ(keys_in_order(sm, t, 0, Point{0, Key{std::int32_t(i)}}, PathKind::PrimaryHash).size(), 1u);
    for (int i = 60; i < 120; ++i) sm.erase_key(t, 0, Key{std::int32_t(i)});
    EXPECT_LT(sm.occupancy(sm.structures(t).front()).overflow, occ.overflow);   // overflow vazio liberado (162)
    EXPECT_EQ(keys_in_order(sm, t, 0, Point{0, Key{std::int32_t(59)}}, PathKind::PrimaryHash).size(), 1u);
}

TEST(StorageManager, RidsEstaveisComDivisoesEReorganize) {   // (141, 169)
    StorageManager sm(test_config("sm_rids"));
    const TableId t = sm.create_table("t", kv_schema(true), PrimarySpec{StructureKind::BTree, 0, 1.0});
    sm.create_index(t, IndexSpec{StructureKind::SecBTree, 1, false});   // g, não único
    sm.create_index(t, IndexSpec{StructureKind::SecHash, 2, false});    // u, único
    std::vector<std::vector<std::byte>> recs;
    std::map<int, int> per_g;
    for (int i = 0; i < 2000; ++i) {
        recs.push_back(kv(sm, t, 4 * i, i % 13, i));
        ++per_g[i % 13];
    }
    VecStream vs(std::move(recs));
    sm.bulk_load(t, vs);
    std::uint64_t s = 99;
    for (int i = 0; i < 1500; ++i) {   // folhas cheias (fill = 1.0): cada inserção divide e move registros
        const int k = static_cast<int>(4 * (lcg(s) % 2000) + 1 + lcg(s) % 3);
        try {
            sm.insert(t, kv(sm, t, k, k % 13, 10000 + i));
            ++per_g[k % 13];
        } catch (const DuplicateKey&) {
        }
    }
    auto check = [&] {
        auto r = sm.reserve(2);
        for (const auto& [g, n] : per_g) {
            AccessPath p;
            for (const auto& ap : sm.access_paths(t))
                if (ap.kind == PathKind::SecondaryBTreeNonClustering) p = ap;
            RidCursor rc = sm.probe(p, Point{1, Key{std::int32_t(g)}}, kRandomReuse);
            int seen = 0;
            while (auto rid = rc.next()) {
                PinnedRecord pr = sm.fetch(t, *rid, kRandomOnce, r);
                EXPECT_EQ(pr.view.get<std::int32_t>(1), g);
                ++seen;
            }
            EXPECT_EQ(seen, n);
        }
        AccessPath hp;
        for (const auto& ap : sm.access_paths(t))
            if (ap.kind == PathKind::SecondaryHash) hp = ap;
        for (int u : {0, 777, 1999, 10005, 11499}) {
            RidCursor rc = sm.probe(hp, Point{2, Key{std::int32_t(u)}}, kRandomReuse);
            if (auto rid = rc.next()) {
                PinnedRecord pr = sm.fetch(t, *rid, kRandomOnce, r);
                EXPECT_EQ(pr.view.get<std::int32_t>(2), u);
            }
        }
    };
    check();
    EXPECT_EQ(sm.verify(t), std::string());
    for (int i = 0; i < 300; ++i) {
        const int k = 4 * static_cast<int>(lcg(s) % 2000);
        if (sm.erase_key(t, 0, Key{std::int32_t(k)})) --per_g[(k / 4) % 13];
    }
    EXPECT_EQ(sm.verify(t), std::string());
    check();
    sm.reorganize(t);
    check();
}

TEST(StorageManager, HeapNaoLiberaBlocosForaDoReorganize) {   // (162)
    StorageManager sm(test_config("sm_heap"));
    const TableId t = sm.create_table("t", kv_schema(true), PrimarySpec{});
    std::vector<RID> rids;
    for (int i = 0; i < 200; ++i) rids.push_back(sm.insert(t, kv(sm, t, i, 0, i)));
    const std::uint32_t last = rids.back().block;
    int erased = 0;
    for (std::size_t i = 0; i < rids.size(); ++i)
        if (rids[i].block == 3) { sm.erase(t, rids[i]); ++erased; }
    EXPECT_GT(erased, 0);
    const RID nr = sm.insert(t, kv(sm, t, 9999, 0, 9999));
    EXPECT_EQ(nr.block, last);   // inserção sempre no fim (30)
    {
        auto r = sm.reserve(1);
        BlockScan scan = sm.scan_blocks(t, kSeqOnce, r);
        int n = 0;
        while (auto c = scan.next()) {
            while (auto rec = c->next()) {
                EXPECT_NE(c->block().block, 3u);
                ++n;
            }
        }
        EXPECT_EQ(n, 200 - erased + 1);
    }
    sm.reorganize(t);
    EXPECT_LT(sm.stats(t).b, last);
}

TEST(StorageManager, EstatisticasSemEntradaESaida) {   // (160)
    StorageManager sm(test_config("sm_stats"));
    const TableId t = sm.create_table("t", kv_schema(true), PrimarySpec{StructureKind::BTree, 0, 0.7});
    for (int i = 0; i < 50; ++i) sm.insert(t, kv(sm, t, i, 0, i));
    EXPECT_THROW(sm.stats(t), std::logic_error);   // desatualizadas depois de insert
    sm.reorganize(t);
    sm.flush_all();
    sm.invalidate_all();
    const Counters s0 = sm.snapshot();
    const TableStats st = sm.stats(t);
    EXPECT_EQ((sm.snapshot() - s0).global.requests, 0u);
    EXPECT_EQ(st.n, 50u);
}

TEST(StorageManager, ValidaCriacaoDeIndices) {
    StorageManager sm(test_config("sm_valida"));
    const TableId h = sm.create_table("h", kv_schema(false), PrimarySpec{});
    EXPECT_THROW(sm.create_index(h, IndexSpec{StructureKind::SecHash, 1, false}), std::invalid_argument);   // g não é único
    EXPECT_THROW(sm.create_index(h, IndexSpec{StructureKind::SecBTree, 1, true}), std::invalid_argument);   // clustering sobre heap
    const TableId b = sm.create_table("b", kv_schema(false), PrimarySpec{StructureKind::BTree, 0, 0.7});
    EXPECT_THROW(sm.create_index(b, IndexSpec{StructureKind::SecBTree, 1, true}), std::invalid_argument);   // fora do campo da organização
    EXPECT_NO_THROW(sm.create_index(b, IndexSpec{StructureKind::SecBTree, 0, true}));
}

TEST(TempFile, SealGravaKBlocosEReleituraLeK) {
    StorageManager sm(test_config("sm_temp"));
    const Schema s = kv_schema(true);
    auto r = sm.reserve(4);
    const Counters s0 = sm.snapshot();
    TempFile tf = sm.create_temp(s, r);
    const auto rec = encode_record(s, {std::int64_t{1}, std::int64_t{2}, std::int64_t{3}, std::string("y")});
    for (int i = 0; i < 400; ++i) tf.append(std::span<const std::byte>(rec));
    tf.seal();
    const std::uint32_t k = tf.blocks();
    const auto w = (sm.snapshot() - s0).per_kind[static_cast<std::size_t>(FileKind::Temp)];
    EXPECT_GT(k, 5u);
    EXPECT_EQ(w.writes(), k);
    const Counters s1 = sm.snapshot();
    {
        BlockScan scan = tf.scan_blocks(kSeqOnce);
        int n = 0;
        while (auto c = scan.next())
            while (c->next()) ++n;
        EXPECT_EQ(n, 400);
    }
    EXPECT_EQ((sm.snapshot() - s1).per_kind[static_cast<std::size_t>(FileKind::Temp)].reads(), k);   // releitura física (139)
}

TEST(ScratchSpace, OrdenaEMontaHash) {
    StorageManager sm(test_config("sm_scratch"));
    const Schema s = kv_schema(false, 20);
    auto r = sm.reserve(4);
    ScratchSpace sc = sm.create_scratch(s, r, 2);
    std::uint64_t seed = 5;
    std::map<int, int> count;
    while (true) {
        const int k = static_cast<int>(lcg(seed) % 50);
        const auto rec = encode_record(s, {std::int64_t{k}, std::int64_t{0}, std::int64_t{0}, std::string("z")});
        if (!sc.add(detail::make_view(rec, &s))) break;
        ++count[k];
    }
    EXPECT_GE(sc.size(), sc.capacity());
    sc.sort_by(0);
    for (std::size_t i = 1; i < sc.size(); ++i) EXPECT_LE(sc.at(i - 1).get<std::int32_t>(0), sc.at(i).get<std::int32_t>(0));
    sc.build_hash(0, 2);
    for (const auto& [k, n] : count) {
        auto m = sc.probe(Key{std::int32_t(k)});
        int seen = 0;
        while (m.next()) ++seen;
        EXPECT_EQ(seen, n);
    }
    EXPECT_THROW(sm.create_scratch(s, r, 3), std::runtime_error);   // 2 + 3 > 4 molduras da reserva
}

TEST(StorageManager, ClusteringSobreHashEBPlus) {
    StorageManager sm(test_config("sm_clust"));
    const TableId th = sm.create_table("th", kv_schema(false), PrimarySpec{StructureKind::Hash, 0, 0.7, 7});
    const TableId tb = sm.create_table("tb", kv_schema(false), PrimarySpec{StructureKind::BTree, 0, 0.7});
    sm.create_index(th, IndexSpec{StructureKind::SecBTree, 0, true});
    sm.create_index(tb, IndexSpec{StructureKind::SecBTree, 0, true});
    std::vector<std::vector<std::byte>> rh, rb;
    for (int i = 0; i < 1200; ++i) rh.push_back(kv(sm, th, i % 40, 0, i));
    std::vector<int> sorted_k;
    for (int i = 0; i < 1200; ++i) sorted_k.push_back(i % 40);
    std::sort(sorted_k.begin(), sorted_k.end());
    int u = 0;
    for (int k : sorted_k) rb.push_back(kv(sm, tb, k, 0, u++));
    VecStream vh(std::move(rh)), vb(std::move(rb));
    sm.bulk_load(th, vh);
    sm.bulk_load(tb, vb);
    KeyRange r{0, Key{std::int32_t{3}}, Key{std::int32_t{7}}};
    const auto h = keys_in_order(sm, th, 0, r, PathKind::SecondaryBTreeClustering);
    const auto b = keys_in_order(sm, tb, 0, r, PathKind::SecondaryBTreeClustering);
    EXPECT_EQ(h.size(), 150u);
    EXPECT_EQ(b.size(), 150u);
    EXPECT_TRUE(std::is_sorted(h.begin(), h.end()));
    EXPECT_TRUE(std::is_sorted(b.begin(), b.end()));
    // manutenção: a nova 1ª ocorrência continua achável depois de inserções e remoções
    sm.insert(th, kv(sm, th, 5, 0, 5000));
    sm.erase_key(th, 0, Key{std::int32_t{6}});
    sm.reorganize(th);
    EXPECT_EQ(keys_in_order(sm, th, 0, r, PathKind::SecondaryBTreeClustering).size(), 150u - 30u + 1u);
}

// Conteúdo e índices após manutenção: oráculo independente em memória.
#include "helpers.hpp"
#include "layer3/storage_manager.hpp"
#include "mini_test.hpp"

#include <map>
#include <set>
#include <tuple>

using namespace cbd;

namespace {
using Row = std::tuple<std::int32_t, std::int32_t, std::string>;

class Rows final : public RecordStream {
public:
    explicit Rows(std::vector<std::vector<std::byte>> records) : records_(std::move(records)) {}
    std::optional<std::span<const std::byte>> next() override {
        if (position_ == records_.size()) return std::nullopt;
        return records_[position_++];
    }
private:
    std::vector<std::vector<std::byte>> records_;
    std::size_t position_ = 0;
};

Row read_row(const RecordView& record) {
    const auto text = record.get_var(2);
    return {record.get<std::int32_t>(0), record.get<std::int32_t>(1),
            std::string(reinterpret_cast<const char*>(text.data()), text.size())};
}
}  // namespace

TEST(StorageManager, ManutencaoVariavelPreservaConteudoNosCaminhos) {
    for (auto kind : {StructureKind::Heap, StructureKind::Hash, StructureKind::BTree}) {
        StorageManager sm(test_config("manutencao_conteudo_" + std::to_string(static_cast<int>(kind))));
        const auto table = sm.create_table("t", make_schema({{"k", FieldType::Int32, 0, true},
            {"g", FieldType::Int32}, {"texto", FieldType::VarChar, 120}}), PrimarySpec{kind, 0, 0.7, 3});
        sm.create_index(table, IndexSpec{StructureKind::SecHash, 0, false});
        sm.create_index(table, IndexSpec{StructureKind::SecBTree, 1, false});
        std::map<std::int32_t, Row> expected;
        auto make_row = [&](std::int32_t k) {
            const auto g = k % 7;
            const std::string text(10 + k % 60, static_cast<char>('a' + k % 20));
            expected[k] = Row{k, g, text};
            return sm.encode(table, {std::int64_t{k}, std::int64_t{g}, text});
        };
        std::vector<std::vector<std::byte>> records;
        for (std::int32_t k = 0; k < 500; ++k) records.push_back(make_row(k));
        Rows input(std::move(records));
        sm.bulk_load(table, input);
        auto check = [&] {
            EXPECT_EQ(sm.verify(table), std::string());
            std::multiset<Row> want, got;
            for (const auto& [key, row] : expected) { (void)key; want.insert(row); }
            {
                auto reservation = sm.reserve(1);
                auto scan = sm.scan_blocks(table, kSeqOnce, reservation);
                while (auto block = scan.next())
                    while (auto record = block->next()) got.insert(read_row(*record));
            }
            EXPECT_TRUE(got == want);
            AccessPath hash, btree;
            for (const auto& path : sm.access_paths(table)) {
                if (path.kind == PathKind::SecondaryHash) hash = path;
                if (path.kind == PathKind::SecondaryBTreeNonClustering) btree = path;
            }
            auto reservation = sm.reserve(2);
            for (std::int32_t g = 0; g < 7; ++g) {
                want.clear(); got.clear();
                for (const auto& [key, row] : expected) {
                    (void)key;
                    if (std::get<1>(row) == g) want.insert(row);
                }
                auto cursor = sm.probe(btree, Point{1, Key{g}}, kRandomReuse, reservation);
                while (auto rid = cursor.next()) {
                    auto record = sm.fetch(table, *rid, kRandomOnce, reservation);
                    got.insert(read_row(record.view));
                }
                EXPECT_TRUE(got == want);
            }
            for (std::int32_t k : {0, 17, 499, 520, 545, 5, 15, 495, 999}) {
                auto cursor = sm.probe(hash, Point{0, Key{k}}, kRandomReuse, reservation);
                const auto rid = cursor.next();
                EXPECT_EQ(rid.has_value(), expected.count(k) != 0);
                if (rid && expected.count(k)) {
                    auto record = sm.fetch(table, *rid, kRandomOnce, reservation);
                    EXPECT_TRUE(read_row(record.view) == expected.at(k));
                }
                EXPECT_FALSE(cursor.next().has_value());
            }
        };
        check();
        for (std::int32_t k = 500; k < 550; ++k) sm.insert(table, make_row(k));
        for (std::int32_t k = 5; k < 500; k += 10) {
            EXPECT_EQ(sm.erase_key(table, 0, Key{k}), 1u);
            expected.erase(k);
        }
        check();
        sm.reorganize(table);
        check();
    }
}

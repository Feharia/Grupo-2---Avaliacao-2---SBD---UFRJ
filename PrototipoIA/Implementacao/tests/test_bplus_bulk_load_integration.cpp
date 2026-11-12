// CBD-001: integração BPlusEngine + SlottedPage + BufferPool + FileManager.
// Os testes verificam o comportamento correto e devem falhar antes da correção.
#include "layer3/bplus.hpp"
#include "mini_test.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

using namespace cbd;

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const char* base = std::getenv("CBD_TEST_DIR");
        const auto root = base ? std::filesystem::path(base) : std::filesystem::temp_directory_path();
        std::filesystem::create_directories(root);
        auto pattern = (root / "cbd001-XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr)
            throw std::system_error(errno, std::generic_category(), "mkdtemp CBD-001");
        path_ = pattern;
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

Config config_for(const std::filesystem::path& directory) {
    Config config;
    config.data_dir = directory;
    config.block_size = 4096;
    return config;
}

// Produz chaves 0..rows-1 em ordem, mantendo o registro vivo até next().
class AscendingStream final : public RecordStream {
public:
    AscendingStream(const Schema& schema, std::int64_t rows) : schema_(schema), rows_(rows) {}
    std::optional<std::span<const std::byte>> next() override {
        if (next_ == rows_) return std::nullopt;
        current_ = encode_record(schema_, {next_++, std::string("p")});
        return std::span<const std::byte>(current_);
    }

private:
    const Schema& schema_;
    const std::int64_t rows_;
    std::int64_t next_ = 0;
    std::vector<std::byte> current_;
};

class BulkLoadCase {
    // Declarado primeiro para ser removido somente após fechar pool e arquivos.
    TemporaryDirectory directory_;

public:
    explicit BulkLoadCase(std::int64_t rows)
        : schema(make_schema({{"k", FieldType::Int32, 0, true}, {"pad", FieldType::Char, 123}})),
          pool(config_for(directory_.path())),
          leaves(pool.create("leaves.dat", FileKind::Data)),
          nodes(pool.create("nodes.dat", FileKind::Index)),
          tree(pool, leaves, nodes, KeyCodec::for_field(schema.field(0)),
               [](auto bytes) -> Key { return load<std::int32_t>(bytes.data()); },
               true, PageType::Leaf, schema.record_size) {
        tree.init_empty();
        AscendingStream input(schema, rows);
        tree.bulk_load(input, 0.7);
    }

    const Schema schema;
    BufferPool pool;
    const FileId leaves;
    const FileId nodes;
    BPlusEngine tree;
};

void expect_leaf_occupancy(BulkLoadCase& loaded) {
    const auto capacity = sp_capacity(loaded.pool.block_size(), loaded.schema.record_size);
    const auto minimum = (capacity + 1) / 2;
    ASSERT_EQ(loaded.schema.record_size, 127u);
    ASSERT_EQ(capacity, 31u);
    std::set<std::uint32_t> visited;
    for (auto block = loaded.tree.meta().first_leaf; block != 0;) {
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kSeqOnce, kNoReservation);
        const auto header = read_header(page.read());
        EXPECT_LE(header.live, capacity);
        if (!loaded.tree.meta().root_is_leaf) {
            EXPECT_GE(header.live, minimum);
        }
        block = header.next;
    }
}

void expect_internal_occupancy(BulkLoadCase& loaded) {
    if (loaded.tree.meta().root_is_leaf) return;
    const auto max_keys = loaded.tree.internal_cap();
    // Mínimo de filhos = ceil((max_keys + 1) / 2); chaves = filhos - 1.
    const auto min_keys = (max_keys + 2) / 2 - 1;
    ASSERT_EQ(max_keys, 509u);
    std::vector<std::uint32_t> pending{loaded.tree.meta().root};
    std::set<std::uint32_t> visited;
    while (!pending.empty()) {
        const auto block = pending.back();
        pending.pop_back();
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.nodes, block}, kSeqOnce, kNoReservation);
        const auto node = read_internal(page.read(), loaded.tree.codec());
        EXPECT_LE(node.keys.size(), max_keys);
        EXPECT_EQ(node.children.size(), node.keys.size() + 1);
        if (block == loaded.tree.meta().root) {
            EXPECT_GE(node.keys.size(), 1u);
        } else {
            EXPECT_GE(node.keys.size(), min_keys);
        }
        if (node.level > 1) pending.insert(pending.end(), node.children.begin(), node.children.end());
    }
}

// Oráculo independente de verify(): confere ordem, quantidade e bytes dos registros.
void expect_records(BulkLoadCase& loaded, std::int64_t rows) {
    std::int64_t seen = 0;
    std::set<std::uint32_t> visited;
    for (auto block = loaded.tree.meta().first_leaf; block != 0;) {
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kSeqOnce, kNoReservation);
        const auto slots = sorted_slots(page.read(), loaded.tree.key_of());
        EXPECT_EQ(slots.size(), read_header(page.read()).live);
        for (const auto& entry : slots) {
            ASSERT_TRUE(seen < rows);
            const auto record = sp_record(page.read(), entry.second);
            const auto expected = encode_record(loaded.schema, {seen, std::string("p")});
            EXPECT_TRUE(std::equal(record.begin(), record.end(), expected.begin(), expected.end()));
            ++seen;
        }
        block = read_header(page.read()).next;
    }
    EXPECT_EQ(seen, rows);
    if (rows == 0) return;
    // Também exercita a descida pelos separadores, além da cadeia de folhas.
    for (const auto key : {std::int64_t{0}, rows / 2, rows - 1}) {
        const auto rid = loaded.tree.find_first(Key{static_cast<std::int32_t>(key)});
        ASSERT_TRUE(rid.has_value());
        auto page = loaded.pool.fetch({loaded.leaves, rid->block}, kRandomReuse, kNoReservation);
        const auto record = sp_record(page.read(), rid->slot);
        const auto expected = encode_record(loaded.schema, {key, std::string("p")});
        EXPECT_TRUE(std::equal(record.begin(), record.end(), expected.begin(), expected.end()));
    }
}

}  // namespace

TEST(BPlusBulkLoadIntegration, FolhasRespeitamMinimoCom22Registros) {
    BulkLoadCase loaded(22);
    expect_leaf_occupancy(loaded);
    expect_records(loaded, 22);
    EXPECT_TRUE(loaded.tree.verify(true).empty());
}

TEST(BPlusBulkLoadIntegration, InternosRespeitamMinimoCom7500Registros) {
    BulkLoadCase loaded(7500);
    ASSERT_FALSE(loaded.tree.meta().root_is_leaf);
    expect_internal_occupancy(loaded);
    expect_leaf_occupancy(loaded);
    expect_records(loaded, 7500);
    EXPECT_TRUE(loaded.tree.verify(true).empty());
}

TEST(BPlusBulkLoadIntegration, VerifyRejeitaUltimaFolhaSubocupada) {
    BulkLoadCase loaded(42);  // Controle inicialmente válido, sem cauda subocupada.
    ASSERT_FALSE(loaded.tree.meta().root_is_leaf);
    ASSERT_TRUE(loaded.tree.verify(true).empty());
    const auto minimum = (sp_capacity(loaded.pool.block_size(), loaded.schema.record_size) + 1) / 2;
    auto block = loaded.tree.meta().first_leaf;
    std::set<std::uint32_t> visited;
    for (;;) {
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kRandomReuse, kNoReservation);
        const auto header = read_header(page.read());
        if (header.next != 0) {
            block = header.next;
            continue;
        }
        const auto slots = sorted_slots(page.read(), loaded.tree.key_of());
        ASSERT_TRUE(slots.size() >= minimum);
        // Corrupção dirigida: preserva chaves iniciais, links e separadores;
        // deixa apenas a ocupação da última folha abaixo do mínimo.
        for (std::size_t i = minimum - 1; i < slots.size(); ++i)
            sp_erase(page.write(), slots[i].second);
        ASSERT_EQ(read_header(page.read()).live, minimum - 1);
        break;
    }
    EXPECT_TRUE(loaded.tree.verify(false).empty());
    EXPECT_FALSE(loaded.tree.verify(true).empty());
}

TEST(BPlusBulkLoadIntegration, RaizFolhaPodeFicarAbaixoDoMinimo) {
    for (const auto rows : {0, 1, 15}) {
        BulkLoadCase loaded(rows);
        ASSERT_TRUE(loaded.tree.meta().root_is_leaf);
        expect_leaf_occupancy(loaded);
        expect_records(loaded, rows);
        EXPECT_TRUE(loaded.tree.verify(true).empty());
    }
}

TEST(BPlusBulkLoadIntegration, DuasFolhasValidasPreservamOsRegistros) {
    BulkLoadCase loaded(42);
    ASSERT_FALSE(loaded.tree.meta().root_is_leaf);
    expect_leaf_occupancy(loaded);
    expect_internal_occupancy(loaded);
    expect_records(loaded, 42);
    EXPECT_TRUE(loaded.tree.verify(true).empty());
}

TEST(BPlusBulkLoadIntegration, FolhasRespeitamMinimoNosLimitesAdjacentes) {
    for (const auto rows : {21, 23, 31, 32, 33, 42, 43}) {
#if defined(CBD_USE_GTEST)
        SCOPED_TRACE("registros=" + std::to_string(rows));
#endif
        BulkLoadCase loaded(rows);
        expect_leaf_occupancy(loaded);
        expect_records(loaded, rows);
        EXPECT_TRUE(loaded.tree.verify(true).empty());
    }
}

TEST(BPlusBulkLoadIntegration, InternosRespeitamMinimoNosLimitesAdjacentes) {
    for (const auto rows : {7499, 7501}) {
#if defined(CBD_USE_GTEST)
        SCOPED_TRACE("registros=" + std::to_string(rows));
#endif
        BulkLoadCase loaded(rows);
        expect_internal_occupancy(loaded);
        expect_leaf_occupancy(loaded);
        expect_records(loaded, rows);
        EXPECT_TRUE(loaded.tree.verify(true).empty());
    }
}

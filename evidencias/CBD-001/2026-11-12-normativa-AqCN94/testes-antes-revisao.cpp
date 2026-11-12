// CBD-001: integração BPlusEngine + SlottedPage + BufferPool + FileManager.
// Os testes verificam o comportamento correto e devem falhar antes da correção.
#include "layer3/bplus.hpp"
#include "layer3/storage_manager.hpp"
#include "mini_test.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <utility>
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
    AscendingStream(const Schema& schema, std::int64_t rows, std::string pad = "p")
        : schema_(schema), rows_(rows), pad_(std::move(pad)) {}
    std::optional<std::span<const std::byte>> next() override {
        if (next_ == rows_) return std::nullopt;
        current_ = encode_record(schema_, {next_++, pad_});
        return std::span<const std::byte>(current_);
    }

private:
    const Schema& schema_;
    const std::int64_t rows_;
    const std::string pad_;
    std::int64_t next_ = 0;
    std::vector<std::byte> current_;
};

class BulkLoadCase {
    // Declarado primeiro para ser removido somente após fechar pool e arquivos.
    TemporaryDirectory directory_;

public:
    explicit BulkLoadCase(std::int64_t rows, double fill = 0.7, bool variable = false)
        : schema(make_schema({{"k", FieldType::Int32, 0, true},
                              {"pad", variable ? FieldType::VarChar : FieldType::Char, 123}})),
          pad(variable ? std::string(121, 'p') : "p"),
          pool(config_for(directory_.path())),
          leaves(pool.create("leaves.dat", FileKind::Data)),
          nodes(pool.create("nodes.dat", FileKind::Index)),
          tree(pool, leaves, nodes, KeyCodec::for_field(schema.field(0)),
               [](auto bytes) -> Key { return load<std::int32_t>(bytes.data()); },
               true, PageType::Leaf, variable ? 0u : schema.record_size) {
        tree.init_empty();
        AscendingStream input(schema, rows, pad);
        tree.bulk_load(input, fill);
    }

    const Schema schema;
    const std::string pad;
    BufferPool pool;
    const FileId leaves;
    const FileId nodes;
    BPlusEngine tree;
};

void expect_leaf_occupancy(BulkLoadCase& loaded) {
    const auto capacity = sp_capacity(loaded.pool.block_size(), loaded.schema.record_size);
    const auto minimum = (capacity + 1) / 2;
    if (!loaded.schema.has_var()) {
        ASSERT_EQ(loaded.schema.record_size, 127u);
        ASSERT_EQ(capacity, 31u);
    }
    std::set<std::uint32_t> visited;
    for (auto block = loaded.tree.meta().first_leaf; block != 0;) {
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kSeqOnce, kNoReservation);
        const auto header = read_header(page.read());
        if (loaded.schema.has_var()) {
            const auto used = sp_used_bytes(page.read());
            EXPECT_LE(used, loaded.pool.block_size() - kPageHeader);
            if (!loaded.tree.meta().root_is_leaf) {
                EXPECT_GE(used * 2, loaded.pool.block_size() - kPageHeader);
            }
        } else if (!loaded.tree.meta().root_is_leaf) {
            EXPECT_LE(header.live, capacity);
            EXPECT_GE(header.live, minimum);
        } else {
            EXPECT_LE(header.live, capacity);
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
            const auto expected = encode_record(loaded.schema, {seen, loaded.pad});
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
        const auto expected = encode_record(loaded.schema, {key, loaded.pad});
        EXPECT_TRUE(std::equal(record.begin(), record.end(), expected.begin(), expected.end()));
    }
}

// Tamanhos escolhidos à mão para reproduzir fronteiras de páginas variáveis.
void expect_variable_sizes(const std::vector<std::size_t>& lengths) {
    TemporaryDirectory directory;
    const auto schema = make_schema({{"k", FieldType::Int32, 0, true}, {"pad", FieldType::VarChar, 2100}});
    BufferPool pool(config_for(directory.path()));
    const auto leaves = pool.create("variable.dat", FileKind::Data);
    const auto nodes = pool.create("variable.idx", FileKind::Index);
    BPlusEngine tree(pool, leaves, nodes, KeyCodec::for_field(schema.field(0)),
                     [](auto bytes) -> Key { return load<std::int32_t>(bytes.data()); },
                     true, PageType::Leaf, 0);
    class SizedStream final : public RecordStream {
    public:
        SizedStream(const Schema& schema, const std::vector<std::size_t>& lengths)
            : schema_(schema), lengths_(lengths) {}
        std::optional<std::span<const std::byte>> next() override {
            if (next_ == lengths_.size()) return std::nullopt;
            record_ = encode_record(schema_, {static_cast<std::int64_t>(next_), std::string(lengths_[next_], 'p')});
            ++next_;
            return std::span<const std::byte>(record_);
        }
    private:
        const Schema& schema_;
        const std::vector<std::size_t>& lengths_;
        std::size_t next_ = 0;
        std::vector<std::byte> record_;
    } input(schema, lengths);
    tree.init_empty();
    tree.bulk_load(input, 0.7);
    EXPECT_TRUE(tree.verify(true).empty());
    std::size_t seen = 0;
    for (auto block = tree.meta().first_leaf; block != 0;) {
        auto page = pool.fetch({leaves, block}, kSeqOnce, kNoReservation);
        if (!tree.meta().root_is_leaf) {
            EXPECT_GE(sp_used_bytes(page.read()), 2036u);
        }
        for (const auto& entry : sorted_slots(page.read(), tree.key_of())) {
            ASSERT_TRUE(seen < lengths.size());
            const auto expected = encode_record(schema, {static_cast<std::int64_t>(seen), std::string(lengths[seen], 'p')});
            const auto actual = sp_record(page.read(), entry.second);
            EXPECT_TRUE(std::equal(actual.begin(), actual.end(), expected.begin(), expected.end()));
            ++seen;
        }
        block = read_header(page.read()).next;
    }
    EXPECT_EQ(seen, lengths.size());
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

TEST(BPlusBulkLoadIntegration, InternosRespeitamMinimoSemCaudaSubocupada) {
    BulkLoadCase loaded(7518);  // 358 folhas completas na meta de 21 registros.
    expect_leaf_occupancy(loaded);
    expect_internal_occupancy(loaded);
    expect_records(loaded, 7518);
    EXPECT_TRUE(loaded.tree.verify(true).empty());
}

TEST(BPlusBulkLoadIntegration, InternosNaoRaizRespeitamMinimoEmTresNiveis) {
    BulkLoadCase loaded(10731);  // 511 folhas: excede os 510 filhos de uma raiz.
    ASSERT_TRUE(loaded.tree.meta().height >= 3);
    expect_leaf_occupancy(loaded);
    expect_internal_occupancy(loaded);
    expect_records(loaded, 10731);
    EXPECT_TRUE(loaded.tree.verify(true).empty());
}

TEST(BPlusBulkLoadIntegration, PreenchimentosExtremosRespeitamMinimo) {
    for (const auto fill : {0.1, 0.5, 1.0}) {
        for (const auto rows : {22, 32, 65}) {
#if defined(CBD_USE_GTEST)
            SCOPED_TRACE("fill=" + std::to_string(fill) + "; registros=" + std::to_string(rows));
#endif
            BulkLoadCase loaded(rows, fill);
            expect_leaf_occupancy(loaded);
            expect_internal_occupancy(loaded);
            expect_records(loaded, rows);
            EXPECT_TRUE(loaded.tree.verify(true).empty());
        }
    }
}

TEST(BPlusBulkLoadIntegration, RegistrosVariaveisRespeitamMinimoEmBytes) {
    for (const auto fill : {0.1, 0.7}) {
        for (const auto rows : {22, 32, 43}) {
            BulkLoadCase loaded(rows, fill, true);
            expect_leaf_occupancy(loaded);
            expect_records(loaded, rows);
            EXPECT_TRUE(loaded.tree.verify(true).empty());
        }
    }
}

TEST(BPlusBulkLoadIntegration, CargaEReorganizacaoPreservamIndiceSecundario) {
    TemporaryDirectory directory;
    StorageManager sm(config_for(directory.path()));
    const auto schema = make_schema({{"k", FieldType::Int32, 0, true}, {"pad", FieldType::Char, 123}});
    const auto table = sm.create_table("indexed", schema, PrimarySpec{StructureKind::BTree, 0, 0.7});
    sm.create_index(table, IndexSpec{StructureKind::SecHash, 0, false});
    AscendingStream input(schema, 22);
    sm.bulk_load(table, input);
    for (int pass = 0; pass < 2; ++pass) {
        EXPECT_TRUE(sm.verify(table).empty());
        AccessPath secondary;
        bool found = false;
        for (const auto& path : sm.access_paths(table)) {
            if (path.kind == PathKind::SecondaryHash) {
                secondary = path;
                found = true;
            }
        }
        ASSERT_TRUE(found);
        {
            auto reservation = sm.reserve(2);
            for (std::int32_t key = 0; key < 22; ++key) {
                auto cursor = sm.probe(secondary, Point{0, Key{key}}, kRandomReuse);
                const auto rid = cursor.next();
                ASSERT_TRUE(rid.has_value());
                auto record = sm.fetch(table, *rid, kRandomOnce, reservation);
                EXPECT_EQ(record.view.get<std::int32_t>(0), key);
                EXPECT_FALSE(cursor.next().has_value());
            }
        }
        // A reserva deve ser liberada antes da reorganização.
        if (pass == 0) sm.reorganize(table);
    }
}

TEST(BPlusBulkLoadIntegration, VariaveisDesiguaisRepartemMaisDeDuasFolhas) {
    std::vector<std::size_t> lengths(64, 90);
    std::fill(lengths.begin() + 1, lengths.begin() + 16, 121);
    // Partição válida em bytes, incluindo slots: 2665, 2100, 2100.
    expect_variable_sizes(lengths);
}

TEST(BPlusBulkLoadIntegration, VariaveisNaoDeixamFolhaIntermediariaSubocupada) {
    // Partição válida em bytes, incluindo slots: 3800, 3100.
    expect_variable_sizes({1390, 1390, 990, 990, 2090});
}

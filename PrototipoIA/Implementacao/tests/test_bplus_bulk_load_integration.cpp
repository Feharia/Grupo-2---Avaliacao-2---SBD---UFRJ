// CBD-001: integração BPlusEngine + SlottedPage + BufferPool + FileManager.
// Requisitos, decisões e fixtures: docs/cbd001/CONTRATO_E_RASTREABILIDADE.md.
// Regressões estruturais falham na baseline; controles podem passar nela.
#include "layer3/bplus.hpp"
#include "layer3/storage_manager.hpp"
#include "cbd001_spec_fixture.hpp"
#include "mini_test.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace cbd;
namespace spec = cbd001_spec;

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
    config.block_size = spec::block_bytes;
    return config;
}

// Produz chaves 0..rows-1 em ordem, mantendo o registro vivo até next().
class AscendingStream final : public RecordStream {
public:
    explicit AscendingStream(std::int64_t rows, std::string pad = "p", bool variable = false)
        : rows_(rows), pad_(std::move(pad)), variable_(variable) {}
    std::optional<std::span<const std::byte>> next() override {
        if (next_ == rows_) return std::nullopt;
        current_ = spec::record(next_++, pad_, variable_);
        return std::span<const std::byte>(current_);
    }

private:
    const std::int64_t rows_;
    const std::string pad_;
    const bool variable_;
    std::int64_t next_ = 0;
    std::vector<std::byte> current_;
};

class BulkLoadCase {
    // Declarado primeiro para ser removido somente após fechar pool e arquivos.
    TemporaryDirectory directory_;

public:
    explicit BulkLoadCase(std::int64_t rows, double fill = 0.7, bool variable = false,
                          std::optional<std::string> padding = std::nullopt)
        : schema(make_schema({{"k", FieldType::Int32, 0, true},
                              {"pad", variable ? FieldType::VarChar : FieldType::Char, 123}})),
          pad(padding.value_or(variable ? std::string(121, 'p') : "p")), has_variable(variable),
          pool(config_for(directory_.path())),
          leaves(pool.create("leaves.dat", FileKind::Data)),
          nodes(pool.create("nodes.dat", FileKind::Index)),
          tree(pool, leaves, nodes, KeyCodec::for_field(schema.field(0)),
               [](auto bytes) -> Key { return spec::native_at<std::int32_t>(bytes, 0); },
               true, PageType::Leaf, variable ? 0u : schema.record_size) {
        tree.init_empty();
        AscendingStream input(rows, pad, variable);
        tree.bulk_load(input, fill);
    }

    const Schema schema;
    const std::string pad;
    const bool has_variable;
    BufferPool pool;
    const FileId leaves;
    const FileId nodes;
    BPlusEngine tree;
};

void expect_leaf_occupancy(BulkLoadCase& loaded) {
    // D1/D2/D4 e configuração física declarados no contrato, sem capacidades do motor.
    if (!loaded.has_variable) {
        ASSERT_EQ(loaded.schema.record_size, spec::fixed_record_bytes);
    }
    std::set<std::uint32_t> visited;
    std::uint32_t previous = 0;
    for (auto block = loaded.tree.meta().first_leaf; block != 0;) {
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kSeqOnce, kNoReservation);
        const auto observed = spec::observe_leaf(page.read());
        EXPECT_EQ(observed.prev, previous);
        const bool root_leaf = loaded.tree.meta().root_is_leaf;
        if (root_leaf) {
            EXPECT_EQ(block, loaded.tree.meta().root);
            EXPECT_EQ(previous, 0u);
            EXPECT_EQ(observed.next, 0u);  // A exceção não pode esconder outras folhas.
        }
        if (loaded.has_variable) {
            EXPECT_LE(observed.used_bytes(), spec::useful_bytes);
            if (!root_leaf) {
                EXPECT_GE(observed.used_bytes(), spec::min_variable_bytes);
            }
        } else {
            EXPECT_LE(observed.records.size(), spec::max_leaf_records);
            if (!root_leaf) {
                EXPECT_GE(observed.records.size(), spec::min_leaf_records);
            }
        }
        previous = block;
        block = observed.next;
    }
}

void expect_internal_occupancy(BulkLoadCase& loaded) {
    if (loaded.tree.meta().root_is_leaf) return;
    // D3: limites derivados no contrato, sem internal_cap/read_internal do motor.
    ASSERT_TRUE(loaded.tree.meta().height >= 2);
    std::vector<std::pair<std::uint32_t, std::uint16_t>> pending{
        {loaded.tree.meta().root, static_cast<std::uint16_t>(loaded.tree.meta().height - 1)}};
    std::set<std::uint32_t> visited;
    std::set<std::uint32_t> leaf_children;
    while (!pending.empty()) {
        const auto [block, expected_level] = pending.back();
        pending.pop_back();
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.nodes, block}, kSeqOnce, kNoReservation);
        const auto node = spec::observe_internal(page.read());
        EXPECT_EQ(node.level, expected_level);
        ASSERT_TRUE(node.level >= 1);
        EXPECT_TRUE(std::is_sorted(node.keys.begin(), node.keys.end()));
        EXPECT_LE(node.keys.size(), spec::max_internal_keys);
        if (block == loaded.tree.meta().root) {
            EXPECT_GE(node.keys.size(), 1u);
        } else {
            EXPECT_GE(node.keys.size(), spec::min_internal_keys);
        }
        for (const auto child : node.children) EXPECT_NE(child, 0u);
        for (const auto child : node.children) {
            if (node.level > 1) {
                pending.emplace_back(child, static_cast<std::uint16_t>(node.level - 1));
            } else {
                ASSERT_TRUE(leaf_children.insert(child).second);
            }
        }
    }
    std::set<std::uint32_t> leaf_chain;
    for (auto block = loaded.tree.meta().first_leaf; block != 0;) {
        ASSERT_TRUE(leaf_chain.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kSeqOnce, kNoReservation);
        block = spec::observe_leaf(page.read()).next;
    }
    EXPECT_EQ(leaf_children, leaf_chain);
}

// Oráculo de fixture independente dos codecs/páginas/capacidades de produção.
void expect_records(BulkLoadCase& loaded, std::int64_t rows) {
    std::int64_t seen = 0;
    std::set<std::uint32_t> visited;
    for (auto block = loaded.tree.meta().first_leaf; block != 0;) {
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kSeqOnce, kNoReservation);
        const auto observed = spec::observe_leaf(page.read());
        for (const auto& entry : observed.records) {
            ASSERT_TRUE(seen < rows);
            const auto expected = spec::record(seen, loaded.pad, loaded.has_variable);
            EXPECT_EQ(entry.key, seen);
            EXPECT_TRUE(std::equal(entry.bytes.begin(), entry.bytes.end(), expected.begin(), expected.end()));
            ++seen;
        }
        block = observed.next;
    }
    EXPECT_EQ(seen, rows);
    if (rows == 0) return;
    // Também exercita a descida pelos separadores, além da cadeia de folhas.
    for (const auto key : {std::int64_t{0}, rows / 2, rows - 1}) {
        const auto rid = loaded.tree.find_first(Key{static_cast<std::int32_t>(key)});
        ASSERT_TRUE(rid.has_value());
        auto page = loaded.pool.fetch({loaded.leaves, rid->block}, kRandomReuse, kNoReservation);
        const auto observed = spec::observe_leaf(page.read());
        const auto found = std::find_if(observed.records.begin(), observed.records.end(),
                                        [&](const auto& record) { return record.slot == rid->slot; });
        ASSERT_TRUE(found != observed.records.end());
        const auto expected = spec::record(key, loaded.pad, loaded.has_variable);
        EXPECT_TRUE(std::equal(found->bytes.begin(), found->bytes.end(), expected.begin(), expected.end()));
    }
}

// Tamanhos escolhidos à mão para reproduzir fronteiras de páginas variáveis.
void expect_variable_sizes(const std::vector<std::size_t>& lengths, bool reject = false) {
    TemporaryDirectory directory;
    const auto schema = make_schema({{"k", FieldType::Int32, 0, true}, {"pad", FieldType::VarChar, 4090}});
    BufferPool pool(config_for(directory.path()));
    const auto leaves = pool.create("variable.dat", FileKind::Data);
    const auto nodes = pool.create("variable.idx", FileKind::Index);
    BPlusEngine tree(pool, leaves, nodes, KeyCodec::for_field(schema.field(0)),
                     [](auto bytes) -> Key { return spec::native_at<std::int32_t>(bytes, 0); },
                     true, PageType::Leaf, 0);
    class SizedStream final : public RecordStream {
    public:
        explicit SizedStream(const std::vector<std::size_t>& lengths) : lengths_(lengths) {}
        std::optional<std::span<const std::byte>> next() override {
            if (next_ == lengths_.size()) return std::nullopt;
            record_ = spec::record(static_cast<std::int64_t>(next_), std::string(lengths_[next_], 'p'), true);
            ++next_;
            return std::span<const std::byte>(record_);
        }
    private:
        const std::vector<std::size_t>& lengths_;
        std::size_t next_ = 0;
        std::vector<std::byte> record_;
    } input(lengths);
    tree.init_empty();
    if (reject) {
        bool rejected = false;
        try {
            tree.bulk_load(input, 0.7);
        } catch (const std::bad_alloc&) {
            throw;  // Falta de memória não comprova rejeição por inviabilidade.
        } catch (const std::out_of_range&) {
            throw;  // Erro de limites também não é uma rejeição semântica.
        } catch (const std::exception&) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
        return;
    }
    tree.bulk_load(input, 0.7);
    EXPECT_TRUE(tree.verify(true).empty());
    std::size_t seen = 0;
    for (auto block = tree.meta().first_leaf; block != 0;) {
        auto page = pool.fetch({leaves, block}, kSeqOnce, kNoReservation);
        const auto observed = spec::observe_leaf(page.read());
        if (!tree.meta().root_is_leaf) {
            EXPECT_GE(observed.used_bytes(), spec::min_variable_bytes);
        }
        for (const auto& entry : observed.records) {
            ASSERT_TRUE(seen < lengths.size());
            const auto expected = spec::record(static_cast<std::int64_t>(seen), std::string(lengths[seen], 'p'), true);
            EXPECT_TRUE(std::equal(entry.bytes.begin(), entry.bytes.end(), expected.begin(), expected.end()));
            ++seen;
        }
        block = observed.next;
    }
    EXPECT_EQ(seen, lengths.size());
}

using LogicalRow = std::pair<std::int32_t, std::string>;
using LogicalRows = std::vector<LogicalRow>;

class ModelStream final : public RecordStream {
public:
    explicit ModelStream(const LogicalRows& rows) : rows_(rows) {}
    std::optional<std::span<const std::byte>> next() override {
        if (next_ == rows_.size()) return std::nullopt;
        const auto& row = rows_[next_++];
        bytes_ = spec::record(row.first, row.second);
        return std::span<const std::byte>(bytes_);
    }
private:
    const LogicalRows& rows_;
    std::size_t next_ = 0;
    std::vector<std::byte> bytes_;
};

AccessPath primary_path(const StorageManager& sm, TableId table) {
    for (const auto& path : sm.access_paths(table)) {
        if (path.kind == PathKind::PrimaryBTree) return path;
    }
    throw std::logic_error("contrato CBD-001: caminho público B+ ausente");
}

LogicalRows observed_rows(StorageManager& sm, TableId table, const Predicate& predicate) {
    auto reservation = sm.reserve(2);
    auto cursor = sm.find(primary_path(sm, table), predicate, kSeqOnce, reservation);
    LogicalRows rows;
    while (const auto record = cursor.next()) {
        // RecordView é o resultado observado do SUT; os esperados são valores do modelo.
        rows.emplace_back(record->get<std::int32_t>(0), std::get<std::string>(record->key(1)));
    }
    return rows;
}

KeyRange all_keys() {
    KeyRange range;
    range.field = 0;
    return range;
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
    auto block = loaded.tree.meta().first_leaf;
    std::set<std::uint32_t> visited;
    for (;;) {
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kRandomReuse, kNoReservation);
        const auto observed = spec::observe_leaf(page.read());
        if (observed.next != 0) {
            block = observed.next;
            continue;
        }
        ASSERT_TRUE(observed.records.size() >= spec::min_leaf_records);
        // Corrupção dirigida: preserva chaves iniciais, links e separadores;
        // deixa apenas a ocupação da última folha abaixo do mínimo.
        for (std::size_t i = 15; i < observed.records.size(); ++i)
            spec::remove_slot(page.write(), observed.records[i].slot);
        ASSERT_EQ(spec::observe_leaf(page.read()).records.size(), 15u);
        break;
    }
    EXPECT_TRUE(loaded.tree.verify(false).empty());
    EXPECT_FALSE(loaded.tree.verify(true).empty());
}

TEST(BPlusBulkLoadIntegration, VerifyRejeitaUltimaFolhaVariavelSubocupadaEmBytes) {
    BulkLoadCase loaded(42, 0.7, true, std::string(90, 'p'));
    ASSERT_FALSE(loaded.tree.meta().root_is_leaf);
    ASSERT_TRUE(loaded.tree.verify(true).empty());
    auto block = loaded.tree.meta().first_leaf;
    std::set<std::uint32_t> visited;
    for (;;) {
        ASSERT_TRUE(visited.insert(block).second);
        auto page = loaded.pool.fetch({loaded.leaves, block}, kRandomReuse, kNoReservation);
        const auto observed = spec::observe_leaf(page.read());
        if (observed.next != 0) {
            block = observed.next;
            continue;
        }
        ASSERT_TRUE(observed.records.size() >= 21);
        for (std::size_t i = 20; i < observed.records.size(); ++i)
            spec::remove_slot(page.write(), observed.records[i].slot);
        const auto corrupted = spec::observe_leaf(page.read());
        ASSERT_EQ(corrupted.records.size(), 20u);  // Acima do mínimo fixo16.
        ASSERT_EQ(corrupted.used_bytes(), 2000u);  // Abaixo do mínimo variável2036.
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
    // 15811 > 31*510: três níveis são necessários mesmo com folhas a 100%.
    BulkLoadCase loaded(15811);
    ASSERT_TRUE(loaded.tree.meta().height >= 3);
    expect_leaf_occupancy(loaded);
    expect_internal_occupancy(loaded);
    expect_records(loaded, 15811);
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
    AscendingStream input(22);
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

TEST(BPlusBulkLoadIntegration, VariaveisSemParticaoValidaSaoRejeitados) {
    // D6: custo com slot 1000/1000/2100; total4100 e nenhum corte com ambos >=2036.
    expect_variable_sizes({990, 990, 2090}, true);
    // Um único registro custa4100 bytes, acima da capacidade útil4072.
    expect_variable_sizes({4090}, true);
}

TEST(StorageBPlusContract, CargaPreservaLinhasEBuscasUnicas) {
    for (const auto count : {0, 1, 22, 32, 7518, 15811}) {
        TemporaryDirectory directory;
        StorageManager sm(config_for(directory.path()));
        const auto schema = make_schema({{"k", FieldType::Int32, 0, true}, {"pad", FieldType::Char, 123}});
        const auto table = sm.create_table("unique_model", schema, PrimarySpec{StructureKind::BTree, 0, 0.7});
        LogicalRows input_rows;
        std::map<std::int32_t, std::string> model;
        for (std::int32_t key = 0; key < count; ++key) {
            const auto value = "row:" + std::to_string(key);
            model.emplace(key, value);
            input_rows.emplace_back(key, value);
        }
        ModelStream input(input_rows);
        sm.bulk_load(table, input);
        const LogicalRows expected(model.begin(), model.end());
        EXPECT_EQ(observed_rows(sm, table, all_keys()), expected);
        for (const auto& row : model) {
            EXPECT_EQ(observed_rows(sm, table, Point{0, Key{row.first}}), LogicalRows({row}));
        }
        EXPECT_TRUE(observed_rows(sm, table, Point{0, Key{std::int32_t{-1}}}).empty());
        EXPECT_TRUE(observed_rows(sm, table, Point{0, Key{static_cast<std::int32_t>(count)}}).empty());
    }
}

TEST(StorageBPlusContract, BuscaPorFaixaRespeitaLimites) {
    TemporaryDirectory directory;
    StorageManager sm(config_for(directory.path()));
    const auto schema = make_schema({{"k", FieldType::Int32, 0, true}, {"pad", FieldType::Char, 123}});
    const auto table = sm.create_table("range_model", schema, PrimarySpec{StructureKind::BTree, 0, 0.7});
    LogicalRows rows;
    for (std::int32_t key = 0; key < 128; key += 2) rows.emplace_back(key, "row:" + std::to_string(key));
    ModelStream input(rows);
    sm.bulk_load(table, input);
    LogicalRows expected;
    for (const auto key : {20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42})
        expected.emplace_back(key, "row:" + std::to_string(key));
    KeyRange range;
    range.field = 0;
    range.lo = Key{std::int32_t{20}};
    range.hi = Key{std::int32_t{42}};
    EXPECT_EQ(observed_rows(sm, table, range), expected);
}

TEST(StorageBPlusContract, OrganizacaoNaoUnicaPreservaDuplicatas) {
    TemporaryDirectory directory;
    StorageManager sm(config_for(directory.path()));
    const auto schema = make_schema({{"k", FieldType::Int32}, {"pad", FieldType::Char, 123}});
    const auto table = sm.create_table("duplicates_model", schema, PrimarySpec{StructureKind::BTree, 0, 0.7});
    LogicalRows rows;
    std::multiset<LogicalRow> expected;
    for (std::int32_t key = 0; key < 8; ++key) {
        for (int duplicate = 0; duplicate < 6; ++duplicate) {
            const LogicalRow row{key, "dup:" + std::to_string(key) + ":" + std::to_string(duplicate)};
            rows.push_back(row);
            expected.insert(row);
        }
    }
    ModelStream input(rows);
    sm.bulk_load(table, input);
    for (int pass = 0; pass < 2; ++pass) {
        const auto actual = observed_rows(sm, table, all_keys());
        EXPECT_EQ(actual.size(), 48u);
        EXPECT_TRUE(std::is_sorted(actual.begin(), actual.end(),
                                   [](const auto& a, const auto& b) { return a.first < b.first; }));
        EXPECT_EQ(std::multiset<LogicalRow>(actual.begin(), actual.end()), expected);
        const auto point = observed_rows(sm, table, Point{0, Key{std::int32_t{3}}});
        std::multiset<LogicalRow> expected_point;
        for (const auto& row : expected) {
            if (row.first == 3) expected_point.insert(row);
        }
        EXPECT_EQ(point.size(), 6u);
        EXPECT_EQ(std::multiset<LogicalRow>(point.begin(), point.end()), expected_point);
        if (pass == 0) sm.reorganize(table);
    }
}

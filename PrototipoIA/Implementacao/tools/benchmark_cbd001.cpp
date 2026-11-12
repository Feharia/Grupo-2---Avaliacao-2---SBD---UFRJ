// Driver comum às duas versões do motor. Compilar contra cada libcbd_motor.a.
#include "layer3/bplus.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace cbd;

namespace {
std::string padding(std::int64_t key, std::int64_t rows, bool variable) {
    if (!variable) return "p";
    if (rows == 5) {
        constexpr std::size_t lengths[]{1390, 1390, 990, 990, 2090};
        return std::string(lengths[key], 'p');
    }
    if (rows == 64) return std::string(key >= 1 && key <= 15 ? 121 : 90, 'p');
    return std::string(121, 'p');
}

class Input final : public RecordStream {
public:
    Input(const Schema& schema, std::int64_t rows, bool variable)
        : schema_(schema), rows_(rows), variable_(variable) {}
    std::optional<std::span<const std::byte>> next() override {
        if (next_ == rows_) return std::nullopt;
        record_ = encode_record(schema_, {next_, padding(next_, rows_, variable_)});
        ++next_;
        return std::span<const std::byte>(record_);
    }
private:
    const Schema& schema_;
    std::int64_t rows_, next_ = 0;
    const bool variable_;
    std::vector<std::byte> record_;
};

struct Measurement {
    double microseconds;
    IoCounts io;
};

template <class Operation>
Measurement measure(BufferPool& pool, Operation operation) {
    const auto before = pool.snapshot();
    const auto begin = std::chrono::steady_clock::now();
    operation();
    const auto end = std::chrono::steady_clock::now();
    return {std::chrono::duration<double, std::micro>(end - begin).count(),
            (pool.snapshot() - before).global};
}

// Não depende da omissão de verify(true) da baseline na última folha.
bool occupancy_ok(BufferPool& pool, const BPlusEngine& tree, std::uint32_t record_size, bool variable) {
    const auto minimum = (sp_capacity(pool.block_size(), record_size) + 1) / 2;
    for (auto block = tree.meta().first_leaf; block != 0;) {
        auto page = pool.fetch({tree.leaf_file(), block}, kSeqOnce, kNoReservation);
        const auto header = read_header(page.read());
        if (!tree.meta().root_is_leaf && (variable
            ? sp_used_bytes(page.read()) * 2 < pool.block_size() - kPageHeader
            : header.live < minimum)) return false;
        block = header.next;
    }
    return tree.verify(true).empty();
}

void print_measurement(const Measurement& m) {
    std::cout << ',' << m.microseconds << ',' << m.io.requests << ','
              << m.io.reads() << ',' << m.io.writes();
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument("uso: benchmark_cbd001 DIR ROWS fixed|variable");
        const bool variable = std::string(argv[3]) == "variable";
        if (!variable && std::string(argv[3]) != "fixed") throw std::invalid_argument("layout inválido");
        const auto rows = std::stoll(argv[2]);
        if (rows <= 0 || rows > INT32_MAX) throw std::invalid_argument("ROWS fora do intervalo");
        Config config;
        config.data_dir = argv[1];
        const auto schema = make_schema({{"k", FieldType::Int32, 0, true},
            {"pad", variable ? FieldType::VarChar : FieldType::Char,
             static_cast<std::uint16_t>(variable ? 2100 : 123)}});
        BufferPool pool(config);
        const auto leaves = pool.create("leaves.dat", FileKind::Data);
        const auto nodes = pool.create("nodes.dat", FileKind::Index);
        BPlusEngine tree(pool, leaves, nodes, KeyCodec::for_field(schema.field(0)),
                         [](auto bytes) -> Key { return load<std::int32_t>(bytes.data()); },
                         true, PageType::Leaf, variable ? 0u : schema.record_size);
        tree.init_empty();
        pool.flush_all();
        pool.invalidate_all();
        Input input(schema, rows, variable);
        const auto load_time = measure(pool, [&] {
            tree.bulk_load(input, 0.7);
            pool.flush_all();  // Inclui escrita das páginas e headers na carga.
        });
        const auto shape = tree.measure();
        if (shape.records != static_cast<std::uint64_t>(rows) || !tree.verify(false).empty())
            throw std::runtime_error("carga perdeu registros ou invariantes de ordenação");
        const bool valid = occupancy_ok(pool, tree, schema.record_size, variable);

        std::vector<std::int32_t> keys;
        for (std::int64_t i = 0; i < 1000; ++i)
            keys.push_back(static_cast<std::int32_t>((i * 104729 + 17) % rows));
        pool.flush_all();
        pool.invalidate_all();  // Uma vez antes do lote, não antes de cada ponto.
        const auto point_time = measure(pool, [&] {
            for (const auto key : keys) {
                const auto rid = tree.find_first(Key{key});
                if (!rid) throw std::runtime_error("ponto ausente");
                auto page = pool.fetch({leaves, rid->block}, kRandomOnce, kNoReservation);
                const auto record = sp_record(page.read(), rid->slot);
                if (load<std::int32_t>(record.data()) != key)
                    throw std::runtime_error("ponto incorreto");
            }
        });

        pool.flush_all();
        pool.invalidate_all();
        const auto scan_time = measure(pool, [&] {
            std::int64_t seen = 0;
            for (auto block = tree.meta().first_leaf; block != 0;) {
                auto page = pool.fetch({leaves, block}, kSeqOnce, kNoReservation);
                for (const auto& entry : sorted_slots(page.read(), tree.key_of())) {
                    const auto record = sp_record(page.read(), entry.second);
                    const auto expected = encode_record(schema, {seen, padding(seen, rows, variable)});
                    ++seen;
                    if (!std::equal(record.begin(), record.end(), expected.begin(), expected.end()))
                        throw std::runtime_error("varredura diverge do oráculo");
                }
                block = read_header(page.read()).next;
            }
            if (seen != rows) throw std::runtime_error("varredura perdeu registros");
        });
        std::cout << rows << ',' << valid << ',' << shape.height << ',' << shape.leaves
                  << ',' << shape.internal_blocks;
        print_measurement(load_time);
        print_measurement(point_time);
        print_measurement(scan_time);
        std::cout << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

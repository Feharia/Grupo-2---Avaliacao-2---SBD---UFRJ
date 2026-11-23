#include "layer2/algorithms.hpp"
#include "helpers.hpp"

#include <iostream>
#include <string>

using namespace cbd;

class Rows final : public RecordStream {
public:
    explicit Rows(const Schema& schema) : schema_(schema) {}
    std::optional<std::span<const std::byte>> next() override {
        if (position_ == 1000) return std::nullopt;
        bytes_ = encode_record(schema_, {position_++, std::string("x")});
        return bytes_;
    }
private:
    const Schema& schema_;
    std::int64_t position_ = 0;
    std::vector<std::byte> bytes_;
};

template <class Engine>
AccessPath scan(const Engine& engine, TableId table) {
    for (const auto& path : engine.access_paths(table))
        if (path.kind == PathKind::Scan) return path;
    throw std::logic_error("varredura ausente");
}

int main() {
    bool valid = true;
    for (const std::uint16_t width : {24, 1000, 4050}) {
        const auto schema = make_schema({{"k", FieldType::Int32, 0, true}, {"v", FieldType::VarChar, width}});
        const auto encoded = encode_record(schema, {std::int64_t{0}, std::string("x")});
        Monitor monitor(test_config("sm_var_" + std::to_string(width)));
        const auto a = monitor.create_table("a", schema, {});
        const auto b = monitor.create_table("b", schema, {});
        Rows rows_a(schema), rows_b(schema);
        monitor.bulk_load(a, rows_a);
        monitor.bulk_load(b, rows_b);
        const JoinInput outer{scan(monitor, a), 0, 0}, inner{scan(monitor, b), 0, 1};
        std::cout << "max_varchar=" << width << " encoded_bytes=" << encoded.size()
                  << " schema_record_size=" << schema.record_size << " b=" << monitor.stats(a).b << '\n';
        for (const std::size_t memory : {5, 16}) {
            monitor.flush_all();
            monitor.invalidate_all();
            const auto plan = monitor.plan_join(JoinAlgorithm::SortMerge, outer, inner, memory, {});
            std::cout << "M=" << memory << " ";
            if (const auto* p = std::get_if<JoinPlan>(&plan))
                std::cout << "run_pages=" << p->run_pages << " runs=" << p->runs_outer << "/" << p->runs_inner << " ";
            const auto outcome = monitor.join(JoinAlgorithm::SortMerge, outer, inner, memory, {});
            if (const auto* result = std::get_if<OpResult>(&outcome)) {
                std::cout << "rows=" << result->rows << " checksum=" << result->checksum
                          << " reads=" << result->delta.global.reads() << " writes=" << result->delta.global.writes() << '\n';
                valid = valid && result->rows == 1000;
            } else {
                std::cout << "inviavel=" << std::get<NotApplicable>(outcome).reason << '\n';
                valid = false;
            }
        }
#ifdef CBD_REVIEW_DIRECT
        StorageManager storage(test_config("sm_var_direct_" + std::to_string(width)));
        const auto x = storage.create_table("a", schema, {}), y = storage.create_table("b", schema, {});
        Rows rows_x(schema), rows_y(schema);
        storage.bulk_load(x, rows_x);
        storage.bulk_load(y, rows_y);
        storage.flush_all();
        storage.invalidate_all();
        auto reservation = storage.reserve(5);
        // Plano testemunha: 2 páginas de scratch + entrada + saída + 1 de descritores.
        // No merge, 3 entradas + saída + descritores; final com até 4 entradas.
        JoinPlan witness;
        witness.alg = JoinAlgorithm::SortMerge;
        witness.m = 5;
        witness.inner_unique = true;
        witness.sort_outer = witness.sort_inner = true;
        witness.run_pages = 2;
        witness.fan_in = 3;
        witness.final_fan_in = 4;
        witness.desc_frames = 1;
        witness.runs_outer = witness.runs_inner = 4;
        ResultSink sink({}, false);
        PhaseRecorder phases(storage);
        sort_merge_join(storage, {scan(storage, x), 0, 0}, {scan(storage, y), 0, 1}, witness, reservation, sink, phases);
        phases.finish();
        std::cout << "direct M=5 frames=" << witness.frames_used() << " rows=" << sink.rows()
                  << " checksum=" << sink.checksum() << '\n';
        if (sink.rows() != 1000) return 2;
#endif
    }
    return valid ? 0 : 1;
}

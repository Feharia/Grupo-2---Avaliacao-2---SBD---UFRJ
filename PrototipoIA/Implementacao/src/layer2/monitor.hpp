// Camada 2 — Mini monitor de execução: cabeçalho público usado pela camada 1.
#pragma once

#include "common/common.hpp"
#include "layer3/storage_manager.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cbd {

// ----------------------------------------------------------------------- tipos da interface 1 → 2
struct JoinInput {
    AccessPath   path;
    FieldId      field = 0;
    std::uint8_t operand = 0;   // 0 = primeiro operando da consulta (mestre); 1 = segundo
};
struct ResultShape {
    std::vector<FieldId> operand0_fields, operand1_fields;   // vazio = todos, na ordem do esquema
    std::optional<std::filesystem::path> dump;               // só em testes: grava as tuplas (166)
};
struct JoinParams {
    std::size_t chunk = 0;          // 0 = derivar de m (seção 7)
    std::size_t partitions = 0;     // 0 = derivar
    std::size_t group_pages = 2;    // grupo de duplicatas do sort-merge
};
struct SelectParams {
    std::size_t m = 2;
    double      expected_rows = 0;
    bool        sort_rids = false;  // variante da 117
};

struct DoesNotFit : std::runtime_error { using std::runtime_error::runtime_error; };   // (151)

// ----------------------------------------------------------------------- planos (143, 148)
struct JoinPlan {
    JoinAlgorithm alg = JoinAlgorithm::Bnl;
    std::size_t   m = 0;
    bool          swapped = false;                      // externa é o operando 1
    bool          inner_unique = false;                 // (157)
    // BNL
    std::size_t   chunk = 0;
    // sort-merge
    bool          sort_outer = true, sort_inner = true;
    std::size_t   run_pages = 0;                        // m − 2
    std::size_t   fan_in = 0;                           // m − 1
    std::size_t   group_pages = 0;                      // 0 se o campo da interna for unique (157)
    std::size_t   final_fan_in = 0;                     // m − group_pages
    std::size_t   runs_outer = 0, runs_inner = 0;
    std::size_t   merge_passes_outer = 0, merge_passes_inner = 0;
    // hash join (externa = build)
    bool          prepartitioned_outer = false, prepartitioned_inner = false;
    std::size_t   partitions = 0;                       // ≤ m − 1
    std::size_t   levels = 0;                           // passes de particionamento previstos (171)
    std::size_t   bucket_group = 0;                     // buckets por partição (entrada pré-particionada)
    std::size_t   build_pages = 0;                      // m − 1
    std::uint64_t partition_seed = 1, table_seed = 2;   // independentes (153)
    std::uint32_t b_outer = 0, b_inner = 0;
    std::uint32_t d_outer = 0, d_inner = 0;             // blocos densos ⌈n/bfr⌉ (emenda da 143)
    std::size_t   frames_used() const;                  // maior uso simultâneo
};
struct SelectPlan {
    PathKind      path = PathKind::Scan;
    bool          range = false;
    bool          filter_in_layer2 = false;             // caminho Scan
    std::size_t   batch = 1;
    std::size_t   m = 2;
    std::size_t   scratch_pages = 0;                    // variante 117
    double        expected_rows = 0;
    bool          sort_rids = false;
    bool          unique_field = false;
    std::size_t   frames_used() const;
};

// ----------------------------------------------------------------------- ResultSink e PhaseRecorder
class ResultSink {
public:
    ResultSink(ResultShape, bool swapped);             // swapped vem do plano
    ResultSink(ResultSink&&) noexcept = default;
    void emit(const RecordView& r);                    // seleção: operando 0
    void emit_pair(const RecordView& outer, const RecordView& inner);
    std::uint64_t rows() const { return rows_; }
    std::uint64_t checksum() const { return sum_; }
private:
    void encode(const RecordView&, const std::vector<FieldId>&);
    void finish_tuple();
    ResultShape   shape_;
    bool          swapped_;
    std::uint64_t rows_ = 0, sum_ = 0;
    std::vector<std::byte> buf_;
    std::unique_ptr<std::ofstream> dump_;               // só com shape.dump (166)
};

class PhaseRecorder {
public:
    explicit PhaseRecorder(StorageManager&);
    void begin(std::string name);
    std::vector<Phase> finish();
private:
    StorageManager& sm_;
    std::optional<std::pair<std::string, Counters>> open_;
    std::vector<Phase> done_;
};

// ----------------------------------------------------------------------- Monitor
class Monitor {
public:
    explicit Monitor(const Config&);
    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    // primitivas medidas
    Outcome join(JoinAlgorithm, const JoinInput& outer, const JoinInput& inner,
                 std::size_t m, const ResultShape&, const JoinParams& = {});
    Outcome select_points(const AccessPath&, std::span<const Point> batch,
                          const ResultShape&, const SelectParams& = {});
    Outcome select_range (const AccessPath&, const KeyRange&,
                          const ResultShape&, const SelectParams& = {});

    // repasses da camada 3 (não medidos)
    TableId     create_table(std::string name, Schema s, PrimarySpec p) { return storage_.create_table(std::move(name), std::move(s), p); }
    StructureId create_index(TableId t, IndexSpec s) { return storage_.create_index(t, s); }
    std::vector<AccessPath> access_paths(TableId t) const { return storage_.access_paths(t); }
    const Schema& schema(TableId t) const { return storage_.schema(t); }
    TableStats  stats(TableId t) const { return storage_.stats(t); }
    Occupancy   occupancy(StructureId s) { return storage_.occupancy(s); }
    std::vector<StructureId> structures(TableId t) const { return storage_.structures(t); }
    std::string structure_name(StructureId s) const { return storage_.structure_name(s); }
    void bulk_load(TableId t, RecordStream& sorted) { storage_.bulk_load(t, sorted); }
    RID  insert(TableId t, std::span<const std::byte> rec) { return storage_.insert(t, rec); }
    void erase(TableId t, RID rid) { storage_.erase(t, rid); }
    std::size_t erase_key(TableId t, FieldId f, const Key& k) { return storage_.erase_key(t, f, k); }
    void reorganize(TableId t) { storage_.reorganize(t); }
    std::vector<std::byte> encode(TableId t, const std::vector<Value>& v) const { return storage_.encode(t, v); }
    std::uint32_t suggest_buckets(const Schema& s, std::uint64_t n, std::uint64_t d, double a) const {
        return storage_.suggest_buckets(s, n, d, a);
    }
    std::string verify(TableId t) const { return storage_.verify(t); }
    std::uint64_t flush_all() { return storage_.flush_all(); }
    void          invalidate_all() { storage_.invalidate_all(); }

    // planos (públicos para teste e para a estimativa)
    std::variant<JoinPlan, NotApplicable> plan_join(JoinAlgorithm, const JoinInput&, const JoinInput&,
                                                    std::size_t m, const JoinParams&) const;
    std::variant<SelectPlan, NotApplicable> plan_select(const AccessPath&, bool range, std::size_t batch,
                                                        const SelectParams&) const;
private:
    template <class Exec, class Est>
    Outcome measured(std::size_t m, std::size_t frames, bool swapped, const ResultShape&, Exec&&, Est&&);
    StorageManager storage_;
};

}  // namespace cbd

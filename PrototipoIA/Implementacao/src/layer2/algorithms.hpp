// Camada 2 (interno) — algoritmos e estimativas: funções livres, sem estado entre chamadas.
#pragma once

#include "layer2/monitor.hpp"

namespace cbd {

void bnl_join       (StorageManager&, const JoinInput& outer, const JoinInput& inner,
                     const JoinPlan&, const Reservation&, ResultSink&, PhaseRecorder&);
void sort_merge_join(StorageManager&, const JoinInput& outer, const JoinInput& inner,
                     const JoinPlan&, const Reservation&, ResultSink&, PhaseRecorder&);
void hash_join      (StorageManager&, const JoinInput& outer, const JoinInput& inner,
                     const JoinPlan&, const Reservation&, ResultSink&, PhaseRecorder&);

void select_scan  (StorageManager&, TableId, FieldId, const std::vector<KeyRange>& keys, bool stop_at_first,
                   const Reservation&, ResultSink&);
void select_find  (StorageManager&, const AccessPath&, const Predicate&, Intent, const Reservation&, ResultSink&);
void select_probe (StorageManager&, const AccessPath&, const Predicate&, const SelectPlan&, const Reservation&,
                   ResultSink&);

// Blocos densos ⌈n/bfr⌉: tamanho dos dados quando gravados cheios (runs e partições), que pode ser
// menor que b quando a organização não está cheia (B+ com fill 0,7, hash com α = 0,8). Emenda da 143.
inline std::uint32_t dense_blocks(const TableStats& t) {
    return t.bfr ? static_cast<std::uint32_t>((t.n + t.bfr - 1) / t.bfr) : t.b;
}

Cost estimate_join  (const JoinPlan&, const TableStats& outer, const TableStats& inner);
Cost estimate_select(const SelectPlan&, const TableStats&, const StructureStats* structure);

}  // namespace cbd

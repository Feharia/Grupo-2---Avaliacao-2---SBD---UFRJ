// Camada 3 — fachada do Gerente de armazenamento. A camada 2 só usa esta interface e handles.hpp.
#pragma once

#include "common/common.hpp"
#include "layer3/handles.hpp"
#include "layer3/record.hpp"
#include "layer4/buffer_pool.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace cbd {

class Catalog;
class PrimaryFile;
class SecondaryIndex;

class StorageManager {
public:
    explicit StorageManager(const Config&);
    ~StorageManager();
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    // definição
    TableId       create_table(std::string name, Schema, PrimarySpec);
    StructureId   create_index(TableId, IndexSpec);
    const Schema& schema(TableId) const;
    std::size_t   tables() const;
    const std::string& table_name(TableId) const;

    // acesso
    std::vector<AccessPath> access_paths(TableId) const;
    BlockScan     scan_blocks(TableId, Intent, const Reservation&);
    BlockScan     scan_buckets(const AccessPath&, std::uint32_t first, std::uint32_t last, Intent, const Reservation&);
    std::uint32_t bucket_of(const AccessPath&, const Key&) const;
    SearchCursor  find(const AccessPath&, const Predicate&, Intent, const Reservation&);
    RidCursor     probe(const AccessPath&, const Predicate&, Intent);
    PinnedRecord  fetch(TableId, RID, Intent, const Reservation&);

    // memória de trabalho e temporários
    TempFile      create_temp(const Schema&, const Reservation&);
    ScratchSpace  create_scratch(const Schema&, const Reservation&, std::size_t pages);
    ScratchSpace  create_rid_scratch(const Reservation&, std::size_t pages);

    // estatística e ocupação
    TableStats    stats(TableId) const;                 // só lê o catálogo, sem E/S (160)
    Occupancy     occupancy(StructureId);
    std::vector<StructureId> structures(TableId) const;
    std::string   structure_name(StructureId) const;

    // carga e manutenção
    void bulk_load(TableId, RecordStream& sorted);
    RID  insert(TableId, std::span<const std::byte> record);
    void erase(TableId, RID);
    std::size_t erase_key(TableId, FieldId, const Key&);   // manutenção: apaga todos com a chave
    void reorganize(TableId);
    std::vector<std::byte> encode(TableId, const std::vector<Value>&) const;
    std::uint32_t suggest_buckets(const Schema&, std::uint64_t n, std::uint64_t distinct, double alpha) const;
    std::uint32_t block_size() const { return pool_.block_size(); }
    std::string   verify(TableId) const;   // invariantes das estruturas (testes)

    // repasses da camada 4
    Reservation   reserve(std::size_t frames);
    Counters      snapshot() const;
    std::uint64_t flush_all();
    void          invalidate_all();

private:
    struct TableRt {
        std::unique_ptr<PrimaryFile> primary;
    };
    struct IndexRt {
        TableId   table = 0;
        IndexSpec spec;
        std::unique_ptr<SecondaryIndex> idx;
    };
    std::unique_ptr<PrimaryFile> make_primary(TableId);
    void build_indexes(TableId);
    void build_index(StructureId);
    void on_moved(TableId, RID from, RID to, std::span<const std::byte> rec);
    void refresh_stats(TableId);
    void invalidate_stats(TableId);
    std::string next_name(const std::string& base, const char* ext);
    std::shared_ptr<const Schema> schema_ptr(TableId) const;

    BufferPool               pool_;          // declarado primeiro: destruído por último
    std::unique_ptr<Catalog> catalog_;
    std::vector<TableRt>     tables_;
    std::unordered_map<StructureId, IndexRt> indexes_;
    std::unordered_map<StructureId, TableId> primary_of_;
    StructureId              next_id_ = 1;
    std::uint32_t            seq_ = 0;
};

}  // namespace cbd

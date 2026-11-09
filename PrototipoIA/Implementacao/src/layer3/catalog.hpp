// Camada 3 (interno) — mini catálogo: esquemas, organizações, índices e estatísticas (144, 160, 165).
#pragma once

#include "common/common.hpp"
#include "layer4/buffer_pool.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cbd {

class Catalog {
public:
    struct TableEntry {
        std::string                   name;
        std::shared_ptr<const Schema> schema;     // (165)
        PrimarySpec                   primary;
        StructureId                   primary_id = 0;
        std::vector<StructureId>      indexes;
        std::uint64_t                 n = 0;
        TableStats                    stats;      // (160)
        bool                          stats_valid = false;
    };
    struct IndexEntry {
        TableId     table = 0;
        IndexSpec   spec;
        StructureId id = 0;
    };

    explicit Catalog(BufferPool&);   // cria o arquivo de catálogo e fixa sua página até o fim (144)
    ~Catalog();
    Catalog(const Catalog&) = delete;
    Catalog& operator=(const Catalog&) = delete;

    TableId add_table(std::string name, Schema, PrimarySpec, StructureId primary_id);
    void    add_index(TableId, IndexSpec, StructureId);
    TableEntry&       table(TableId t) { return tables_.at(t); }
    const TableEntry& table(TableId t) const { return tables_.at(t); }
    std::size_t       size() const { return tables_.size(); }
    const IndexEntry& index(StructureId) const;
    void              persist();   // grava um resumo na página fixada (marca suja)

private:
    BufferPool&              pool_;
    FileId                   file_ = 0;
    std::optional<PageGuard> page_;
    std::vector<TableEntry>  tables_;
    std::vector<IndexEntry>  indexes_;
};

}  // namespace cbd

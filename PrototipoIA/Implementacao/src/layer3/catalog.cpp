#include "layer3/catalog.hpp"

#include <algorithm>

namespace cbd {

Catalog::Catalog(BufferPool& pool) : pool_(pool) {
    file_ = pool_.create("catalogo.cat", FileKind::Catalog);
    auto [bid, g] = pool_.allocate(file_, kNoReservation);   // área de catálogo, fixada até o fim
    page_.emplace(std::move(g));
    persist();
}

Catalog::~Catalog() { page_.reset(); }

TableId Catalog::add_table(std::string name, Schema s, PrimarySpec p, StructureId primary_id) {
    for (const auto& t : tables_)
        if (t.name == name) throw std::invalid_argument("tabela já existe: " + name);
    TableEntry e;
    e.name = std::move(name);
    e.schema = std::make_shared<const Schema>(std::move(s));
    e.primary = p;
    e.primary_id = primary_id;
    tables_.push_back(std::move(e));
    persist();
    return static_cast<TableId>(tables_.size() - 1);
}

void Catalog::add_index(TableId t, IndexSpec s, StructureId id) {
    tables_.at(t).indexes.push_back(id);
    indexes_.push_back(IndexEntry{t, s, id});
    persist();
}

const Catalog::IndexEntry& Catalog::index(StructureId id) const {
    for (const auto& e : indexes_)
        if (e.id == id) return e;
    throw std::out_of_range("índice inexistente no catálogo");
}

void Catalog::persist() {
    std::string text = "catalogo v1\n";
    for (std::size_t t = 0; t < tables_.size(); ++t) {
        const auto& e = tables_[t];
        text += std::to_string(t) + ";" + e.name + ";" + to_string(e.primary.kind) + ";campos=" +
                std::to_string(e.schema->fields.size()) + ";n=" + std::to_string(e.n) + ";indices=" +
                std::to_string(e.indexes.size()) + "\n";
    }
    auto bytes = page_->write();
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    std::memcpy(bytes.data(), text.data(), std::min(text.size(), bytes.size() - 1));
}

}  // namespace cbd

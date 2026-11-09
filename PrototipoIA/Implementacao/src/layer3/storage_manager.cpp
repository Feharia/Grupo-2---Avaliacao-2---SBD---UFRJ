#include "layer3/storage_manager.hpp"

#include "layer3/catalog.hpp"
#include "layer3/structures.hpp"

#include <cmath>

namespace cbd {

namespace {

// Fluxo de registros de uma varredura (manutenção: reorganize e construção de índices).
class ScanStream final : public RecordStream {
public:
    explicit ScanStream(BlockScan s) : scan_(std::move(s)) {}
    std::optional<std::span<const std::byte>> next() override {
        for (;;) {
            if (!cur_) {
                cur_ = scan_.next();
                if (!cur_) return std::nullopt;
            }
            if (auto r = cur_->next()) return r->bytes();
            cur_.reset();
        }
    }
private:
    BlockScan scan_;
    std::optional<BlockCursor> cur_;
};

// Fluxo ordenado das folhas de uma B+ primária (ordena cada folha pela chave).
class SortedLeafStream final : public RecordStream {
public:
    SortedLeafStream(BufferPool& p, FileId f, std::uint32_t first, BPlusEngine::KeyOf key_of)
        : pool_(p), file_(f), next_(first), key_of_(std::move(key_of)) {}
    std::optional<std::span<const std::byte>> next() override {
        for (;;) {
            if (!leaf_) {
                if (next_ == 0) return std::nullopt;
                leaf_.emplace(pool_.fetch(BlockId{file_, next_}, kSeqOnce, kNoReservation));
                order_ = sorted_slots(leaf_->read(), key_of_);
                pos_ = 0;
            }
            if (pos_ < order_.size()) return sp_record(leaf_->read(), order_[pos_++].second);
            next_ = read_header(leaf_->read()).next;
            leaf_.reset();
        }
    }
private:
    BufferPool& pool_;
    FileId file_;
    std::uint32_t next_;
    BPlusEngine::KeyOf key_of_;
    std::optional<PageGuard> leaf_;
    std::vector<std::pair<Key, std::uint16_t>> order_;
    std::size_t pos_ = 0;
};

class CountingStream final : public RecordStream {
public:
    explicit CountingStream(RecordStream& in) : in_(in) {}
    std::optional<std::span<const std::byte>> next() override {
        auto r = in_.next();
        if (r) ++n_;
        return r;
    }
    std::uint64_t count() const { return n_; }
private:
    RecordStream& in_;
    std::uint64_t n_ = 0;
};

}  // namespace

StorageManager::StorageManager(const Config& cfg) : pool_(cfg), catalog_(std::make_unique<Catalog>(pool_)) {}

StorageManager::~StorageManager() {
    indexes_.clear();
    tables_.clear();
    catalog_.reset();
}

std::string StorageManager::next_name(const std::string& base, const char* ext) {
    return base + "_" + std::to_string(++seq_) + "." + ext;
}

std::shared_ptr<const Schema> StorageManager::schema_ptr(TableId t) const { return catalog_->table(t).schema; }
const Schema& StorageManager::schema(TableId t) const { return *catalog_->table(t).schema; }
std::size_t StorageManager::tables() const { return tables_.size(); }
const std::string& StorageManager::table_name(TableId t) const { return catalog_->table(t).name; }

// ----------------------------------------------------------------------- definição
std::unique_ptr<PrimaryFile> StorageManager::make_primary(TableId t) {
    const auto& e = catalog_->table(t);
    const auto& sch = e.schema;
    const PrimarySpec& p = e.primary;
    std::unique_ptr<PrimaryFile> pf;
    switch (p.kind) {
        case StructureKind::Heap:
            pf = std::make_unique<HeapFile>(pool_, pool_.create(next_name(e.name, "heap"), FileKind::Data), sch);
            break;
        case StructureKind::BTree: {
            const FileId data = pool_.create(next_name(e.name, "btd"), FileKind::Data);
            const FileId index = pool_.create(next_name(e.name, "bti"), FileKind::Index);   // dois arquivos (145)
            pf = std::make_unique<BPlusTreeFile>(pool_, data, index, sch, p.key, sch->field(p.key).unique, p.fill);
            break;
        }
        case StructureKind::Hash:
            pf = std::make_unique<HashFile>(pool_, pool_.create(next_name(e.name, "hash"), FileKind::Data), sch, p.key,
                                            p.buckets, sch->field(p.key).unique);
            break;
        default:
            throw std::invalid_argument("organização primária inválida");
    }
    pf->set_on_move([this, t](RID from, RID to, std::span<const std::byte> rec) { on_moved(t, from, to, rec); });
    return pf;
}

TableId StorageManager::create_table(std::string name, Schema s, PrimarySpec p) {
    if (p.kind != StructureKind::Heap) {
        if (p.key >= s.fields.size()) throw std::invalid_argument("chave da organização fora do esquema");
        if (s.fields[p.key].nullable) throw std::invalid_argument("chave da organização não pode ser anulável");
        if (p.kind == StructureKind::Hash && p.buckets == 0)
            throw std::invalid_argument("hash: buckets precisa vir calculado (39, 163)");
    }
    const StructureId pid = next_id_++;
    const TableId t = catalog_->add_table(std::move(name), std::move(s), p, pid);
    tables_.emplace_back();
    tables_[t].primary = make_primary(t);
    primary_of_[pid] = t;
    refresh_stats(t);
    return t;
}

StructureId StorageManager::create_index(TableId t, IndexSpec spec) {
    const auto& e = catalog_->table(t);
    const auto& f = e.schema->field(spec.key);
    if (spec.kind == StructureKind::SecHash) {
        if (!f.unique) throw std::invalid_argument("hash secundário só em campo unique (157)");
        if (spec.clustering) throw std::invalid_argument("índice clustering só em B+ (116)");
    } else if (spec.kind == StructureKind::SecBTree) {
        if (spec.clustering) {
            const bool ok_kind = e.primary.kind == StructureKind::BTree || e.primary.kind == StructureKind::Hash;
            if (!ok_kind || e.primary.key != spec.key)
                throw std::invalid_argument("clustering só sobre B+ ou hash primários e no campo da organização (116)");
        }
    } else {
        throw std::invalid_argument("tipo de índice secundário inválido");
    }
    const StructureId id = next_id_++;
    catalog_->add_index(t, spec, id);
    indexes_[id] = IndexRt{t, spec, nullptr};
    build_index(id);   // constrói a partir dos dados existentes, se houver
    refresh_stats(t);
    return id;
}

void StorageManager::build_index(StructureId id) {
    IndexRt& ix = indexes_.at(id);
    const auto& e = catalog_->table(ix.table);
    if (ix.idx) {
        for (FileId f : ix.idx->files()) pool_.remove_file(f);
        ix.idx.reset();
    }
    const auto& fd = e.schema->field(ix.spec.key);
    const KeyCodec kc = KeyCodec::for_field(fd);
    const FileId file = pool_.create(next_name(e.name + "_" + fd.name, "idx"), FileKind::Index);
    if (ix.spec.kind == StructureKind::SecHash) {
        const std::uint32_t cap = sp_capacity(pool_.block_size(), kc.size + sizeof(RID));
        const auto nb = next_prime(static_cast<std::uint32_t>(
            std::max<double>(1.0, std::ceil(static_cast<double>(e.n) / (0.8 * cap)))));
        ix.idx = std::make_unique<HashIndex>(pool_, file, kc, nb, fd.unique);
    } else {
        ix.idx = std::make_unique<BPlusIndex>(pool_, file, kc, ix.spec.clustering);
    }
    if (e.n == 0) return;
    PrimaryFile& pf = *tables_[ix.table].primary;
    BlockScan scan(&pool_, pf.scan_plan(), e.schema, kSeqOnce, kNoReservation);
    while (auto cur = scan.next()) {
        while (auto r = cur->next()) {
            if (r->is_null(ix.spec.key)) continue;
            const Key k = r->key(ix.spec.key);
            const RID rid = cur->last_rid();
            if (ix.spec.clustering) {
                if (!ix.idx->first(k)) ix.idx->set_first(k, rid);   // 1º na ordem da organização
            } else {
                ix.idx->add(k, rid);
            }
        }
    }
}

void StorageManager::build_indexes(TableId t) {
    for (StructureId id : catalog_->table(t).indexes) build_index(id);
}

// ----------------------------------------------------------------------- acesso
std::vector<AccessPath> StorageManager::access_paths(TableId t) const {
    const auto& e = catalog_->table(t);
    std::vector<AccessPath> out;
    AccessPath scan;
    scan.table = t;
    scan.id = e.primary_id;
    scan.kind = PathKind::Scan;
    out.push_back(scan);
    if (e.primary.kind == StructureKind::BTree || e.primary.kind == StructureKind::Hash) {
        AccessPath p;
        p.table = t;
        p.id = e.primary_id;
        p.field = e.primary.key;
        p.unique = e.schema->field(p.field).unique;
        if (e.primary.kind == StructureKind::BTree) {
            p.kind = PathKind::PrimaryBTree;
            p.supports_point = p.supports_range = true;
            p.ordered_by = p.field;
        } else {
            p.kind = PathKind::PrimaryHash;
            p.supports_point = true;
            const auto* hf = static_cast<const HashFile*>(tables_.at(t).primary.get());
            p.hash_partitioned_by = HashPartitioning{e.primary.hash_function, hf->buckets()};
        }
        out.push_back(p);
    }
    for (StructureId id : e.indexes) {
        const IndexRt& ix = indexes_.at(id);
        AccessPath p;
        p.table = t;
        p.id = id;
        p.field = ix.spec.key;
        p.unique = e.schema->field(p.field).unique;
        if (ix.spec.kind == StructureKind::SecHash) {
            p.kind = PathKind::SecondaryHash;
            p.supports_point = true;
        } else if (ix.spec.clustering) {
            p.kind = PathKind::SecondaryBTreeClustering;
            p.supports_point = p.supports_range = true;
            p.ordered_by = p.field;
        } else {
            p.kind = PathKind::SecondaryBTreeNonClustering;
            p.supports_point = p.supports_range = true;
        }
        out.push_back(p);
    }
    return out;
}

BlockScan StorageManager::scan_blocks(TableId t, Intent i, const Reservation& r) {
    return BlockScan(&pool_, tables_.at(t).primary->scan_plan(), schema_ptr(t), i, r.id());
}

BlockScan StorageManager::scan_buckets(const AccessPath& p, std::uint32_t first, std::uint32_t last, Intent i,
                                       const Reservation& r) {
    if (p.kind != PathKind::PrimaryHash) throw std::logic_error("scan_buckets só no hash primário");
    const auto* hf = static_cast<const HashFile*>(tables_.at(p.table).primary.get());
    if (first > last || last >= hf->buckets()) throw std::out_of_range("scan_buckets: intervalo inválido");
    ScanPlan plan{ScanPlan::Order::BucketChains, hf->data_file(), first, last - first + 1};
    return BlockScan(&pool_, plan, schema_ptr(p.table), i, r.id());
}

std::uint32_t StorageManager::bucket_of(const AccessPath& p, const Key& k) const {
    if (p.kind != PathKind::PrimaryHash) throw std::logic_error("bucket_of só no hash primário");
    return static_cast<const HashFile*>(tables_.at(p.table).primary.get())->bucket_of(k);
}

SearchCursor StorageManager::find(const AccessPath& p, const Predicate& pred, Intent i, const Reservation& r) {
    if (predicate_field(pred) != p.field) throw std::logic_error("find: predicado em campo diferente do caminho");
    PrimaryFile& pf = *tables_.at(p.table).primary;
    switch (p.kind) {
        case PathKind::PrimaryBTree:
        case PathKind::PrimaryHash:
            return SearchCursor(pf.find(pred, p.unique, i, r.id()));
        case PathKind::SecondaryBTreeClustering: {
            const auto& ix = indexes_.at(p.id);
            const auto* bi = static_cast<const BPlusIndex*>(ix.idx.get());
            return SearchCursor(make_clustering_search(*bi, pf, schema_ptr(p.table), p.field, pred, p.unique, i, r.id(), pool_));
        }
        default:
            throw std::logic_error("find recusa este caminho (146): use probe ou o Scan");
    }
}

RidCursor StorageManager::probe(const AccessPath& p, const Predicate& pred, Intent i) {
    if (predicate_field(pred) != p.field) throw std::logic_error("probe: predicado em campo diferente do caminho");
    if (p.kind != PathKind::SecondaryHash && p.kind != PathKind::SecondaryBTreeNonClustering)
        throw std::logic_error("probe só em índice secundário não clustering");
    return RidCursor(indexes_.at(p.id).idx->probe(pred, i));
}

PinnedRecord StorageManager::fetch(TableId t, RID rid, Intent i, const Reservation& r) {
    PageGuard g = pool_.fetch(BlockId{tables_.at(t).primary->data_file(), rid.block}, i, r.id());
    const auto rec = sp_record(g.read(), rid.slot);
    if (rec.empty()) throw std::logic_error("fetch: RID aponta para registro removido");
    const RecordView v = detail::make_view(rec, catalog_->table(t).schema.get());
    return PinnedRecord{std::move(g), v};
}

// ----------------------------------------------------------------------- memória de trabalho
TempFile StorageManager::create_temp(const Schema& s, const Reservation& r) {
    const FileId f = pool_.create(next_name("tmp", "tmp"), FileKind::Temp);
    return TempFile(&pool_, f, std::make_shared<const Schema>(s), r.id());   // cópia do esquema (165)
}

ScratchSpace StorageManager::create_scratch(const Schema& s, const Reservation& r, std::size_t pages) {
    if (pages == 0) throw std::invalid_argument("create_scratch com 0 páginas");
    std::vector<PageGuard> pg;
    pg.reserve(pages);
    for (std::size_t i = 0; i < pages; ++i) pg.push_back(pool_.scratch(r.id()));
    return ScratchSpace(std::move(pg), std::make_shared<const Schema>(s), pool_.block_size());
}

ScratchSpace StorageManager::create_rid_scratch(const Reservation& r, std::size_t pages) {
    static const Schema rid_schema = make_schema({FieldSpec{"rid", FieldType::Int64}});
    return create_scratch(rid_schema, r, pages);
}

// ----------------------------------------------------------------------- estatística e ocupação
TableStats StorageManager::stats(TableId t) const {
    const auto& e = catalog_->table(t);
    if (!e.stats_valid) throw std::logic_error("estatísticas desatualizadas: rode reorganize antes de medir (160)");
    return e.stats;
}

void StorageManager::refresh_stats(TableId t) {
    auto& e = catalog_->table(t);
    PrimaryFile& pf = *tables_.at(t).primary;
    TableStats s;
    s.n = e.n;
    s.r = e.schema->record_size;
    s.bfr = sp_capacity(pool_.block_size(), s.r);
    s.b = pf.data_blocks();
    StructureStats ps = pf.measure();
    ps.id = e.primary_id;
    s.structures.push_back(ps);
    for (StructureId id : e.indexes) {
        StructureStats is = indexes_.at(id).idx->measure();
        is.id = id;
        s.structures.push_back(is);
    }
    e.stats = std::move(s);
    e.stats_valid = true;
}

void StorageManager::invalidate_stats(TableId t) { catalog_->table(t).stats_valid = false; }

Occupancy StorageManager::occupancy(StructureId id) {
    if (auto it = primary_of_.find(id); it != primary_of_.end()) return tables_.at(it->second).primary->occupancy();
    return indexes_.at(id).idx->occupancy();
}

std::vector<StructureId> StorageManager::structures(TableId t) const {
    std::vector<StructureId> v{catalog_->table(t).primary_id};
    for (auto id : catalog_->table(t).indexes) v.push_back(id);
    return v;
}

std::string StorageManager::structure_name(StructureId id) const {
    if (auto it = primary_of_.find(id); it != primary_of_.end()) {
        const auto& e = catalog_->table(it->second);
        return e.name + ":" + to_string(e.primary.kind);
    }
    const auto& ix = indexes_.at(id);
    const auto& e = catalog_->table(ix.table);
    return e.name + ":" + to_string(ix.spec.kind) + (ix.spec.clustering ? "_clustering" : "") + "(" +
           e.schema->field(ix.spec.key).name + ")";
}

// ----------------------------------------------------------------------- carga e manutenção
void StorageManager::bulk_load(TableId t, RecordStream& sorted) {
    CountingStream cs(sorted);
    tables_.at(t).primary->bulk_load(cs);
    catalog_->table(t).n += cs.count();
    build_indexes(t);   // índices construídos depois da organização
    refresh_stats(t);
    catalog_->persist();
}

RID StorageManager::insert(TableId t, std::span<const std::byte> rec) {
    const auto& e = catalog_->table(t);
    const RecordView v = detail::make_view(rec, e.schema.get());
    for (StructureId id : e.indexes) {   // unicidade verificada pela estrutura que indexa o campo (157)
        const IndexRt& ix = indexes_.at(id);
        if (!e.schema->field(ix.spec.key).unique || ix.spec.clustering || v.is_null(ix.spec.key)) continue;
        auto st = ix.idx->probe(Point{ix.spec.key, v.key(ix.spec.key)}, kRandomReuse);
        if (st->next()) throw DuplicateKey("duplicata em " + e.schema->field(ix.spec.key).name);
    }
    PrimaryFile& pf = *tables_.at(t).primary;
    const RID rid = pf.insert(rec);
    for (StructureId id : e.indexes) {
        IndexRt& ix = indexes_.at(id);
        if (v.is_null(ix.spec.key)) continue;
        const Key k = v.key(ix.spec.key);
        if (ix.spec.clustering) ix.idx->set_first(k, pf.first_of_group(k));
        else ix.idx->add(k, rid);
    }
    ++catalog_->table(t).n;
    invalidate_stats(t);
    return rid;
}

void StorageManager::erase(TableId t, RID rid) {
    const auto& e = catalog_->table(t);
    PrimaryFile& pf = *tables_.at(t).primary;
    std::vector<std::byte> copy;
    {
        PageGuard g = pool_.fetch(BlockId{pf.data_file(), rid.block}, kRandomReuse, kNoReservation);
        const auto r = sp_record(g.read(), rid.slot);
        if (r.empty()) throw std::logic_error("erase: registro inexistente");
        copy.assign(r.begin(), r.end());
    }
    const RecordView v = detail::make_view(copy, e.schema.get());
    for (StructureId id : e.indexes) {
        IndexRt& ix = indexes_.at(id);
        if (!ix.spec.clustering && !v.is_null(ix.spec.key)) ix.idx->remove(v.key(ix.spec.key), rid);
    }
    pf.erase(rid);   // movimentações avisadas ao MoveListener (141)
    for (StructureId id : e.indexes) {
        IndexRt& ix = indexes_.at(id);
        if (ix.spec.clustering && !v.is_null(ix.spec.key)) {
            const Key k = v.key(ix.spec.key);
            ix.idx->set_first(k, pf.first_of_group(k));
        }
    }
    --catalog_->table(t).n;
    invalidate_stats(t);
}

std::size_t StorageManager::erase_key(TableId t, FieldId f, const Key& k) {
    // Um registro por vez: apagar pode mover outros (B+), então o RID é localizado de novo a cada passo.
    const auto& e = catalog_->table(t);
    PrimaryFile& pf = *tables_.at(t).primary;
    auto locate = [&]() -> std::optional<RID> {
        if (e.primary.kind != StructureKind::Heap && e.primary.key == f) return pf.first_of_group(k);
        for (StructureId id : e.indexes) {
            const IndexRt& ix = indexes_.at(id);
            if (ix.spec.key != f || ix.spec.clustering) continue;
            auto st = ix.idx->probe(Point{f, k}, kRandomReuse);
            return st->next();
        }
        BlockScan scan(&pool_, pf.scan_plan(), e.schema, kSeqOnce, kNoReservation);
        while (auto c = scan.next())
            while (auto r = c->next())
                if (!r->is_null(f) && compare_keys(r->key(f), k) == 0) return c->last_rid();
        return std::nullopt;
    };
    std::size_t n = 0;
    while (auto rid = locate()) {
        erase(t, *rid);
        ++n;
    }
    return n;
}

void StorageManager::on_moved(TableId t, RID from, RID to, std::span<const std::byte> rec) {
    const auto& e = catalog_->table(t);
    const RecordView v = detail::make_view(rec, e.schema.get());
    for (StructureId id : e.indexes) {
        IndexRt& ix = indexes_.at(id);
        if (v.is_null(ix.spec.key)) continue;
        ix.idx->move(v.key(ix.spec.key), from, to);
    }
}

void StorageManager::reorganize(TableId t) {
    auto& e = catalog_->table(t);
    std::unique_ptr<PrimaryFile> old = std::move(tables_.at(t).primary);
    tables_.at(t).primary = make_primary(t);
    PrimaryFile& fresh = *tables_.at(t).primary;
    std::uint64_t n = 0;
    if (old->kind() == StructureKind::BTree) {
        const auto* bt = static_cast<const BPlusTreeFile*>(old.get());
        SortedLeafStream s(pool_, bt->data_file(), bt->engine().meta().first_leaf, bt->engine().key_of());
        CountingStream cs(s);
        fresh.bulk_load(cs);
        n = cs.count();
    } else {
        ScanStream s(BlockScan(&pool_, old->scan_plan(), e.schema, kSeqOnce, kNoReservation));
        CountingStream cs(s);
        fresh.bulk_load(cs);
        n = cs.count();
    }
    for (FileId f : old->files()) pool_.remove_file(f);
    old.reset();
    e.n = n;
    build_indexes(t);   // RIDs mudaram: índices reconstruídos (141)
    refresh_stats(t);
    catalog_->persist();
}

std::vector<std::byte> StorageManager::encode(TableId t, const std::vector<Value>& vals) const {
    return encode_record(schema(t), vals);
}

std::uint32_t StorageManager::suggest_buckets(const Schema& s, std::uint64_t n, std::uint64_t distinct, double alpha) const {
    const std::uint32_t cap = sp_capacity(pool_.block_size(), s.record_size);
    double x = std::ceil(static_cast<double>(n) / (alpha * cap));
    if (distinct > 0 && static_cast<double>(distinct) < x) x = static_cast<double>(distinct);   // (163)
    return next_prime(static_cast<std::uint32_t>(std::max(1.0, x)));
}

std::string StorageManager::verify(TableId t) const {
    const PrimaryFile& pf = *tables_.at(t).primary;
    if (pf.kind() == StructureKind::BTree) {
        auto r = static_cast<const BPlusTreeFile&>(pf).verify(true);
        if (!r.empty()) return "primária: " + r;
    }
    for (StructureId id : catalog_->table(t).indexes) {
        const auto& ix = indexes_.at(id);
        if (ix.spec.kind == StructureKind::SecBTree) {
            auto r = static_cast<const BPlusIndex&>(*ix.idx).verify();
            if (!r.empty()) return "índice " + std::to_string(id) + ": " + r;
        }
    }
    return {};
}

// ----------------------------------------------------------------------- repasses
Reservation StorageManager::reserve(std::size_t n) { return pool_.reserve(n); }
Counters StorageManager::snapshot() const { return pool_.snapshot(); }
std::uint64_t StorageManager::flush_all() { return pool_.flush_all(); }
void StorageManager::invalidate_all() { pool_.invalidate_all(); }

}  // namespace cbd

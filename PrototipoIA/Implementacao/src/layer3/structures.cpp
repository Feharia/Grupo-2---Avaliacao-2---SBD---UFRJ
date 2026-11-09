#include "layer3/structures.hpp"

#include <algorithm>

namespace cbd {

namespace {

RecordView view_of(std::span<const std::byte> r, const Schema* s) { return detail::make_view(r, s); }

// ----------------------------------------------------------------------- estados de busca
class HashSearch final : public SearchState {
public:
    HashSearch(BufferPool& p, FileId f, std::shared_ptr<const Schema> s, FieldId field, Key k, std::uint32_t head,
               bool unique, Intent i, ReservationId r)
        : pool_(p), file_(f), schema_(std::move(s)), field_(field), key_(std::move(k)), blk_(head),
          unique_(unique), intent_(i), res_(r) {}
    std::optional<RecordView> next() override {
        if (stop_) { page_.reset(); return std::nullopt; }
        for (;;) {
            if (!page_) {
                if (blk_ == 0) return std::nullopt;
                page_.emplace(pool_.fetch(BlockId{file_, blk_}, intent_, res_));
                slot_ = 0;
            }
            const auto pg = page_->read();
            const std::uint16_t n = sp_nslots(pg);
            while (slot_ < n) {
                const auto r = sp_record(pg, slot_++);
                if (r.empty()) continue;
                const RecordView v = make_view(r, schema_.get());
                if (!v.is_null(field_) && compare_keys(v.key(field_), key_) == 0) {
                    if (unique_) stop_ = true;
                    return v;
                }
            }
            blk_ = read_header(pg).next;   // segue a cadeia de overflow (§7.1)
            page_.reset();
        }
    }
private:
    BufferPool& pool_;
    FileId file_;
    std::shared_ptr<const Schema> schema_;
    FieldId field_;
    Key key_;
    std::uint32_t blk_;
    bool unique_;
    Intent intent_;
    ReservationId res_;
    std::optional<PageGuard> page_;
    std::uint16_t slot_ = 0;
    bool stop_ = false;
};

// Percorre folhas encadeadas a partir de um bloco, em ordem de chave, dentro da faixa.
class LeafRangeSearch final : public SearchState {
public:
    LeafRangeSearch(BufferPool& p, FileId leaf_file, std::shared_ptr<const Schema> s, FieldId field, KeyRange range,
                    bool unique_point, Intent i, ReservationId r, std::function<std::uint32_t()> start)
        : pool_(p), file_(leaf_file), schema_(std::move(s)), field_(field), range_(std::move(range)),
          unique_point_(unique_point), intent_(i), res_(r), start_(std::move(start)) {}
    std::optional<RecordView> next() override {
        if (done_) { leaf_.reset(); return std::nullopt; }
        if (!started_) {
            started_ = true;
            blk_ = start_();   // nós internos/índice lidos sem reserva, na área de índices
        }
        for (;;) {
            if (!leaf_) {
                if (blk_ == 0) { done_ = true; return std::nullopt; }
                leaf_.emplace(pool_.fetch(BlockId{file_, blk_}, intent_, res_));
                order_ = sorted_slots(leaf_->read(), [&](std::span<const std::byte> r) {
                    const RecordView v = view_of(r, schema_.get());
                    return v.key(field_);
                });
                pos_ = 0;
            }
            while (pos_ < order_.size()) {
                const auto& [k, slot] = order_[pos_++];
                if (range_.below(k)) continue;
                if (range_.above(k)) { done_ = true; return std::nullopt; }
                if (unique_point_) done_ = true;
                return make_view(sp_record(leaf_->read(), slot), schema_.get());
            }
            blk_ = read_header(leaf_->read()).next;   // solta o bloco anterior ao passar ao seguinte
            leaf_.reset();
        }
    }
private:
    BufferPool& pool_;
    FileId file_;
    std::shared_ptr<const Schema> schema_;
    FieldId field_;
    KeyRange range_;
    bool unique_point_;
    Intent intent_;
    ReservationId res_;
    std::function<std::uint32_t()> start_;
    std::optional<PageGuard> leaf_;
    std::vector<std::pair<Key, std::uint16_t>> order_;
    std::size_t pos_ = 0;
    std::uint32_t blk_ = 0;
    bool started_ = false, done_ = false;
};

// Índice clustering sobre hash primário: para cada valor na faixa, percorre a cadeia a partir do 1º RID.
class ClusteringHashSearch final : public SearchState {
public:
    ClusteringHashSearch(const BPlusIndex& idx, FileId data, std::shared_ptr<const Schema> s, FieldId field,
                         KeyRange range, Intent i, ReservationId r)
        : idx_(idx), data_(data), schema_(std::move(s)), field_(field), it_(&idx, range, kRandomReuse),
          intent_(i), res_(r) {}
    std::optional<RecordView> next() override {
        for (;;) {
            if (!cur_) {
                page_.reset();
                auto e = it_.next();
                if (!e) return std::nullopt;
                cur_ = e->first;
                blk_ = idx_.entry_rid(e->second, 0).block;
            }
            if (!page_) {
                if (blk_ == 0) { cur_.reset(); continue; }
                page_.emplace(idx_.pool().fetch(BlockId{data_, blk_}, intent_, res_));
                slot_ = 0;
            }
            const auto pg = page_->read();
            const std::uint16_t n = sp_nslots(pg);
            while (slot_ < n) {
                const auto r = sp_record(pg, slot_++);
                if (r.empty()) continue;
                const RecordView v = make_view(r, schema_.get());
                if (!v.is_null(field_) && compare_keys(v.key(field_), *cur_) == 0) return v;
            }
            blk_ = read_header(pg).next;
            page_.reset();
        }
    }
private:
    const BPlusIndex& idx_;
    FileId data_;
    std::shared_ptr<const Schema> schema_;
    FieldId field_;
    BPlusIndex::EntryIter it_;
    Intent intent_;
    ReservationId res_;
    std::optional<Key> cur_;
    std::optional<PageGuard> page_;
    std::uint32_t blk_ = 0;
    std::uint16_t slot_ = 0;
};

// ----------------------------------------------------------------------- estados de RIDs
class HashIndexRidState final : public RidState {
public:
    HashIndexRidState(BufferPool& p, FileId f, KeyCodec kc, Key k, std::uint32_t head, bool unique, Intent i)
        : pool_(p), file_(f), kc_(kc), key_(std::move(k)), blk_(head), unique_(unique), intent_(i) {}
    std::optional<RID> next() override {
        if (stop_) return std::nullopt;
        for (;;) {
            if (!page_) {
                if (blk_ == 0) return std::nullopt;
                page_.emplace(pool_.fetch(BlockId{file_, blk_}, intent_, kNoReservation));
                slot_ = 0;
            }
            const auto pg = page_->read();
            const std::uint16_t n = sp_nslots(pg);
            while (slot_ < n) {
                const auto e = sp_record(pg, slot_++);
                if (e.empty()) continue;
                if (compare_keys(kc_.decode(e.data()), key_) == 0) {
                    RID rid;
                    std::memcpy(&rid, e.data() + kc_.size, sizeof rid);
                    if (unique_) { stop_ = true; page_.reset(); }
                    return rid;
                }
            }
            blk_ = read_header(pg).next;
            page_.reset();
        }
    }
private:
    BufferPool& pool_;
    FileId file_;
    KeyCodec kc_;
    Key key_;
    std::uint32_t blk_;
    bool unique_;
    Intent intent_;
    std::optional<PageGuard> page_;
    std::uint16_t slot_ = 0;
    bool stop_ = false;
};

class BTreeIndexRidState final : public RidState {
public:
    BTreeIndexRidState(const BPlusIndex& idx, KeyRange range, Intent i) : idx_(idx), it_(&idx, std::move(range), i), intent_(i) {}
    std::optional<RID> next() override {
        for (;;) {
            if (!entry_) {
                ovf_page_.reset();
                auto e = it_.next();
                if (!e) return std::nullopt;
                entry_ = e->second;
                count_ = idx_.entry_count(*entry_);
                inline_n_ = std::min(count_, BPlusIndex::kInline);
                ovf_ = idx_.entry_overflow(*entry_);
                i_ = 0;
                j_ = 0;
            }
            if (i_ < inline_n_) return idx_.entry_rid(*entry_, i_++);
            // cadeia de overflow da posting list
            if (!ovf_page_) {
                if (ovf_ == 0) { entry_.reset(); continue; }
                ovf_page_.emplace(idx_.pool().fetch(BlockId{idx_.file(), ovf_}, intent_, kNoReservation));
                j_ = 0;
            }
            const auto pg = ovf_page_->read();
            const PageHeader h = read_header(pg);
            if (j_ < h.nslots) {
                RID rid;
                std::memcpy(&rid, pg.data() + kPageHeader + 8u * j_, sizeof rid);
                ++j_;
                return rid;
            }
            ovf_ = h.next;
            ovf_page_.reset();
        }
    }
private:
    const BPlusIndex& idx_;
    BPlusIndex::EntryIter it_;
    Intent intent_;
    std::optional<std::span<const std::byte>> entry_;
    std::uint32_t count_ = 0, inline_n_ = 0, ovf_ = 0, i_ = 0, j_ = 0;
    std::optional<PageGuard> ovf_page_;
};

}  // namespace

// ======================================================================= HeapFile
HeapFile::HeapFile(BufferPool& p, FileId f, std::shared_ptr<const Schema> s) : pool_(p), file_(f), schema_(std::move(s)) {}

void HeapFile::bulk_load(RecordStream& s) {
    while (auto r = s.next()) insert(*r);
}

RID HeapFile::insert(std::span<const std::byte> rec) {
    if (last_ != 0) {
        auto g = pool_.fetch(BlockId{file_, last_}, kRandomReuse, kNoReservation);
        if (auto slot = sp_insert(g.write(), rec)) {
            fsm_[last_] = static_cast<std::uint16_t>(sp_free_bytes(g.read()));
            return RID{last_, *slot};
        }
    }
    auto [bid, g] = pool_.allocate(file_, kNoReservation);
    if (bid.block != last_ + 1) throw std::logic_error("heap: bloco novo fora do fim do arquivo (30, 162)");
    sp_init(g.write(), PageType::Heap);
    const auto slot = sp_insert(g.write(), rec);
    if (!slot) throw std::logic_error("heap: registro maior que o bloco");
    last_ = bid.block;
    fsm_.resize(last_ + 1, 0);
    fsm_[last_] = static_cast<std::uint16_t>(sp_free_bytes(g.read()));
    return RID{last_, *slot};
}

void HeapFile::erase(RID rid) {   // marca o slot; nunca libera o bloco (162)
    if (rid.block == 0 || rid.block > last_) throw std::logic_error("heap erase: RID fora do arquivo");
    auto g = pool_.fetch(BlockId{file_, rid.block}, kRandomReuse, kNoReservation);
    sp_erase(g.write(), rid.slot);
    fsm_[rid.block] = static_cast<std::uint16_t>(sp_free_bytes(g.read()));
}

std::unique_ptr<SearchState> HeapFile::find(const Predicate&, bool, Intent, ReservationId) {
    throw std::logic_error("heap não tem find: o caminho Scan é filtrado na camada 2 (146)");
}

StructureStats HeapFile::measure() {
    StructureStats s;
    s.kind = StructureKind::Heap;
    s.blocks = last_;
    return s;
}

Occupancy HeapFile::occupancy() {
    Occupancy o;
    o.data = last_;
    o.header = 1;
    return o;
}

std::uint32_t HeapFile::free_bytes() const {
    std::uint32_t t = 0;
    for (auto f : fsm_) t += f;
    return t;
}

// ======================================================================= HashFile
HashFile::HashFile(BufferPool& p, FileId f, std::shared_ptr<const Schema> s, FieldId key, std::uint32_t nb, bool unique)
    : pool_(p), file_(f), schema_(std::move(s)), key_(key), nbuckets_(nb), unique_(unique) {
    if (nb == 0) throw std::invalid_argument("hash: nº de buckets precisa ser calculado pela camada 1 (39, 163)");
    for (std::uint32_t b = 0; b < nb; ++b) {
        auto [bid, g] = pool_.allocate(file_, kNoReservation);
        if (bid.block != b + 1) throw std::logic_error("hash: buckets precisam ser contíguos");
        sp_init(g.write(), PageType::Bucket);
        PageHeader h = read_header(g.read());
        h.aux = b;
        write_header(g.write(), h);
    }
}

void HashFile::bulk_load(RecordStream& s) {
    while (auto r = s.next()) insert(*r);
}

RID HashFile::insert(std::span<const std::byte> rec) {
    const Key k = key_of(rec);
    const std::uint32_t bucket = bucket_of(k);
    std::uint32_t blk = bucket + 1, target = 0, last = 0;
    while (blk != 0) {
        auto g = pool_.fetch(BlockId{file_, blk}, kRandomReuse, kNoReservation);
        const auto pg = g.read();
        if (unique_) {
            for (std::uint16_t i = 0; i < sp_nslots(pg); ++i) {
                const auto r = sp_record(pg, i);
                if (!r.empty() && compare_keys(key_of(r), k) == 0)
                    throw DuplicateKey("hash: chave duplicada " + key_to_string(k));
            }
        }
        if (target == 0 && sp_fits(pg, rec.size())) {
            target = blk;
            if (!unique_) break;
        }
        last = blk;
        blk = read_header(pg).next;
    }
    if (target != 0) {
        auto g = pool_.fetch(BlockId{file_, target}, kRandomReuse, kNoReservation);
        return RID{target, *sp_insert(g.write(), rec)};
    }
    auto [bid, g] = pool_.allocate(file_, kNoReservation);   // bucket overflow (43)
    sp_init(g.write(), PageType::Bucket);
    PageHeader h = read_header(g.read());
    h.aux = bucket;
    write_header(g.write(), h);
    const auto slot = sp_insert(g.write(), rec);
    if (!slot) throw std::logic_error("hash: registro maior que o bucket");
    g.reset();
    {
        auto lg = pool_.fetch(BlockId{file_, last}, kRandomReuse, kNoReservation);
        PageHeader lh = read_header(lg.read());
        lh.next = bid.block;
        write_header(lg.write(), lh);
    }
    ++overflow_;
    return RID{bid.block, *slot};
}

void HashFile::erase(RID rid) {
    Key k;
    PageHeader h;
    {
        auto g = pool_.fetch(BlockId{file_, rid.block}, kRandomReuse, kNoReservation);
        const auto r = sp_record(g.read(), rid.slot);
        if (r.empty()) throw std::logic_error("hash erase: registro inexistente");
        k = key_of(r);
        sp_erase(g.write(), rid.slot);
        h = read_header(g.read());
    }
    if (rid.block > nbuckets_ && h.live == 0) {   // overflow vazio: desliga da cadeia e libera (162)
        for (std::uint32_t b = bucket_of(k) + 1; b != 0;) {
            auto pg = pool_.fetch(BlockId{file_, b}, kRandomReuse, kNoReservation);
            PageHeader ph = read_header(pg.read());
            if (ph.next == rid.block) {
                ph.next = h.next;
                write_header(pg.write(), ph);
                break;
            }
            b = ph.next;
        }
        pool_.free_block(BlockId{file_, rid.block});
        --overflow_;
    }
}

std::unique_ptr<SearchState> HashFile::find(const Predicate& p, bool unique, Intent i, ReservationId r) {
    const auto* pt = std::get_if<Point>(&p);
    if (!pt) throw std::logic_error("hash primário só responde a igualdade (128)");
    return std::make_unique<HashSearch>(pool_, file_, schema_, key_, pt->key, bucket_of(pt->key) + 1, unique, i, r);
}

std::optional<RID> HashFile::first_of_group(const Key& k) {
    for (std::uint32_t b = bucket_of(k) + 1; b != 0;) {
        auto g = pool_.fetch(BlockId{file_, b}, kRandomReuse, kNoReservation);
        const auto pg = g.read();
        for (std::uint16_t i = 0; i < sp_nslots(pg); ++i) {
            const auto r = sp_record(pg, i);
            if (!r.empty() && compare_keys(key_of(r), k) == 0) return RID{b, i};
        }
        b = read_header(pg).next;
    }
    return std::nullopt;
}

StructureStats HashFile::measure() {
    StructureStats s;
    s.kind = StructureKind::Hash;
    std::uint64_t records = 0, pages = 0;
    std::uint32_t chain_max = 0, overflow = 0;
    for (std::uint32_t b = 0; b < nbuckets_; ++b) {
        std::uint32_t len = 0;
        for (std::uint32_t blk = b + 1; blk != 0;) {
            auto g = pool_.fetch(BlockId{file_, blk}, kRandomReuse, kNoReservation);
            const PageHeader h = read_header(g.read());
            records += h.live;
            ++len;
            blk = h.next;
        }
        pages += len;
        overflow += len - 1;
        chain_max = std::max(chain_max, len);
    }
    const std::uint32_t cap = sp_capacity(pool_.block_size(), schema_->record_size);
    s.buckets = nbuckets_;
    s.overflow_blocks = overflow;
    s.blocks = nbuckets_ + overflow;
    s.load_factor = static_cast<double>(records) / (static_cast<double>(nbuckets_) * cap);
    s.chain_avg = static_cast<double>(pages) / nbuckets_;
    s.chain_max = chain_max;
    return s;
}

Occupancy HashFile::occupancy() {
    Occupancy o;
    o.data = nbuckets_;
    o.overflow = overflow_;
    o.header = 1;
    return o;
}

std::uint32_t HashFile::data_blocks() { return nbuckets_ + overflow_; }

// ======================================================================= BPlusTreeFile
BPlusTreeFile::BPlusTreeFile(BufferPool& p, FileId data, FileId index, std::shared_ptr<const Schema> s, FieldId key,
                             bool unique, double fill)
    : pool_(p), schema_(std::move(s)), key_(key), fill_(fill),
      engine_(p, data, index, KeyCodec::for_field(schema_->field(key)),
              [sch = schema_, key](std::span<const std::byte> r) { return detail::make_view(r, sch.get()).key(key); },
              unique, PageType::Leaf, schema_->has_var() ? 0u : schema_->record_size) {
    engine_.init_empty();
}

std::unique_ptr<SearchState> BPlusTreeFile::find(const Predicate& p, bool unique, Intent i, ReservationId r) {
    const KeyRange range = as_range(p);
    const bool unique_point = unique && std::holds_alternative<Point>(p);
    const BPlusEngine* eng = &engine_;
    return std::make_unique<LeafRangeSearch>(pool_, engine_.leaf_file(), schema_, key_, range, unique_point, i, r,
                                             [eng, lo = range.lo] { return eng->leaf_for(lo); });
}

StructureStats BPlusTreeFile::measure() {
    const auto shape = engine_.measure();
    StructureStats s;
    s.kind = StructureKind::BTree;
    s.blocks = shape.leaves;
    s.height = shape.height;
    s.internal_blocks = shape.internal_blocks;
    s.leaves = shape.leaves;
    s.distinct = shape.distinct;
    return s;
}

Occupancy BPlusTreeFile::occupancy() {
    const auto shape = engine_.measure();
    Occupancy o;
    o.data = shape.leaves;
    o.header = 2;   // dois arquivos (145)
    o.index_levels = shape.levels;
    return o;
}

// ======================================================================= HashIndex
HashIndex::HashIndex(BufferPool& p, FileId f, KeyCodec kc, std::uint32_t nb, bool unique)
    : pool_(p), file_(f), codec_(kc), nbuckets_(nb), unique_(unique) {
    if (nb == 0) throw std::invalid_argument("índice hash sem buckets");
    for (std::uint32_t b = 0; b < nb; ++b) {
        auto [bid, g] = pool_.allocate(file_, kNoReservation);
        if (bid.block != b + 1) throw std::logic_error("índice hash: buckets precisam ser contíguos");
        sp_init(g.write(), PageType::IndexBucket);
    }
}

void HashIndex::add(const Key& k, RID rid) {
    std::vector<std::byte> e(codec_.size + sizeof(RID));
    codec_.encode(k, e.data());
    std::memcpy(e.data() + codec_.size, &rid, sizeof rid);
    std::uint32_t blk = bucket_of(k) + 1, last = 0;
    while (blk != 0) {
        auto g = pool_.fetch(BlockId{file_, blk}, kRandomReuse, kNoReservation);
        if (sp_insert(g.write(), e)) return;
        last = blk;
        blk = read_header(g.read()).next;
    }
    auto [bid, g] = pool_.allocate(file_, kNoReservation);
    sp_init(g.write(), PageType::IndexBucket);
    sp_insert(g.write(), e);
    g.reset();
    auto lg = pool_.fetch(BlockId{file_, last}, kRandomReuse, kNoReservation);
    PageHeader h = read_header(lg.read());
    h.next = bid.block;
    write_header(lg.write(), h);
    ++overflow_;
}

void HashIndex::remove(const Key& k, RID rid) {
    std::uint32_t prev = 0;
    for (std::uint32_t blk = bucket_of(k) + 1; blk != 0;) {
        auto g = pool_.fetch(BlockId{file_, blk}, kRandomReuse, kNoReservation);
        const auto pg = g.read();
        for (std::uint16_t i = 0; i < sp_nslots(pg); ++i) {
            const auto e = sp_record(pg, i);
            if (e.empty() || compare_keys(codec_.decode(e.data()), k) != 0) continue;
            RID r;
            std::memcpy(&r, e.data() + codec_.size, sizeof r);
            if (r != rid) continue;
            sp_erase(g.write(), i);
            const PageHeader h = read_header(g.read());
            if (blk > nbuckets_ && h.live == 0 && prev != 0) {   // overflow vazio
                g.reset();
                auto pg2 = pool_.fetch(BlockId{file_, prev}, kRandomReuse, kNoReservation);
                PageHeader ph = read_header(pg2.read());
                ph.next = h.next;
                write_header(pg2.write(), ph);
                pg2.reset();
                pool_.free_block(BlockId{file_, blk});
                --overflow_;
            }
            return;
        }
        prev = blk;
        blk = read_header(pg).next;
    }
    throw std::logic_error("índice hash: entrada não encontrada para remoção");
}

void HashIndex::move(const Key& k, RID from, RID to) {
    for (std::uint32_t blk = bucket_of(k) + 1; blk != 0;) {
        auto g = pool_.fetch(BlockId{file_, blk}, kRandomReuse, kNoReservation);
        const auto pg = g.read();
        for (std::uint16_t i = 0; i < sp_nslots(pg); ++i) {
            const auto e = sp_record(pg, i);
            if (e.empty() || compare_keys(codec_.decode(e.data()), k) != 0) continue;
            RID r;
            std::memcpy(&r, e.data() + codec_.size, sizeof r);
            if (r != from) continue;
            std::vector<std::byte> ne(e.begin(), e.end());
            std::memcpy(ne.data() + codec_.size, &to, sizeof to);
            sp_replace(g.write(), i, ne);
            return;
        }
        blk = read_header(pg).next;
    }
    throw std::logic_error("índice hash: entrada não encontrada para mover");
}

std::unique_ptr<RidState> HashIndex::probe(const Predicate& p, Intent i) {
    const auto* pt = std::get_if<Point>(&p);
    if (!pt) throw std::logic_error("índice hash só responde a igualdade (128)");
    return std::make_unique<HashIndexRidState>(pool_, file_, codec_, pt->key, bucket_of(pt->key) + 1, unique_, i);
}

StructureStats HashIndex::measure() {
    StructureStats s;
    s.kind = StructureKind::SecHash;
    std::uint64_t entries = 0, pages = 0;
    std::uint32_t chain_max = 0, overflow = 0;
    for (std::uint32_t b = 0; b < nbuckets_; ++b) {
        std::uint32_t len = 0;
        for (std::uint32_t blk = b + 1; blk != 0;) {
            auto g = pool_.fetch(BlockId{file_, blk}, kRandomReuse, kNoReservation);
            const PageHeader h = read_header(g.read());
            entries += h.live;
            ++len;
            blk = h.next;
        }
        pages += len;
        overflow += len - 1;
        chain_max = std::max(chain_max, len);
    }
    const std::uint32_t cap = sp_capacity(pool_.block_size(), codec_.size + sizeof(RID));
    s.buckets = nbuckets_;
    s.overflow_blocks = overflow;
    s.blocks = nbuckets_ + overflow;
    s.load_factor = static_cast<double>(entries) / (static_cast<double>(nbuckets_) * cap);
    s.chain_avg = static_cast<double>(pages) / nbuckets_;
    s.chain_max = chain_max;
    return s;
}

Occupancy HashIndex::occupancy() {
    Occupancy o;
    o.data = nbuckets_;
    o.overflow = overflow_;
    o.header = 1;
    return o;
}

// ======================================================================= BPlusIndex
BPlusIndex::BPlusIndex(BufferPool& p, FileId f, KeyCodec kc, bool clustering)
    : pool_(p), file_(f), codec_(kc), clustering_(clustering),
      engine_(p, f, f, kc, [kc](std::span<const std::byte> e) { return kc.decode(e.data()); }, true,
              PageType::IndexLeaf, 0) {
    engine_.init_empty();
}

std::uint32_t BPlusIndex::entry_count(std::span<const std::byte> e) const { return load<std::uint32_t>(e.data() + codec_.size); }
std::uint32_t BPlusIndex::entry_overflow(std::span<const std::byte> e) const { return load<std::uint32_t>(e.data() + codec_.size + 4); }
RID BPlusIndex::entry_rid(std::span<const std::byte> e, std::uint32_t i) const {
    const std::size_t off = codec_.size + 8u + 8u * i;
    if (off + 8 > e.size()) throw std::out_of_range("entrada de índice: RID fora da entrada");
    RID r;
    std::memcpy(&r, e.data() + off, sizeof r);
    return r;
}

BPlusIndex::Entry BPlusIndex::decode(std::span<const std::byte> e) const {
    Entry x;
    x.key = codec_.decode(e.data());
    x.count = entry_count(e);
    x.ovf = entry_overflow(e);
    const std::uint32_t n = std::min(x.count, kInline);
    for (std::uint32_t i = 0; i < n; ++i) x.inline_rids.push_back(entry_rid(e, i));
    return x;
}

std::vector<std::byte> BPlusIndex::encode(const Entry& x) const {
    std::vector<std::byte> e(codec_.size + 8u + 8u * x.inline_rids.size());
    codec_.encode(x.key, e.data());
    store<std::uint32_t>(e.data() + codec_.size, x.count);
    store<std::uint32_t>(e.data() + codec_.size + 4, x.ovf);
    for (std::size_t i = 0; i < x.inline_rids.size(); ++i)
        std::memcpy(e.data() + codec_.size + 8 + 8 * i, &x.inline_rids[i], sizeof(RID));
    return e;
}

std::optional<std::pair<RID, BPlusIndex::Entry>> BPlusIndex::lookup(const Key& k) const {
    const auto where = engine_.find_first(k);
    if (!where) return std::nullopt;
    auto g = pool_.fetch(BlockId{file_, where->block}, kRandomReuse, kNoReservation);
    return std::make_pair(*where, decode(sp_record(g.read(), where->slot)));
}

void BPlusIndex::store_entry(RID where, const Entry& e) { engine_.replace(where, encode(e)); }

void BPlusIndex::add(const Key& k, RID rid) {
    auto found = lookup(k);
    if (!found) {
        Entry e{k, 1, 0, {rid}};
        engine_.insert(encode(e));
        return;
    }
    auto& [where, e] = *found;
    if (clustering_) throw std::logic_error("clustering usa set_first");
    if (e.inline_rids.size() < kInline) {
        e.inline_rids.push_back(rid);
        ++e.count;
        store_entry(where, e);
        return;
    }
    const std::uint32_t cap = (pool_.block_size() - kPageHeader) / 8;
    std::uint32_t last = 0;
    for (std::uint32_t b = e.ovf; b != 0;) {
        auto g = pool_.fetch(BlockId{file_, b}, kRandomReuse, kNoReservation);
        last = b;
        b = read_header(g.read()).next;
    }
    bool appended = false;
    if (last != 0) {
        auto g = pool_.fetch(BlockId{file_, last}, kRandomReuse, kNoReservation);
        PageHeader h = read_header(g.read());
        if (h.nslots < cap) {
            std::memcpy(g.write().data() + kPageHeader + 8u * h.nslots, &rid, sizeof rid);
            ++h.nslots;
            write_header(g.write(), h);
            appended = true;
        }
    }
    if (!appended) {
        auto [bid, g] = pool_.allocate(file_, kNoReservation);
        sp_init(g.write(), PageType::Posting);
        PageHeader h = read_header(g.read());
        std::memcpy(g.write().data() + kPageHeader, &rid, sizeof rid);
        h.nslots = 1;
        write_header(g.write(), h);
        g.reset();
        if (last == 0) {
            e.ovf = bid.block;
        } else {
            auto lg = pool_.fetch(BlockId{file_, last}, kRandomReuse, kNoReservation);
            PageHeader lh = read_header(lg.read());
            lh.next = bid.block;
            write_header(lg.write(), lh);
        }
    }
    ++e.count;
    store_entry(where, e);
}

void BPlusIndex::pop_last_overflow(Entry& e, RID& out) {
    std::uint32_t prev = 0, last = 0;
    for (std::uint32_t b = e.ovf; b != 0;) {
        auto g = pool_.fetch(BlockId{file_, b}, kRandomReuse, kNoReservation);
        prev = last;
        last = b;
        b = read_header(g.read()).next;
    }
    if (last == 0) throw std::logic_error("posting list sem overflow");
    bool empty;
    {
        auto g = pool_.fetch(BlockId{file_, last}, kRandomReuse, kNoReservation);
        PageHeader h = read_header(g.read());
        std::memcpy(&out, g.read().data() + kPageHeader + 8u * (h.nslots - 1), sizeof out);
        --h.nslots;
        write_header(g.write(), h);
        empty = h.nslots == 0;
    }
    if (empty) {
        if (prev == 0) {
            e.ovf = 0;
        } else {
            auto pg = pool_.fetch(BlockId{file_, prev}, kRandomReuse, kNoReservation);
            PageHeader ph = read_header(pg.read());
            ph.next = 0;
            write_header(pg.write(), ph);
        }
        pool_.free_block(BlockId{file_, last});
    }
}

void BPlusIndex::remove(const Key& k, RID rid) {
    auto found = lookup(k);
    if (!found) throw std::logic_error("índice B+: chave não encontrada para remoção");
    auto& [where, e] = *found;
    auto it = std::find(e.inline_rids.begin(), e.inline_rids.end(), rid);
    if (it != e.inline_rids.end()) {
        if (e.count > e.inline_rids.size()) {   // repõe com o último do overflow
            RID last;
            pop_last_overflow(e, last);
            *it = last;
        } else {
            e.inline_rids.erase(it);
        }
    } else {
        std::uint32_t pos_b = 0, pos_j = 0;
        bool is_last = false;
        for (std::uint32_t b = e.ovf; b != 0 && pos_b == 0;) {
            auto g = pool_.fetch(BlockId{file_, b}, kRandomReuse, kNoReservation);
            const PageHeader h = read_header(g.read());
            for (std::uint32_t j = 0; j < h.nslots; ++j) {
                RID r;
                std::memcpy(&r, g.read().data() + kPageHeader + 8u * j, sizeof r);
                if (r == rid) {
                    pos_b = b;
                    pos_j = j;
                    is_last = h.next == 0 && j + 1 == h.nslots;
                    break;
                }
            }
            b = h.next;
        }
        if (pos_b == 0) throw std::logic_error("índice B+: RID não encontrado na posting list");
        RID last;
        pop_last_overflow(e, last);
        if (!is_last) {
            auto g = pool_.fetch(BlockId{file_, pos_b}, kRandomReuse, kNoReservation);
            std::memcpy(g.write().data() + kPageHeader + 8u * pos_j, &last, sizeof last);
        }
    }
    --e.count;
    if (e.count == 0) engine_.erase(where);
    else store_entry(where, e);
}

void BPlusIndex::move(const Key& k, RID from, RID to) {
    auto found = lookup(k);
    if (!found) throw std::logic_error("índice B+: chave não encontrada para mover");
    auto& [where, e] = *found;
    if (clustering_) {
        if (!e.inline_rids.empty() && e.inline_rids[0] == from) {
            e.inline_rids[0] = to;
            store_entry(where, e);
        }
        return;
    }
    auto it = std::find(e.inline_rids.begin(), e.inline_rids.end(), from);
    if (it != e.inline_rids.end()) {
        *it = to;
        store_entry(where, e);
        return;
    }
    for (std::uint32_t b = e.ovf; b != 0;) {
        auto g = pool_.fetch(BlockId{file_, b}, kRandomReuse, kNoReservation);
        const PageHeader h = read_header(g.read());
        for (std::uint32_t j = 0; j < h.nslots; ++j) {
            RID r;
            std::memcpy(&r, g.read().data() + kPageHeader + 8u * j, sizeof r);
            if (r == from) {
                std::memcpy(g.write().data() + kPageHeader + 8u * j, &to, sizeof to);
                return;
            }
        }
        b = h.next;
    }
    throw std::logic_error("índice B+: RID não encontrado para mover");
}

std::optional<RID> BPlusIndex::first(const Key& k) {
    auto found = lookup(k);
    if (!found || found->second.inline_rids.empty()) return std::nullopt;
    return found->second.inline_rids[0];
}

void BPlusIndex::set_first(const Key& k, std::optional<RID> rid) {
    auto found = lookup(k);
    if (rid) {
        Entry e{k, 1, 0, {*rid}};
        if (found) store_entry(found->first, e);
        else engine_.insert(encode(e));
    } else if (found) {
        engine_.erase(found->first);
    }
}

std::unique_ptr<RidState> BPlusIndex::probe(const Predicate& p, Intent i) {
    return std::make_unique<BTreeIndexRidState>(*this, as_range(p), i);
}

StructureStats BPlusIndex::measure() {
    const auto shape = engine_.measure();
    std::uint32_t overflow = 0;
    EntryIter it(this, KeyRange{}, kRandomReuse);
    while (auto e = it.next()) {
        for (std::uint32_t b = entry_overflow(e->second); b != 0;) {
            auto g = pool_.fetch(BlockId{file_, b}, kRandomReuse, kNoReservation);
            ++overflow;
            b = read_header(g.read()).next;
        }
    }
    StructureStats s;
    s.kind = StructureKind::SecBTree;
    s.blocks = shape.internal_blocks + shape.leaves + overflow;
    s.height = shape.height;
    s.internal_blocks = shape.internal_blocks;
    s.leaves = shape.leaves;
    s.distinct = shape.distinct;
    s.overflow_blocks = overflow;
    return s;
}

Occupancy BPlusIndex::occupancy() {
    const auto s = measure();
    Occupancy o;
    o.data = *s.leaves;
    o.overflow = *s.overflow_blocks;
    o.header = 1;
    o.index_levels = engine_.measure().levels;
    return o;
}

BPlusIndex::EntryIter::EntryIter(const BPlusIndex* idx, KeyRange range, Intent i)
    : idx_(idx), range_(std::move(range)), intent_(i) {}

std::optional<std::pair<Key, std::span<const std::byte>>> BPlusIndex::EntryIter::next() {
    if (done_) { leaf_.reset(); return std::nullopt; }
    if (!started_) {
        started_ = true;
        next_leaf_ = idx_->engine_.leaf_for(range_.lo);
    }
    for (;;) {
        if (!leaf_) {
            if (next_leaf_ == 0) { done_ = true; return std::nullopt; }
            leaf_.emplace(idx_->pool_.fetch(BlockId{idx_->file_, next_leaf_}, intent_, kNoReservation));
            order_ = sorted_slots(leaf_->read(), idx_->engine_.key_of());
            pos_ = 0;
        }
        while (pos_ < order_.size()) {
            const auto& [k, slot] = order_[pos_++];
            if (range_.below(k)) continue;
            if (range_.above(k)) { done_ = true; return std::nullopt; }
            return std::make_pair(k, sp_record(leaf_->read(), slot));
        }
        next_leaf_ = read_header(leaf_->read()).next;
        leaf_.reset();
    }
}

// ======================================================================= fábricas de busca usadas pelo StorageManager
std::unique_ptr<SearchState> make_clustering_search(const BPlusIndex& idx, PrimaryFile& primary,
                                                    std::shared_ptr<const Schema> schema, FieldId field,
                                                    const Predicate& p, bool unique, Intent i, ReservationId r,
                                                    BufferPool& pool) {
    KeyRange range = as_range(p);
    if (primary.kind() == StructureKind::Hash)
        return std::make_unique<ClusteringHashSearch>(idx, primary.data_file(), std::move(schema), field, range, i, r);
    // B+ primária: o 1º RID do 1º valor na faixa dá a folha de partida; depois, cadeia de folhas
    const bool unique_point = unique && std::holds_alternative<Point>(p);
    const BPlusIndex* ip = &idx;
    return std::make_unique<LeafRangeSearch>(pool, primary.data_file(), std::move(schema), field, range, unique_point,
                                             i, r, [ip, range]() -> std::uint32_t {
                                                 BPlusIndex::EntryIter it(ip, range, kRandomReuse);
                                                 auto e = it.next();
                                                 return e ? ip->entry_rid(e->second, 0).block : 0u;
                                             });
}

}  // namespace cbd

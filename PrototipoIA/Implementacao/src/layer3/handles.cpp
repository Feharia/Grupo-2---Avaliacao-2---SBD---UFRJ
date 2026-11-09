#include "layer3/handles.hpp"

#include <algorithm>
#include <numeric>

namespace cbd {

// ======================================================================= BlockCursor / BlockScan
std::optional<RecordView> BlockCursor::next() {
    const auto page = guard_.read();
    const std::uint16_t n = sp_nslots(page);
    while (slot_ < n) {
        auto r = sp_record(page, slot_++);
        if (!r.empty()) return detail::make_view(r, schema_.get());
    }
    return std::nullopt;
}

BlockScan::BlockScan(BufferPool* p, ScanPlan plan, std::shared_ptr<const Schema> s, Intent i, ReservationId r)
    : pool_(p), plan_(plan), schema_(std::move(s)), intent_(i), res_(r) {}

std::optional<BlockCursor> BlockScan::next() {
    std::uint32_t blk = 0;
    switch (plan_.order) {
        case ScanPlan::Order::Sequential:
            if (done_ >= plan_.count) return std::nullopt;
            blk = plan_.first + done_++;
            break;
        case ScanPlan::Order::LeafChain:
            if (!started_) { next_ = plan_.first; started_ = true; }
            if (next_ == 0) return std::nullopt;
            blk = next_;
            break;
        case ScanPlan::Order::BucketChains:
            if (!started_) { done_ = plan_.first; started_ = true; }
            if (next_ == 0) {
                if (done_ >= plan_.first + plan_.count) return std::nullopt;
                blk = 1 + done_++;   // bucket b vive no bloco b + 1
            } else {
                blk = next_;
            }
            break;
    }
    PageGuard g = pool_->fetch(BlockId{plan_.file, blk}, intent_, res_);
    if (plan_.order != ScanPlan::Order::Sequential) next_ = read_header(g.read()).next;   // lê o seguinte antes
    return BlockCursor(std::move(g), schema_);
}

// ======================================================================= SearchCursor / RidCursor
SearchCursor::SearchCursor(std::unique_ptr<SearchState> s) : state_(std::move(s)) {}
SearchCursor::SearchCursor(SearchCursor&&) noexcept = default;
SearchCursor& SearchCursor::operator=(SearchCursor&&) noexcept = default;
SearchCursor::~SearchCursor() = default;
std::optional<RecordView> SearchCursor::next() { return state_ ? state_->next() : std::nullopt; }

RidCursor::RidCursor(std::unique_ptr<RidState> s) : state_(std::move(s)) {}
RidCursor::RidCursor(RidCursor&&) noexcept = default;
RidCursor& RidCursor::operator=(RidCursor&&) noexcept = default;
RidCursor::~RidCursor() = default;
std::optional<RID> RidCursor::next() { return state_ ? state_->next() : std::nullopt; }

// ======================================================================= TempFile
TempFile::TempFile(BufferPool* p, FileId f, std::shared_ptr<const Schema> s, ReservationId r)
    : pool_(p), file_(f), schema_(std::move(s)), res_(r) {}

TempFile::TempFile(TempFile&& o) noexcept
    : pool_(std::exchange(o.pool_, nullptr)), file_(o.file_), schema_(std::move(o.schema_)), res_(o.res_),
      tail_(std::move(o.tail_)), blocks_(o.blocks_), records_(o.records_), sealed_(o.sealed_) {
    o.tail_.reset();
}
TempFile& TempFile::operator=(TempFile&& o) noexcept {
    if (this != &o) {
        destroy();
        pool_ = std::exchange(o.pool_, nullptr);
        file_ = o.file_;
        schema_ = std::move(o.schema_);
        res_ = o.res_;
        tail_ = std::move(o.tail_);
        o.tail_.reset();
        blocks_ = o.blocks_;
        records_ = o.records_;
        sealed_ = o.sealed_;
    }
    return *this;
}
TempFile::~TempFile() { destroy(); }

void TempFile::destroy() noexcept {
    if (!pool_) return;
    tail_.reset();
    pool_->remove_file(file_);   // descarta sem gravar e sem contar (155)
    pool_ = nullptr;
}

void TempFile::append(std::span<const std::byte> rec) {
    if (sealed_) throw std::logic_error("TempFile::append depois do seal");
    if (!tail_ || !sp_insert(tail_->write(), rec)) {
        tail_.reset();   // solta a página cheia: ela vira candidata no subpool
        auto [bid, g] = pool_->allocate(file_, res_);
        sp_init(g.write(), PageType::Temp);
        if (!sp_insert(g.write(), rec)) throw std::logic_error("registro maior que o bloco");
        tail_.emplace(std::move(g));
        ++blocks_;
    }
    ++records_;
}

void TempFile::seal() {
    if (sealed_) return;
    tail_.reset();
    pool_->flush_file(file_, true);   // grava as páginas sujas, conta como temp e as retira (139)
    sealed_ = true;
}

BlockScan TempFile::scan_blocks(Intent intent) {
    if (!sealed_) throw std::logic_error("TempFile::scan_blocks antes do seal");
    return BlockScan(pool_, ScanPlan{ScanPlan::Order::Sequential, file_, 1, blocks_}, schema_, intent, res_);
}

// ======================================================================= ScratchSpace
ScratchSpace::ScratchSpace(std::vector<PageGuard> pages, std::shared_ptr<const Schema> s, std::uint32_t bs)
    : pages_(std::move(pages)), schema_(std::move(s)), bs_(bs) {
    const std::size_t per_page = bs_ / (static_cast<std::size_t>(schema_->record_size) + 2);
    capacity_ = per_page * pages_.size();
}
ScratchSpace::ScratchSpace(ScratchSpace&&) noexcept = default;
ScratchSpace& ScratchSpace::operator=(ScratchSpace&&) noexcept = default;
ScratchSpace::~ScratchSpace() = default;   // os guards devolvem as molduras ao subpool

bool ScratchSpace::add_bytes(std::span<const std::byte> rec) {
    const std::size_t need = rec.size() + 2;
    if (need > bs_) throw std::logic_error("registro maior que a página de trabalho");
    while (page_ < pages_.size() && used_ + need > bs_) {
        ++page_;
        used_ = 0;
    }
    if (page_ >= pages_.size()) return false;
    std::byte* dst = pages_[page_].write().data() + used_;
    store<std::uint16_t>(dst, static_cast<std::uint16_t>(rec.size()));
    std::memcpy(dst + 2, rec.data(), rec.size());
    slots_.push_back((page_ << 16) | used_);
    used_ += static_cast<std::uint32_t>(need);
    heads_.clear();
    chain_.clear();
    return true;
}

bool ScratchSpace::add(const RecordView& r) { return add_bytes(r.bytes()); }

bool ScratchSpace::add(RID rid) {
    std::byte buf[8];
    store<std::int64_t>(buf, rid_to_int(rid));
    return add_bytes(std::span<const std::byte>(buf, 8));
}

std::span<const std::byte> ScratchSpace::item(std::size_t i) const {
    const std::uint32_t s = slots_.at(i);
    const std::byte* base = pages_[s >> 16].read().data() + (s & 0xFFFF);
    return {base + 2, load<std::uint16_t>(base)};
}

RecordView ScratchSpace::at(std::size_t i) const { return detail::make_view(item(i), schema_.get()); }
RID ScratchSpace::rid_at(std::size_t i) const { return int_to_rid(load<std::int64_t>(item(i).data())); }

void ScratchSpace::sort_by(FieldId f) {
    std::vector<Key> keys;
    keys.reserve(slots_.size());
    for (std::size_t i = 0; i < slots_.size(); ++i) keys.push_back(at(i).key(f));
    std::vector<std::uint32_t> idx(slots_.size());
    std::iota(idx.begin(), idx.end(), 0u);
    std::stable_sort(idx.begin(), idx.end(), [&](std::uint32_t a, std::uint32_t b) {
        return compare_keys(keys[a], keys[b]) < 0;
    });
    std::vector<std::uint32_t> sorted;
    sorted.reserve(slots_.size());
    for (auto i : idx) sorted.push_back(slots_[i]);
    slots_ = std::move(sorted);
    heads_.clear();
    chain_.clear();
}

void ScratchSpace::sort_rids() {
    std::vector<std::pair<std::int64_t, std::uint32_t>> v;
    v.reserve(slots_.size());
    for (std::size_t i = 0; i < slots_.size(); ++i) v.emplace_back(load<std::int64_t>(item(i).data()), slots_[i]);
    std::sort(v.begin(), v.end());
    for (std::size_t i = 0; i < v.size(); ++i) slots_[i] = v[i].second;
}

void ScratchSpace::build_hash(FieldId f, std::uint64_t seed) {
    std::size_t n = 1;
    while (n < slots_.size()) n <<= 1;
    heads_.assign(n, 0);
    chain_.assign(slots_.size(), 0);
    hash_field_ = f;
    hash_seed_ = seed;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        const RecordView r = at(i);
        if (r.is_null(f)) continue;   // chave NULL nunca casa (158)
        const std::size_t h = hash_key(r.key(f), seed) & (n - 1);
        chain_[i] = heads_[h];
        heads_[h] = static_cast<std::uint32_t>(i + 1);
    }
}

ScratchMatches ScratchSpace::probe(const Key& k) const {
    if (heads_.empty()) return ScratchMatches(this, k, 0);
    const std::size_t h = hash_key(k, hash_seed_) & (heads_.size() - 1);
    return ScratchMatches(this, k, heads_[h]);
}

void ScratchSpace::clear() {
    slots_.clear();
    heads_.clear();
    chain_.clear();
    page_ = 0;
    used_ = 0;
}

std::optional<RecordView> ScratchMatches::next() {
    while (cur_ != 0) {
        const std::size_t i = cur_ - 1;
        cur_ = s_->chain_[i];
        const RecordView r = s_->at(i);
        if (compare_keys(r.key(s_->hash_field_), key_) == 0) return r;
    }
    return std::nullopt;
}

}  // namespace cbd

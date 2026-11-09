#include "layer4/buffer_pool.hpp"

#include <algorithm>

namespace cbd {

// ======================================================================= PageGuard
PageGuard::PageGuard(BufferPool* p, FrameId f, Intent i) : pool_(p), frame_(f), intent_(i) {}
PageGuard::PageGuard(PageGuard&& o) noexcept
    : pool_(std::exchange(o.pool_, nullptr)), frame_(o.frame_), intent_(o.intent_) {}
PageGuard& PageGuard::operator=(PageGuard&& o) noexcept {
    if (this != &o) {
        reset();
        pool_ = std::exchange(o.pool_, nullptr);
        frame_ = o.frame_;
        intent_ = o.intent_;
    }
    return *this;
}
PageGuard::~PageGuard() { reset(); }
void PageGuard::reset() noexcept {
    if (pool_) std::exchange(pool_, nullptr)->unpin(frame_, intent_);
}
std::span<const std::byte> PageGuard::read() const {
    if (!pool_) throw std::logic_error("PageGuard vazio");
    return pool_->bytes(frame_);
}
std::span<std::byte> PageGuard::write() {
    if (!pool_) throw std::logic_error("PageGuard vazio");
    auto& fr = pool_->frames_[frame_];
    if (!fr.anonymous) fr.dirty = true;
    return pool_->bytes(frame_);
}
BlockId PageGuard::id() const {
    if (!pool_) return {};
    const auto& fr = pool_->frames_[frame_];
    return fr.anonymous ? BlockId{} : fr.id;
}
bool PageGuard::anonymous() const { return pool_ && pool_->frames_[frame_].anonymous; }

// ======================================================================= Reservation
Reservation::Reservation(BufferPool* p, ReservationId id) : pool_(p), id_(id) {}
Reservation::Reservation(Reservation&& o) noexcept : pool_(std::exchange(o.pool_, nullptr)), id_(o.id_) {}
Reservation& Reservation::operator=(Reservation&& o) noexcept {
    if (this != &o) {
        if (pool_) pool_->release(id_);
        pool_ = std::exchange(o.pool_, nullptr);
        id_ = o.id_;
    }
    return *this;
}
Reservation::~Reservation() {
    if (pool_) pool_->release(id_);
}
std::size_t Reservation::capacity() const { return pool_ ? pool_->subpools_.at(id_).frames.size() : 0; }
std::size_t Reservation::pinned() const { return pool_ ? pool_->subpools_.at(id_).pinned : 0; }
std::size_t Reservation::peak_pinned() const { return pool_ ? pool_->subpools_.at(id_).peak : 0; }

// ======================================================================= BufferPool
BufferPool::BufferPool(const Config& cfg) : cfg_(cfg), files_(cfg) {
    const std::uint32_t n = cfg.frames();
    if (n == 0) throw std::invalid_argument("buffer pool sem molduras");
    arena_.assign(static_cast<std::size_t>(n) * cfg.block_size, std::byte{0});
    frames_.resize(n);
    FrameId f = 0;
    for (std::size_t a = 0; a < 4; ++a) {
        areas_[a].policy = make_policy(cfg.policy, cfg.toss_immediate);
        for (std::uint32_t i = 0; i < cfg.area_frames[a]; ++i, ++f) {
            frames_[f].area = static_cast<Area>(a);
            areas_[a].frames.push_back(f);
            areas_[a].free.push_back(f);
        }
        std::reverse(areas_[a].free.begin(), areas_[a].free.end());   // usa as molduras em ordem
    }
}

BufferPool::~BufferPool() {
    try {
        for (FrameId f = 0; f < frames_.size(); ++f) {
            auto& fr = frames_[f];
            if (fr.used && fr.dirty && !fr.anonymous) files_.write_block(fr.id, bytes(f));
        }
    } catch (...) {
    }
}

Area BufferPool::area_of(FileKind k) {
    switch (k) {
        case FileKind::Data: return Area::Data;
        case FileKind::Index: return Area::Index;
        case FileKind::Catalog: return Area::Catalog;
        case FileKind::Temp: return Area::Work;
    }
    return Area::Work;
}

void BufferPool::check_reservation(ReservationId r) const {
    if (r != kNoReservation && !subpools_.count(r))
        throw std::logic_error("reserva inexistente ou já liberada (155)");
}

BufferPool::Region& BufferPool::region_for(ReservationId r, FileKind k) {
    if (r != kNoReservation) return subpools_.at(r);
    return areas_[static_cast<std::size_t>(area_of(k))];
}
BufferPool::Region& BufferPool::region_of(const Frame& fr) {
    if (fr.res != kNoReservation) return subpools_.at(fr.res);
    return areas_[static_cast<std::size_t>(fr.area)];
}

void BufferPool::bump(FileId f, std::uint64_t IoCounts::*field) {
    counters_.global.*field += 1;
    counters_.per_kind[static_cast<std::size_t>(files_.kind(f))].*field += 1;
    counters_.per_file[f].*field += 1;
}

FileId BufferPool::create(const std::string& name, FileKind k) { return files_.create(name, k); }
FileId BufferPool::open(const std::string& name, FileKind k) { return files_.open(name, k); }
std::uint32_t BufferPool::header_blocks(FileId f) const { return files_.header_blocks(f); }
std::uint32_t BufferPool::file_blocks(FileId f) const { return files_.blocks(f); }
FileKind BufferPool::file_kind(FileId f) const { return files_.kind(f); }

FrameId BufferPool::obtain_frame(Region& r) {
    if (!r.free.empty()) {
        const FrameId f = r.free.back();
        r.free.pop_back();
        return f;
    }
    auto v = r.policy->pick_victim();
    if (!v) throw std::runtime_error("todas as molduras da região estão fixadas");
    evict(*v);
    return *v;
}

void BufferPool::evict(FrameId f) {
    Frame& fr = frames_[f];
    if (fr.dirty) {
        files_.write_block(fr.id, bytes(f));
        bump(fr.id.file, &IoCounts::dirty_evictions);
    }
    page_table_.erase(fr.id);
    fr.used = false;
    fr.dirty = false;
    fr.id = {};
}

void BufferPool::drop(FrameId f) noexcept {
    Frame& fr = frames_[f];
    Region& r = region_of(fr);
    r.policy->on_remove(f);
    if (fr.used) page_table_.erase(fr.id);
    fr.used = false;
    fr.dirty = false;
    fr.anonymous = false;
    fr.id = {};
    r.free.push_back(f);
}

void BufferPool::pin(FrameId f) {
    Frame& fr = frames_[f];
    Region& r = region_of(fr);
    if (fr.pins == 0) {
        r.policy->on_access(f);
        ++r.pinned;
        r.peak = std::max(r.peak, r.pinned);
    }
    ++fr.pins;
}

void BufferPool::unpin(FrameId f, Intent intent) noexcept {
    Frame& fr = frames_[f];
    if (fr.pins == 0) fatal("unpin sem pin correspondente");
    if (--fr.pins > 0) return;
    Region& r = region_of(fr);
    --r.pinned;
    if (fr.anonymous) {
        fr.anonymous = false;
        r.free.push_back(f);
    } else {
        r.policy->on_unpin(f, intent);
    }
}

PageGuard BufferPool::fetch(BlockId b, Intent intent, ReservationId res) {
    check_reservation(res);
    if (b.block == 0) throw std::logic_error("fetch do bloco 0 (cabeçalho)");
    if (b.block >= files_.blocks(b.file)) throw std::out_of_range("fetch: bloco fora do arquivo");
    const FileKind kind = files_.kind(b.file);
    Region& target = region_for(res, kind);
    bump(b.file, &IoCounts::requests);
    if (res == kNoReservation && kind == FileKind::Data) bump(b.file, &IoCounts::data_area_requests);

    if (auto it = page_table_.find(b); it != page_table_.end()) {
        const FrameId f = it->second;
        Frame& fr = frames_[f];
        if (&region_of(fr) != &target) {   // migração para a região pedida (150)
            if (fr.pins > 0) throw std::logic_error("fetch: página fixada em outra região");
            const FrameId nf = obtain_frame(target);
            std::copy_n(bytes(f).data(), cfg_.block_size, bytes(nf).data());
            Frame& nfr = frames_[nf];
            nfr.id = b;
            nfr.used = true;
            nfr.dirty = fr.dirty;
            nfr.anonymous = false;
            fr.dirty = false;
            drop(f);
            page_table_[b] = nf;
            bump(b.file, &IoCounts::hits);
            pin(nf);
            return PageGuard(this, nf, intent);
        }
        bump(b.file, &IoCounts::hits);
        pin(f);
        return PageGuard(this, f, intent);
    }

    bump(b.file, &IoCounts::misses);
    const FrameId f = obtain_frame(target);
    Frame& fr = frames_[f];
    try {
        files_.read_block(b, bytes(f));
    } catch (...) {
        target.free.push_back(f);
        throw;
    }
    fr.id = b;
    fr.used = true;
    fr.dirty = false;
    fr.anonymous = false;
    page_table_[b] = f;
    pin(f);
    if (cfg_.prefetch && intent.sequential) prefetch(b, target, intent);
    return PageGuard(this, f, intent);
}

void BufferPool::prefetch(BlockId start, Region& r, Intent intent) {
    const std::uint32_t nb = files_.blocks(start.file);
    for (std::uint32_t k = 1; k < cfg_.extent_blocks; ++k) {
        const BlockId b{start.file, start.block + k};
        if (b.block >= nb) break;
        if (page_table_.count(b)) continue;
        if (r.free.empty() && r.policy->candidates() == 0) break;   // só expulsa páginas não fixadas (150)
        const FrameId f = obtain_frame(r);
        files_.read_block(b, bytes(f));
        Frame& fr = frames_[f];
        fr.id = b;
        fr.used = true;
        fr.dirty = false;
        page_table_[b] = f;
        r.policy->on_unpin(f, intent);
    }
}

std::pair<BlockId, PageGuard> BufferPool::allocate(FileId file, ReservationId res) {
    check_reservation(res);
    const FileKind kind = files_.kind(file);
    Region& target = region_for(res, kind);
    const std::uint32_t blk = files_.allocate_block(file);
    const BlockId b{file, blk};
    if (page_table_.count(b)) throw std::logic_error("allocate: bloco novo já estava no pool");
    FrameId f;
    try {
        f = obtain_frame(target);
    } catch (...) {
        files_.free_block(file, blk);
        throw;
    }
    std::fill_n(bytes(f).data(), cfg_.block_size, std::byte{0});
    Frame& fr = frames_[f];
    fr.id = b;
    fr.used = true;
    fr.dirty = true;
    fr.anonymous = false;
    page_table_[b] = f;
    bump(file, &IoCounts::allocations);   // allocate não soma em requests (138)
    pin(f);
    return {b, PageGuard(this, f, kRandomReuse)};
}

PageGuard BufferPool::scratch(ReservationId res) {
    if (res == kNoReservation) throw std::logic_error("scratch exige reserva");
    check_reservation(res);
    Region& r = subpools_.at(res);
    const FrameId f = obtain_frame(r);
    Frame& fr = frames_[f];
    fr.used = false;
    fr.dirty = false;
    fr.anonymous = true;
    fr.id = {};
    pin(f);
    return PageGuard(this, f, kRandomOnce);
}

void BufferPool::free_block(BlockId b) {
    if (b.block == 0) throw std::logic_error("free_block do bloco 0 (168)");
    if (auto it = page_table_.find(b); it != page_table_.end()) {
        if (frames_[it->second].pins > 0) throw std::logic_error("free_block de página fixada (168)");
        drop(it->second);   // descarta sem gravar
    }
    files_.free_block(b.file, b.block);
}

void BufferPool::flush_file(FileId file, bool evict_pages) {
    std::vector<FrameId> mine;
    for (const auto& [b, f] : page_table_)
        if (b.file == file) mine.push_back(f);
    std::sort(mine.begin(), mine.end(), [&](FrameId a, FrameId c) { return frames_[a].id < frames_[c].id; });
    for (FrameId f : mine) {
        Frame& fr = frames_[f];
        if (fr.dirty) {
            files_.write_block(fr.id, bytes(f));
            bump(file, &IoCounts::forced_writes);
            fr.dirty = false;
        }
        if (evict_pages) {
            if (fr.pins > 0) throw std::logic_error("flush_file com retirada de página fixada");
            drop(f);
        }
    }
}

void BufferPool::remove_file(FileId file) noexcept {
    std::vector<FrameId> mine;
    for (const auto& [b, f] : page_table_)
        if (b.file == file) mine.push_back(f);
    for (FrameId f : mine) {
        if (frames_[f].pins > 0) fatal("remove_file com página fixada (155)");
        frames_[f].dirty = false;
        drop(f);
    }
    files_.remove(file);
}

Reservation BufferPool::reserve(std::size_t n) {
    if (n == 0) throw std::invalid_argument("reserve(0)");
    Region& work = areas_[static_cast<std::size_t>(Area::Work)];
    while (work.free.size() < n) {
        auto v = work.policy->pick_victim();
        if (!v) break;
        evict(*v);
        work.free.push_back(*v);
    }
    if (work.free.size() < n) throw std::runtime_error("reserve: molduras livres insuficientes na área de trabalho");
    const ReservationId id = next_res_++;
    Region sub;
    sub.policy = make_policy(cfg_.policy, cfg_.toss_immediate);
    for (std::size_t i = 0; i < n; ++i) {
        const FrameId f = work.free.back();
        work.free.pop_back();
        frames_[f].res = id;
        sub.frames.push_back(f);
        sub.free.push_back(f);
    }
    std::reverse(sub.free.begin(), sub.free.end());
    subpools_.emplace(id, std::move(sub));
    return Reservation(this, id);
}

void BufferPool::release(ReservationId id) noexcept {
    auto it = subpools_.find(id);
    if (it == subpools_.end()) return;
    Region& work = areas_[static_cast<std::size_t>(Area::Work)];
    for (FrameId f : it->second.frames) {
        Frame& fr = frames_[f];
        if (fr.pins > 0) fatal("reserva liberada com página fixada (155)");
        if (fr.used) {
            if (fr.dirty) {
                try {
                    files_.write_block(fr.id, bytes(f));
                    bump(fr.id.file, &IoCounts::dirty_evictions);
                } catch (...) {
                    fatal("falha ao gravar página suja ao liberar a reserva");
                }
            }
            page_table_.erase(fr.id);
        }
        fr = Frame{};
        fr.area = Area::Work;
        work.free.push_back(f);
    }
    subpools_.erase(it);
}

Counters BufferPool::snapshot() const {
    Counters c = counters_;
    files_.add_counts(c);
    return c;
}

std::uint64_t BufferPool::flush_all() {
    std::uint64_t n = 0;
    for (FrameId f = 0; f < frames_.size(); ++f) {
        Frame& fr = frames_[f];
        if (fr.used && fr.dirty && !fr.anonymous) {
            files_.write_block(fr.id, bytes(f));
            bump(fr.id.file, &IoCounts::forced_writes);
            fr.dirty = false;
            ++n;
        }
    }
    n += files_.flush_headers();
    return n;
}

void BufferPool::invalidate_all() {
    if (!subpools_.empty()) throw std::logic_error("invalidate_all com reserva ativa");
    for (FrameId f = 0; f < frames_.size(); ++f) {
        const Frame& fr = frames_[f];
        if (fr.area == Area::Catalog || !fr.used) continue;
        if (fr.pins > 0) throw std::logic_error("invalidate_all com página fixada fora do catálogo");
        if (fr.dirty) throw std::logic_error("invalidate_all exige flush prévio");
    }
    for (FrameId f = 0; f < frames_.size(); ++f) {
        const Frame& fr = frames_[f];
        if (fr.area != Area::Catalog && fr.used) drop(f);
    }
}

}  // namespace cbd

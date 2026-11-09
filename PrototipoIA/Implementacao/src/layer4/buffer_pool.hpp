// Camada 4 — Gerente de buffer pool: áreas por tipo de página e subpools por reserva (8, 9, 136).
#pragma once

#include "common/common.hpp"
#include "layer4/file_manager.hpp"
#include "layer4/replacement_policy.hpp"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__clang__)
#  define LIFETIMEBOUND [[clang::lifetimebound]]
#else
#  define LIFETIMEBOUND
#endif

namespace cbd {

class BufferPool;

// Um pin numa página: dá acesso aos bytes e solta o pin quando destruído.
class PageGuard {
public:
    PageGuard() = default;
    PageGuard(PageGuard&& o) noexcept;
    PageGuard& operator=(PageGuard&& o) noexcept;
    PageGuard(const PageGuard&) = delete;
    PageGuard& operator=(const PageGuard&) = delete;
    ~PageGuard();
    std::span<const std::byte> read() const LIFETIMEBOUND;
    std::span<std::byte>       write() LIFETIMEBOUND;   // marca suja (exceto anônima)
    BlockId id() const;
    bool    anonymous() const;
    bool    valid() const { return pool_ != nullptr; }
    void    reset() noexcept;
private:
    friend class BufferPool;
    PageGuard(BufferPool*, FrameId, Intent);
    BufferPool* pool_ = nullptr;
    FrameId     frame_{};
    Intent      intent_{};
};

// O subpool de um operador: m molduras exclusivas, com política própria (107, 136).
class Reservation {
public:
    Reservation(Reservation&& o) noexcept;
    Reservation& operator=(Reservation&& o) noexcept;
    Reservation(const Reservation&) = delete;
    Reservation& operator=(const Reservation&) = delete;
    ~Reservation();                        // noexcept
    ReservationId id() const { return id_; }
    std::size_t   capacity() const;
    std::size_t   pinned() const;
    std::size_t   peak_pinned() const;     // maior nº de molduras fixadas ao mesmo tempo
private:
    friend class BufferPool;
    Reservation(BufferPool*, ReservationId);
    BufferPool*   pool_ = nullptr;
    ReservationId id_ = 0;
};

class BufferPool {
public:
    explicit BufferPool(const Config&);
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    ~BufferPool();

    // acesso
    FileId        create(const std::string& name, FileKind);   // falha se existir (161)
    FileId        open(const std::string& name, FileKind);     // falha se não existir (161)
    PageGuard     fetch(BlockId, Intent, ReservationId);       // sem valor padrão (159)
    std::pair<BlockId, PageGuard> allocate(FileId, ReservationId);
    PageGuard     scratch(ReservationId);
    void          free_block(BlockId);
    void          flush_file(FileId, bool evict);
    void          remove_file(FileId) noexcept;
    std::uint32_t header_blocks(FileId) const;
    std::uint32_t file_blocks(FileId) const;    // inclui o cabeçalho
    FileKind      file_kind(FileId) const;
    std::uint32_t block_size() const { return cfg_.block_size; }
    const Config& config() const { return cfg_; }

    // controle
    Reservation   reserve(std::size_t frames);
    Counters      snapshot() const;
    std::uint64_t flush_all();
    void          invalidate_all();

private:
    friend class PageGuard;
    friend class Reservation;
    struct Frame {
        BlockId       id{};
        bool          used = false;        // contém um bloco
        bool          anonymous = false;   // memória de trabalho (ScratchSpace)
        bool          dirty = false;
        std::uint32_t pins = 0;
        Area          area = Area::Work;
        ReservationId res = 0;
    };
    struct Region {
        std::vector<FrameId> frames;
        std::vector<FrameId> free;
        std::unique_ptr<ReplacementPolicy> policy;
        std::size_t pinned = 0, peak = 0;
    };
    static Area area_of(FileKind k);
    Region& region_for(ReservationId, FileKind);
    Region& region_of(const Frame&);
    FrameId obtain_frame(Region&);
    void    evict(FrameId);
    void    drop(FrameId) noexcept;          // esvazia sem gravar e devolve à lista livre
    void    pin(FrameId);
    void    unpin(FrameId, Intent) noexcept;
    void    release(ReservationId) noexcept;
    void    prefetch(BlockId start, Region&, Intent);
    void    bump(FileId, std::uint64_t IoCounts::*field);
    void    check_reservation(ReservationId) const;
    std::span<std::byte> bytes(FrameId f) {
        return {arena_.data() + static_cast<std::size_t>(f) * cfg_.block_size, cfg_.block_size};
    }

    Config                 cfg_;
    FileManager            files_;
    std::vector<std::byte> arena_;
    std::vector<Frame>     frames_;
    std::unordered_map<BlockId, FrameId, BlockIdHash> page_table_;
    std::array<Region, 4>  areas_;
    std::unordered_map<ReservationId, Region> subpools_;
    ReservationId          next_res_ = 1;
    Counters               counters_;
};

}  // namespace cbd

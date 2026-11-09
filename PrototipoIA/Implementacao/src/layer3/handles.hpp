// Camada 3 — handles entregues à camada 2: cursores, registro fixado, temporários e memória de trabalho.
#pragma once

#include "common/common.hpp"
#include "layer3/page.hpp"
#include "layer3/record.hpp"
#include "layer4/buffer_pool.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace cbd {

class StorageManager;
class TempFile;
class ScratchSpace;

struct ScanPlan {
    enum class Order : std::uint8_t { Sequential, LeafChain, BucketChains } order = Order::Sequential;
    FileId        file = 0;
    std::uint32_t first = 1;   // 1º bloco de dados (≥ 1), 1ª folha ou 1º bucket
    std::uint32_t count = 0;   // nº de blocos ou de buckets
};

// Mantém um bloco fixado e percorre os registros válidos dele.
class BlockCursor {
public:
    BlockCursor(BlockCursor&&) noexcept = default;
    BlockCursor& operator=(BlockCursor&&) noexcept = default;
    BlockCursor(const BlockCursor&) = delete;
    BlockCursor& operator=(const BlockCursor&) = delete;
    std::optional<RecordView> next();
    BlockId block() const { return guard_.id(); }
    RID     last_rid() const { return RID{guard_.id().block, static_cast<std::uint16_t>(slot_ - 1)}; }
private:
    friend class BlockScan;
    BlockCursor(PageGuard g, std::shared_ptr<const Schema> s) : guard_(std::move(g)), schema_(std::move(s)) {}
    PageGuard                     guard_;
    std::shared_ptr<const Schema> schema_;   // (165)
    std::uint16_t                 slot_ = 0;
};

// Percorre os blocos de um arquivo na ordem física da organização; não lê nada até o 1º next().
class BlockScan {
public:
    BlockScan(BlockScan&&) noexcept = default;
    BlockScan& operator=(BlockScan&&) noexcept = default;
    BlockScan(const BlockScan&) = delete;
    BlockScan& operator=(const BlockScan&) = delete;
    std::optional<BlockCursor> next();
private:
    friend class StorageManager;
    friend class TempFile;
    BlockScan(BufferPool*, ScanPlan, std::shared_ptr<const Schema>, Intent, ReservationId);
    BufferPool*                   pool_;
    ScanPlan                      plan_;
    std::shared_ptr<const Schema> schema_;   // (165)
    Intent                        intent_;
    ReservationId                 res_;
    std::uint32_t                 done_ = 0;       // Sequential: blocos entregues; BucketChains: próximo bucket
    std::uint32_t                 next_ = 0;       // próximo bloco da cadeia (0 = acabou a cadeia)
    bool                          started_ = false;
};

// Estado polimórfico das buscas (implementado por cada organização e pelo clustering).
struct SearchState {
    virtual ~SearchState() = default;
    virtual std::optional<RecordView> next() = 0;
protected:
    static RecordView make_view(std::span<const std::byte> b, const Schema* s) { return detail::make_view(b, s); }
};
struct RidState {
    virtual ~RidState() = default;
    virtual std::optional<RID> next() = 0;
};

class SearchCursor {
public:
    SearchCursor(SearchCursor&&) noexcept;
    SearchCursor& operator=(SearchCursor&&) noexcept;
    SearchCursor(const SearchCursor&) = delete;
    ~SearchCursor();
    std::optional<RecordView> next();   // o registro vale até a próxima chamada
private:
    friend class StorageManager;
    explicit SearchCursor(std::unique_ptr<SearchState>);
    std::unique_ptr<SearchState> state_;
};

class RidCursor {
public:
    RidCursor(RidCursor&&) noexcept;
    RidCursor& operator=(RidCursor&&) noexcept;
    RidCursor(const RidCursor&) = delete;
    ~RidCursor();
    std::optional<RID> next();
private:
    friend class StorageManager;
    explicit RidCursor(std::unique_ptr<RidState>);
    std::unique_ptr<RidState> state_;
};

struct PinnedRecord {
    PageGuard  guard;   // declarado antes: destruído depois da visão
    RecordView view;
};

// Arquivo temporário para runs e partições: selado antes de ser lido, apagado no destrutor (118, 139).
class TempFile {
public:
    TempFile(TempFile&&) noexcept;
    TempFile& operator=(TempFile&&) noexcept;
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    ~TempFile();                                // noexcept; página fixada aqui é erro fatal
    void          append(const RecordView& r) { append(r.bytes()); }
    void          append(std::span<const std::byte> rec);
    void          seal();
    bool          sealed() const { return sealed_; }
    std::uint32_t blocks() const { return blocks_; }
    std::uint64_t records() const { return records_; }
    BlockScan     scan_blocks(Intent);          // usa a reserva do próprio arquivo
private:
    friend class StorageManager;
    TempFile(BufferPool*, FileId, std::shared_ptr<const Schema>, ReservationId);
    void destroy() noexcept;
    BufferPool*                   pool_ = nullptr;
    FileId                        file_ = 0;
    std::shared_ptr<const Schema> schema_;   // cópia própria (165)
    ReservationId                 res_ = 0;
    std::optional<PageGuard>      tail_;
    std::uint32_t                 blocks_ = 0;
    std::uint64_t                 records_ = 0;
    bool                          sealed_ = false;
};

class ScratchMatches {
public:
    std::optional<RecordView> next();
private:
    friend class ScratchSpace;
    ScratchMatches(const ScratchSpace* s, Key k, std::uint32_t head) : s_(s), key_(std::move(k)), cur_(head) {}
    const ScratchSpace* s_;
    Key                 key_;
    std::uint32_t       cur_;   // 1 + índice; 0 = fim
};

// Memória de trabalho do operador em molduras anônimas da reserva (140): nunca vai ao disco.
class ScratchSpace {
public:
    ScratchSpace(ScratchSpace&&) noexcept;
    ScratchSpace& operator=(ScratchSpace&&) noexcept;
    ScratchSpace(const ScratchSpace&) = delete;
    ScratchSpace& operator=(const ScratchSpace&) = delete;
    ~ScratchSpace();                                      // noexcept
    bool           add(const RecordView& r);              // false quando cheio
    bool           add(RID rid);                          // só no espaço de RIDs
    std::size_t    size() const { return slots_.size(); }
    std::size_t    capacity() const { return capacity_; }
    RecordView     at(std::size_t i) const;
    RID            rid_at(std::size_t i) const;
    void           sort_by(FieldId);
    void           sort_rids();                           // por (bloco, slot)
    void           build_hash(FieldId, std::uint64_t seed);
    ScratchMatches probe(const Key&) const;
    void           clear();
    std::size_t    pages() const { return pages_.size(); }
private:
    friend class StorageManager;
    friend class ScratchMatches;
    ScratchSpace(std::vector<PageGuard> pages, std::shared_ptr<const Schema>, std::uint32_t block_size);
    bool add_bytes(std::span<const std::byte>);
    std::span<const std::byte> item(std::size_t i) const;
    std::vector<PageGuard>        pages_;
    std::shared_ptr<const Schema> schema_;                // cópia própria (165); sintético no espaço de RIDs
    std::uint32_t                 bs_ = 0;
    std::vector<std::uint32_t>    slots_;                 // (página << 16) | deslocamento; item = [u16 tam][bytes]
    std::vector<std::uint32_t>    heads_, chain_;
    FieldId                       hash_field_{};
    std::uint64_t                 hash_seed_ = 0;
    std::uint32_t                 page_ = 0, used_ = 0;
    std::size_t                   capacity_ = 0;
};

}  // namespace cbd

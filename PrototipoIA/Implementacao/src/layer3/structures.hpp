// Camada 3 (interno) — organizações primárias e índices secundários.
#pragma once

#include "common/common.hpp"
#include "layer3/bplus.hpp"
#include "layer3/handles.hpp"
#include "layer3/page.hpp"
#include "layer3/record.hpp"
#include "layer4/buffer_pool.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace cbd {

using MoveFn = BPlusEngine::MoveFn;

// ======================================================================= organizações primárias
class PrimaryFile {
public:
    virtual ~PrimaryFile() = default;
    virtual StructureKind kind() const = 0;
    virtual FileId        data_file() const = 0;
    virtual std::vector<FileId> files() const = 0;
    virtual void          bulk_load(RecordStream&) = 0;
    virtual RID           insert(std::span<const std::byte>) = 0;
    virtual void          erase(RID) = 0;
    virtual ScanPlan      scan_plan() const = 0;
    virtual std::unique_ptr<SearchState> find(const Predicate&, bool unique, Intent, ReservationId) = 0;
    virtual std::optional<RID> first_of_group(const Key&) = 0;   // manutenção do clustering
    virtual StructureStats measure() = 0;
    virtual Occupancy      occupancy() = 0;
    virtual std::uint32_t  data_blocks() = 0;                     // b (119)
    virtual FieldId        key_field() const { return 0; }
    void set_on_move(MoveFn f) { on_move_ = std::move(f); on_move_changed(); }
protected:
    virtual void on_move_changed() {}
    MoveFn on_move_;
};

class HeapFile final : public PrimaryFile {
public:
    HeapFile(BufferPool&, FileId, std::shared_ptr<const Schema>);
    StructureKind kind() const override { return StructureKind::Heap; }
    FileId data_file() const override { return file_; }
    std::vector<FileId> files() const override { return {file_}; }
    void bulk_load(RecordStream&) override;
    RID  insert(std::span<const std::byte>) override;
    void erase(RID) override;
    ScanPlan scan_plan() const override { return {ScanPlan::Order::Sequential, file_, 1, last_}; }
    std::unique_ptr<SearchState> find(const Predicate&, bool, Intent, ReservationId) override;
    std::optional<RID> first_of_group(const Key&) override { return std::nullopt; }
    StructureStats measure() override;
    Occupancy occupancy() override;
    std::uint32_t data_blocks() override { return last_; }
    std::uint32_t free_bytes() const;   // mapa de espaço livre (30)
private:
    BufferPool& pool_;
    FileId file_;
    std::shared_ptr<const Schema> schema_;
    std::uint32_t last_ = 0;            // inserção sempre no fim (30)
    std::vector<std::uint16_t> fsm_;    // bytes livres por bloco (índice = bloco)
};

class HashFile final : public PrimaryFile {
public:
    HashFile(BufferPool&, FileId, std::shared_ptr<const Schema>, FieldId key, std::uint32_t buckets, bool unique);
    StructureKind kind() const override { return StructureKind::Hash; }
    FileId data_file() const override { return file_; }
    std::vector<FileId> files() const override { return {file_}; }
    void bulk_load(RecordStream&) override;
    RID  insert(std::span<const std::byte>) override;
    void erase(RID) override;
    ScanPlan scan_plan() const override { return {ScanPlan::Order::BucketChains, file_, 0, nbuckets_}; }
    std::unique_ptr<SearchState> find(const Predicate&, bool, Intent, ReservationId) override;
    std::optional<RID> first_of_group(const Key&) override;
    StructureStats measure() override;
    Occupancy occupancy() override;
    std::uint32_t data_blocks() override;
    FieldId key_field() const override { return key_; }
    std::uint32_t buckets() const { return nbuckets_; }
    std::uint32_t bucket_of(const Key& k) const { return static_cast<std::uint32_t>(hash_key(k, 0) % nbuckets_); }
private:
    Key key_of(std::span<const std::byte> r) const { return detail::make_view(r, schema_.get()).key(key_); }
    BufferPool& pool_;
    FileId file_;
    std::shared_ptr<const Schema> schema_;
    FieldId key_;
    std::uint32_t nbuckets_;
    bool unique_;
    std::uint32_t overflow_ = 0;
};

class BPlusTreeFile final : public PrimaryFile {
public:
    BPlusTreeFile(BufferPool&, FileId data, FileId index, std::shared_ptr<const Schema>, FieldId key, bool unique,
                  double fill);
    StructureKind kind() const override { return StructureKind::BTree; }
    FileId data_file() const override { return engine_.leaf_file(); }
    std::vector<FileId> files() const override { return {engine_.leaf_file(), engine_.node_file()}; }
    void bulk_load(RecordStream& s) override { engine_.bulk_load(s, fill_); }
    RID  insert(std::span<const std::byte> r) override { return engine_.insert(r); }
    void erase(RID rid) override { engine_.erase(rid); }
    ScanPlan scan_plan() const override { return {ScanPlan::Order::LeafChain, engine_.leaf_file(), engine_.meta().first_leaf, 0}; }
    std::unique_ptr<SearchState> find(const Predicate&, bool, Intent, ReservationId) override;
    std::optional<RID> first_of_group(const Key& k) override { return engine_.find_first(k); }
    StructureStats measure() override;
    Occupancy occupancy() override;
    std::uint32_t data_blocks() override { return engine_.measure().leaves; }
    FieldId key_field() const override { return key_; }
    const BPlusEngine& engine() const { return engine_; }
    std::string verify(bool occ) const { return engine_.verify(occ); }
private:
    void on_move_changed() override { engine_.set_on_move(on_move_); }
    BufferPool& pool_;
    std::shared_ptr<const Schema> schema_;
    FieldId key_;
    double fill_;
    BPlusEngine engine_;
};

// ======================================================================= índices secundários
class SecondaryIndex {
public:
    virtual ~SecondaryIndex() = default;
    virtual void add(const Key&, RID) = 0;
    virtual void remove(const Key&, RID) = 0;
    virtual void move(const Key&, RID from, RID to) = 0;
    virtual std::unique_ptr<RidState> probe(const Predicate&, Intent) = 0;
    virtual StructureStats measure() = 0;
    virtual Occupancy occupancy() = 0;
    virtual std::vector<FileId> files() const = 0;
    virtual std::optional<RID> first(const Key&) { return std::nullopt; }        // clustering
    virtual void set_first(const Key&, std::optional<RID>) {}                     // clustering
};

class HashIndex final : public SecondaryIndex {   // UNIQUE, sobre heap (§4.2)
public:
    HashIndex(BufferPool&, FileId, KeyCodec, std::uint32_t buckets, bool unique);
    void add(const Key&, RID) override;
    void remove(const Key&, RID) override;
    void move(const Key&, RID from, RID to) override;
    std::unique_ptr<RidState> probe(const Predicate&, Intent) override;
    StructureStats measure() override;
    Occupancy occupancy() override;
    std::vector<FileId> files() const override { return {file_}; }
    std::uint32_t buckets() const { return nbuckets_; }
private:
    friend class HashIndexRids;
    std::uint32_t bucket_of(const Key& k) const { return static_cast<std::uint32_t>(hash_key(k, 0) % nbuckets_); }
    BufferPool& pool_;
    FileId file_;
    KeyCodec codec_;
    std::uint32_t nbuckets_;
    bool unique_;
    std::uint32_t overflow_ = 0;
};

// B+ sem repetição de chave nas folhas: entrada = [chave][u32 n][u32 overflow][até kInline RIDs].
class BPlusIndex final : public SecondaryIndex {
public:
    static constexpr std::uint32_t kInline = 64;
    BPlusIndex(BufferPool&, FileId, KeyCodec, bool clustering);
    void add(const Key&, RID) override;
    void remove(const Key&, RID) override;
    void move(const Key&, RID from, RID to) override;
    std::unique_ptr<RidState> probe(const Predicate&, Intent) override;
    StructureStats measure() override;
    Occupancy occupancy() override;
    std::vector<FileId> files() const override { return {file_}; }
    std::optional<RID> first(const Key&) override;
    void set_first(const Key&, std::optional<RID>) override;
    std::string verify() const { return engine_.verify(false); }

    // iteração das entradas em ordem de chave (usada pelo RidCursor e pelo clustering)
    class EntryIter {
    public:
        EntryIter(const BPlusIndex*, KeyRange, Intent);
        // devolve (chave, bytes da entrada); válidos até a próxima chamada
        std::optional<std::pair<Key, std::span<const std::byte>>> next();
    private:
        const BPlusIndex* idx_;
        KeyRange range_;
        Intent intent_;
        std::optional<PageGuard> leaf_;
        std::vector<std::pair<Key, std::uint16_t>> order_;
        std::size_t pos_ = 0;
        std::uint32_t next_leaf_ = 0;
        bool started_ = false, done_ = false;
    };
    // posting list de uma entrada: inline e depois a cadeia de overflow
    std::uint32_t entry_count(std::span<const std::byte> e) const;
    std::uint32_t entry_overflow(std::span<const std::byte> e) const;
    RID           entry_rid(std::span<const std::byte> e, std::uint32_t i) const;
    BufferPool&   pool() const { return pool_; }
    FileId        file() const { return file_; }

private:
    struct Entry { Key key; std::uint32_t count = 0; std::uint32_t ovf = 0; std::vector<RID> inline_rids; };
    Entry decode(std::span<const std::byte>) const;
    std::vector<std::byte> encode(const Entry&) const;
    std::optional<std::pair<RID, Entry>> lookup(const Key&) const;
    void store_entry(RID where, const Entry&);
    void pop_last_overflow(Entry&, RID& out);   // tira o último RID da cadeia de overflow
    BufferPool& pool_;
    FileId file_;
    KeyCodec codec_;
    bool clustering_;
    BPlusEngine engine_;
};

// busca pelo índice clustering (sobre B+ ou hash primários, 116)
std::unique_ptr<SearchState> make_clustering_search(const BPlusIndex& idx, PrimaryFile& primary,
                                                    std::shared_ptr<const Schema> schema, FieldId field,
                                                    const Predicate& p, bool unique, Intent i, ReservationId r,
                                                    BufferPool& pool);

}  // namespace cbd

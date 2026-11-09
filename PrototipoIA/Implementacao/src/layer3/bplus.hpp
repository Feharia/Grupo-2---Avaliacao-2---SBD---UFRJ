// Camada 3 — motor B+ paginado, usado pela B+ primária (folhas = registros, no arquivo de dados;
// nós internos num arquivo Index, decisão 145) e pelos índices B+ secundários (tudo no arquivo Index).
// Divide a 100% (35), redistribui antes de fundir (36), ocupação mínima ⌈n/2⌉ (37), bulk load (38).
#pragma once

#include "common/common.hpp"
#include "layer3/page.hpp"
#include "layer3/record.hpp"
#include "layer4/buffer_pool.hpp"

#include <functional>
#include <string>
#include <vector>

namespace cbd {

class BPlusEngine {
public:
    using KeyOf  = std::function<Key(std::span<const std::byte>)>;
    using MoveFn = std::function<void(RID from, RID to, std::span<const std::byte> rec)>;
    struct Meta {
        std::uint32_t root = 0;
        bool          root_is_leaf = true;
        std::uint16_t height = 1;          // níveis, incluindo as folhas
        std::uint32_t first_leaf = 0;
    };
    struct Shape {
        std::uint16_t              height = 1;
        std::vector<std::uint32_t> levels;           // nós internos por nível, da raiz para baixo
        std::uint32_t              internal_blocks = 0;
        std::uint32_t              leaves = 0;
        std::uint64_t              records = 0;
        std::uint32_t              distinct = 0;
    };

    BPlusEngine(BufferPool&, FileId leaf_file, FileId node_file, KeyCodec, KeyOf, bool unique_keys,
                PageType leaf_type, std::uint32_t fixed_record /* 0 = registros variáveis */);

    void set_on_move(MoveFn f) { on_move_ = std::move(f); }
    void init_empty();
    void bulk_load(RecordStream& sorted, double fill);
    RID  insert(std::span<const std::byte> rec);          // DuplicateKey se a chave for única e existir
    void erase(RID rid);
    RID  replace(RID rid, std::span<const std::byte> rec); // mesma chave; pode mudar de lugar
    std::uint32_t      leaf_for(const std::optional<Key>& lo) const;   // descida pela esquerda
    std::optional<RID> find_first(const Key& k) const;               // manutenção
    Shape              measure() const;
    std::string        verify(bool check_occupancy) const;           // vazio = invariantes ok

    const Meta&     meta() const { return meta_; }
    FileId          leaf_file() const { return leaf_file_; }
    FileId          node_file() const { return node_file_; }
    const KeyCodec& codec() const { return codec_; }
    const KeyOf&    key_of() const { return key_of_; }
    PageType        leaf_type() const { return leaf_type_; }
    std::uint32_t   internal_cap() const { return int_cap_; }

private:
    using Path = std::vector<std::pair<std::uint32_t, std::size_t>>;   // (nó, índice do filho)
    PageGuard    leaf(std::uint32_t b) const;
    PageGuard    node(std::uint32_t b) const;
    InternalNode load_node(std::uint32_t b) const;
    void         save_node(std::uint32_t b, const InternalNode&);
    std::uint32_t descend(const Key&, Path&, bool rightmost) const;
    bool          find_path(std::uint32_t b, const Key&, std::uint32_t target, Path&) const;
    void          split_leaf(std::uint32_t leafb, Path path, const Key& k_new);
    void          insert_into_parent(Path path, std::uint32_t left, const Key& sep, std::uint32_t right);
    void          fix_leaf(std::uint32_t leafb, Path path);
    void          fix_internal(std::uint32_t nb, Path path);
    bool          leaf_ok(std::uint32_t b) const;
    bool          leaf_ok_after_removing(std::uint32_t b, std::size_t len) const;
    bool          move_record(std::uint32_t from_b, std::uint16_t slot, std::uint32_t to_b);
    void          unlink_leaf(std::uint32_t b);
    Key           min_key(std::uint32_t leafb) const;

    BufferPool&   pool_;
    FileId        leaf_file_, node_file_;
    KeyCodec      codec_;
    KeyOf         key_of_;
    bool          unique_;
    PageType      leaf_type_;
    std::uint32_t fixed_;
    std::uint32_t leaf_cap_;   // registros por folha (registros fixos)
    std::uint32_t int_cap_;    // chaves por nó interno
    std::uint32_t min_int_;
    Meta          meta_;
    MoveFn        on_move_;
};

}  // namespace cbd

// Camada 3 — layout dos blocos: cabeçalho de 24 B e SlottedPage única (26–28).
#pragma once

#include "common/common.hpp"
#include "layer3/record.hpp"

#include <functional>
#include <span>
#include <vector>

namespace cbd {

enum class PageType : std::uint16_t {
    Free = 0, Heap = 1, Leaf = 2, Internal = 3, Bucket = 4, Temp = 5,
    IndexBucket = 6, IndexLeaf = 7, Posting = 8, Catalog = 9
};

struct PageHeader {
    std::uint16_t type = 0;
    std::uint16_t nslots = 0;    // páginas com slots: nº de slots; nó interno: nº de chaves
    std::uint16_t free_lo = 0;   // fim do diretório de slots
    std::uint16_t free_hi = 0;   // início da área de registros (cresce para baixo)
    std::uint32_t next = 0;      // próximo bloco da cadeia (folhas, overflow); 0 = nenhum
    std::uint32_t prev = 0;      // folha anterior
    std::uint32_t aux = 0;       // nó interno: filho 0; bucket: nº do bucket
    std::uint16_t live = 0;      // registros vivos
    std::uint16_t level = 0;     // nó interno: 1 = filhos são folhas
};
static_assert(sizeof(PageHeader) == 24);
inline constexpr std::uint32_t kPageHeader = 24;
inline constexpr std::uint32_t kSlotEntry = 4;   // u16 offset + u16 tamanho; tamanho 0 = removido

PageHeader read_header(std::span<const std::byte>);
void       write_header(std::span<std::byte>, const PageHeader&);

// --- SlottedPage (funções sobre os bytes da moldura; slots nunca são renumerados)
std::uint32_t sp_capacity(std::uint32_t block_size, std::uint32_t record_size);   // bfr derivado do layout
void          sp_init(std::span<std::byte>, PageType);
std::uint16_t sp_nslots(std::span<const std::byte>);
std::span<const std::byte> sp_record(std::span<const std::byte>, std::uint16_t slot);   // vazio se removido
std::optional<std::uint16_t> sp_insert(std::span<std::byte>, std::span<const std::byte> rec);
void          sp_erase(std::span<std::byte>, std::uint16_t slot);
bool          sp_replace(std::span<std::byte>, std::uint16_t slot, std::span<const std::byte> rec);
void          sp_compact(std::span<std::byte>);
std::uint32_t sp_used_bytes(std::span<const std::byte>);   // registros vivos + seus slots
std::uint32_t sp_free_bytes(std::span<const std::byte>);   // livre depois de compactar
bool          sp_fits(std::span<const std::byte>, std::size_t len);

// slots vivos ordenados pela chave (CPU apenas; limitado aos registros de uma página)
std::vector<std::pair<Key, std::uint16_t>> sorted_slots(std::span<const std::byte> page,
                                                        const std::function<Key(std::span<const std::byte>)>& key_of);

// --- nó interno da B+: cabeçalho + (chave, filho u32) × n; aux = filho 0
struct InternalNode {
    std::uint16_t              level = 1;
    std::vector<Key>           keys;       // keys[i] = menor chave do filho i+1
    std::vector<std::uint32_t> children;   // children.size() == keys.size() + 1
};
std::uint32_t internal_capacity(std::uint32_t block_size, const KeyCodec&);   // máx. de chaves
InternalNode  read_internal(std::span<const std::byte>, const KeyCodec&);
void          write_internal(std::span<std::byte>, const InternalNode&, const KeyCodec&);

}  // namespace cbd

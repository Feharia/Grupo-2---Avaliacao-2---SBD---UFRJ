#include "layer3/page.hpp"

#include <algorithm>
#include <functional>

namespace cbd {

PageHeader read_header(std::span<const std::byte> p) {
    PageHeader h;
    std::memcpy(&h, p.data(), sizeof h);
    return h;
}
void write_header(std::span<std::byte> p, const PageHeader& h) { std::memcpy(p.data(), &h, sizeof h); }

namespace {
struct Slot { std::uint16_t off, len; };
Slot slot_at(std::span<const std::byte> p, std::uint16_t i) {
    const std::byte* s = p.data() + kPageHeader + kSlotEntry * i;
    return {load<std::uint16_t>(s), load<std::uint16_t>(s + 2)};
}
void set_slot(std::span<std::byte> p, std::uint16_t i, Slot s) {
    std::byte* d = p.data() + kPageHeader + kSlotEntry * i;
    store<std::uint16_t>(d, s.off);
    store<std::uint16_t>(d + 2, s.len);
}
std::uint32_t page_size(std::span<const std::byte> p) { return static_cast<std::uint32_t>(p.size()); }
}  // namespace

std::uint32_t sp_capacity(std::uint32_t bs, std::uint32_t r) { return (bs - kPageHeader) / (r + kSlotEntry); }

void sp_init(std::span<std::byte> p, PageType t) {
    std::fill(p.begin(), p.end(), std::byte{0});
    PageHeader h;
    h.type = static_cast<std::uint16_t>(t);
    h.free_lo = kPageHeader;
    h.free_hi = static_cast<std::uint16_t>(std::min<std::uint32_t>(page_size(p), 0xFFFF));
    write_header(p, h);
}

std::uint16_t sp_nslots(std::span<const std::byte> p) { return read_header(p).nslots; }

std::span<const std::byte> sp_record(std::span<const std::byte> p, std::uint16_t i) {
    const PageHeader h = read_header(p);
    if (i >= h.nslots) return {};
    const Slot s = slot_at(p, i);
    if (s.len == 0) return {};
    if (static_cast<std::uint32_t>(s.off) + s.len > p.size() || s.off < h.free_lo)
        throw std::out_of_range("slot com offset fora do bloco");   // (§7.1)
    return p.subspan(s.off, s.len);
}

std::uint32_t sp_used_bytes(std::span<const std::byte> p) {
    const PageHeader h = read_header(p);
    std::uint32_t used = 0;
    for (std::uint16_t i = 0; i < h.nslots; ++i) {
        const Slot s = slot_at(p, i);
        if (s.len) used += s.len + kSlotEntry;
    }
    return used;
}

std::uint32_t sp_free_bytes(std::span<const std::byte> p) {
    const PageHeader h = read_header(p);
    std::uint32_t live = 0;
    for (std::uint16_t i = 0; i < h.nslots; ++i) live += slot_at(p, i).len;
    return page_size(p) - kPageHeader - kSlotEntry * h.nslots - live;
}

bool sp_fits(std::span<const std::byte> p, std::size_t len) {
    const PageHeader h = read_header(p);
    bool dead = false;
    for (std::uint16_t i = 0; i < h.nslots && !dead; ++i) dead = slot_at(p, i).len == 0;
    return sp_free_bytes(p) >= len + (dead ? 0 : kSlotEntry);
}

void sp_compact(std::span<std::byte> p) {
    PageHeader h = read_header(p);
    std::vector<std::byte> copy(p.begin(), p.end());
    std::uint32_t hi = page_size(p);
    for (std::uint16_t i = 0; i < h.nslots; ++i) {
        Slot s = slot_at(copy, i);
        if (s.len == 0) continue;
        hi -= s.len;
        std::memcpy(p.data() + hi, copy.data() + s.off, s.len);
        s.off = static_cast<std::uint16_t>(hi);
        set_slot(p, i, s);
    }
    h.free_hi = static_cast<std::uint16_t>(hi);
    write_header(p, h);
}

std::optional<std::uint16_t> sp_insert(std::span<std::byte> p, std::span<const std::byte> rec) {
    if (rec.empty()) throw std::invalid_argument("registro vazio");
    PageHeader h = read_header(p);
    std::optional<std::uint16_t> dead;
    for (std::uint16_t i = 0; i < h.nslots; ++i)
        if (slot_at(p, i).len == 0) { dead = i; break; }
    const std::uint32_t need = static_cast<std::uint32_t>(rec.size()) + (dead ? 0 : kSlotEntry);
    if (sp_free_bytes(p) < need) return std::nullopt;
    if (static_cast<std::uint32_t>(h.free_hi - h.free_lo) < need) {
        sp_compact(p);
        h = read_header(p);
    }
    const std::uint16_t slot = dead ? *dead : h.nslots;
    if (!dead) {
        ++h.nslots;
        h.free_lo = static_cast<std::uint16_t>(h.free_lo + kSlotEntry);
    }
    h.free_hi = static_cast<std::uint16_t>(h.free_hi - rec.size());
    std::memcpy(p.data() + h.free_hi, rec.data(), rec.size());
    ++h.live;
    write_header(p, h);
    set_slot(p, slot, Slot{h.free_hi, static_cast<std::uint16_t>(rec.size())});
    return slot;
}

void sp_erase(std::span<std::byte> p, std::uint16_t i) {
    PageHeader h = read_header(p);
    if (i >= h.nslots || slot_at(p, i).len == 0) throw std::logic_error("sp_erase: slot inexistente ou já removido");
    set_slot(p, i, Slot{0, 0});
    --h.live;
    write_header(p, h);
}

bool sp_replace(std::span<std::byte> p, std::uint16_t i, std::span<const std::byte> rec) {
    PageHeader h = read_header(p);
    if (i >= h.nslots) throw std::logic_error("sp_replace: slot inexistente");
    Slot s = slot_at(p, i);
    if (s.len == 0) throw std::logic_error("sp_replace: slot removido");
    if (rec.size() <= s.len) {   // cabe no lugar
        std::memcpy(p.data() + s.off, rec.data(), rec.size());
        set_slot(p, i, Slot{s.off, static_cast<std::uint16_t>(rec.size())});
        return true;
    }
    if (sp_free_bytes(p) + s.len < rec.size()) return false;
    set_slot(p, i, Slot{0, 0});   // libera o espaço antigo, mantendo o nº do slot
    if (static_cast<std::uint32_t>(h.free_hi - h.free_lo) < rec.size()) sp_compact(p);
    h = read_header(p);
    h.free_hi = static_cast<std::uint16_t>(h.free_hi - rec.size());
    std::memcpy(p.data() + h.free_hi, rec.data(), rec.size());
    write_header(p, h);
    set_slot(p, i, Slot{h.free_hi, static_cast<std::uint16_t>(rec.size())});
    return true;
}

std::vector<std::pair<Key, std::uint16_t>> sorted_slots(
    std::span<const std::byte> p, const std::function<Key(std::span<const std::byte>)>& key_of) {
    std::vector<std::pair<Key, std::uint16_t>> out;
    const std::uint16_t n = sp_nslots(p);
    out.reserve(n);
    for (std::uint16_t i = 0; i < n; ++i) {
        auto r = sp_record(p, i);
        if (!r.empty()) out.emplace_back(key_of(r), i);
    }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        const int c = compare_keys(a.first, b.first);
        return c < 0 || (c == 0 && a.second < b.second);
    });
    return out;
}

std::uint32_t internal_capacity(std::uint32_t bs, const KeyCodec& kc) { return (bs - kPageHeader) / (kc.size + 4u); }

InternalNode read_internal(std::span<const std::byte> p, const KeyCodec& kc) {
    const PageHeader h = read_header(p);
    if (h.type != static_cast<std::uint16_t>(PageType::Internal)) throw std::logic_error("bloco não é nó interno");
    InternalNode n;
    n.level = h.level;
    n.children.push_back(h.aux);
    const std::size_t entry = kc.size + 4u;
    if (kPageHeader + entry * h.nslots > p.size()) throw std::out_of_range("nó interno corrompido");
    for (std::uint16_t i = 0; i < h.nslots; ++i) {
        const std::byte* e = p.data() + kPageHeader + entry * i;
        n.keys.push_back(kc.decode(e));
        n.children.push_back(load<std::uint32_t>(e + kc.size));
    }
    return n;
}

void write_internal(std::span<std::byte> p, const InternalNode& n, const KeyCodec& kc) {
    if (n.children.size() != n.keys.size() + 1) throw std::logic_error("nó interno inconsistente");
    if (n.keys.size() > internal_capacity(static_cast<std::uint32_t>(p.size()), kc))
        throw std::logic_error("nó interno acima da capacidade");
    std::fill(p.begin(), p.end(), std::byte{0});
    PageHeader h;
    h.type = static_cast<std::uint16_t>(PageType::Internal);
    h.nslots = static_cast<std::uint16_t>(n.keys.size());
    h.aux = n.children[0];
    h.level = n.level;
    write_header(p, h);
    const std::size_t entry = kc.size + 4u;
    for (std::size_t i = 0; i < n.keys.size(); ++i) {
        std::byte* e = p.data() + kPageHeader + entry * i;
        kc.encode(n.keys[i], e);
        store<std::uint32_t>(e + kc.size, n.children[i + 1]);
    }
}

}  // namespace cbd

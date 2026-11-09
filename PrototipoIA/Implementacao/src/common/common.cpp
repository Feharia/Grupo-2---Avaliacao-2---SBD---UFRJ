#include "common/common.hpp"

#include <cstdio>
#include <cstdlib>

namespace cbd {

const char* to_string(FileKind k) {
    switch (k) {
        case FileKind::Data: return "data";
        case FileKind::Index: return "index";
        case FileKind::Catalog: return "catalog";
        case FileKind::Temp: return "temp";
    }
    return "?";
}
const char* to_string(Policy p) { return p == Policy::Lru ? "LRU" : "MRU"; }
const char* to_string(StructureKind k) {
    switch (k) {
        case StructureKind::Heap: return "heap";
        case StructureKind::BTree: return "btree";
        case StructureKind::Hash: return "hash";
        case StructureKind::SecHash: return "sec_hash";
        case StructureKind::SecBTree: return "sec_btree";
    }
    return "?";
}
const char* to_string(JoinAlgorithm a) {
    switch (a) {
        case JoinAlgorithm::Bnl: return "bnl";
        case JoinAlgorithm::SortMerge: return "sort_merge";
        case JoinAlgorithm::Hash: return "hash";
    }
    return "?";
}
const char* to_string(PathKind k) {
    switch (k) {
        case PathKind::Scan: return "scan";
        case PathKind::PrimaryBTree: return "primary_btree";
        case PathKind::PrimaryHash: return "primary_hash";
        case PathKind::SecondaryHash: return "secondary_hash";
        case PathKind::SecondaryBTreeClustering: return "clustering_btree";
        case PathKind::SecondaryBTreeNonClustering: return "secondary_btree";
    }
    return "?";
}

int compare_keys(const Key& a, const Key& b) {
    if (a.index() != b.index()) throw std::invalid_argument("compare_keys: tipos de chave diferentes");
    if (auto pa = std::get_if<std::int32_t>(&a)) {
        auto vb = std::get<std::int32_t>(b);
        return *pa < vb ? -1 : (*pa > vb ? 1 : 0);
    }
    if (auto pa = std::get_if<std::int64_t>(&a)) {
        auto vb = std::get<std::int64_t>(b);
        return *pa < vb ? -1 : (*pa > vb ? 1 : 0);
    }
    const int c = std::get<std::string>(a).compare(std::get<std::string>(b));
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

std::string key_to_string(const Key& k) {
    if (auto p = std::get_if<std::int32_t>(&k)) return std::to_string(*p);
    if (auto p = std::get_if<std::int64_t>(&k)) return std::to_string(*p);
    return std::get<std::string>(k);
}

bool KeyRange::below(const Key& k) const {
    if (!lo) return false;
    const int c = compare_keys(k, *lo);
    return lo_incl ? c < 0 : c <= 0;
}
bool KeyRange::above(const Key& k) const {
    if (!hi) return false;
    const int c = compare_keys(k, *hi);
    return hi_incl ? c > 0 : c >= 0;
}

KeyRange as_range(const Predicate& p) {
    if (auto pt = std::get_if<Point>(&p)) return KeyRange{pt->field, pt->key, pt->key, true, true};
    return std::get<KeyRange>(p);
}
FieldId predicate_field(const Predicate& p) {
    return std::visit([](const auto& x) { return x.field; }, p);
}

FieldId Schema::find(std::string_view name) const {
    for (std::size_t i = 0; i < fields.size(); ++i)
        if (fields[i].name == name) return static_cast<FieldId>(i);
    throw std::out_of_range("campo inexistente: " + std::string(name));
}

Schema make_schema(const std::vector<FieldSpec>& specs) {
    Schema s;
    std::uint16_t nullable = 0;
    for (const auto& f : specs) nullable += f.nullable ? 1 : 0;
    s.null_bytes = static_cast<std::uint16_t>((nullable + 7) / 8);
    std::uint32_t off = s.null_bytes;
    std::uint16_t var_ordinal = 0, null_ordinal = 0;
    std::uint32_t var_max = 0;
    for (const auto& f : specs) {
        FieldDef d;
        d.name = f.name;
        d.type = f.type;
        d.unique = f.unique;
        d.nullable = f.nullable;
        switch (f.type) {
            case FieldType::Int32: d.size = 4; break;
            case FieldType::Int64: d.size = 8; break;
            case FieldType::Char:
            case FieldType::VarChar:
                if (f.size == 0) throw std::invalid_argument("campo texto sem tamanho: " + f.name);
                d.size = f.size;
                break;
        }
        if (f.type == FieldType::VarChar) {
            d.offset = var_ordinal++;
            var_max += d.size;
        } else {
            d.offset = static_cast<std::uint16_t>(off);
            off += d.size;
        }
        s.null_bit.push_back(f.nullable ? static_cast<std::int16_t>(null_ordinal++) : std::int16_t{-1});
        s.fields.push_back(std::move(d));
    }
    s.var_fields = var_ordinal;
    s.fixed_end = static_cast<std::uint16_t>(off);
    const std::uint32_t total = off + 2u * var_ordinal + var_max;
    if (total > 0xFFFF) throw std::invalid_argument("registro grande demais");
    s.record_size = static_cast<std::uint16_t>(total);
    return s;
}

IoCounts& IoCounts::operator+=(const IoCounts& o) {
    requests += o.requests; hits += o.hits; misses += o.misses; allocations += o.allocations;
    reads_seq += o.reads_seq; reads_rand += o.reads_rand;
    writes_seq += o.writes_seq; writes_rand += o.writes_rand;
    dirty_evictions += o.dirty_evictions; forced_writes += o.forced_writes;
    data_area_requests += o.data_area_requests;
    return *this;
}
IoCounts& IoCounts::operator-=(const IoCounts& o) {
    requests -= o.requests; hits -= o.hits; misses -= o.misses; allocations -= o.allocations;
    reads_seq -= o.reads_seq; reads_rand -= o.reads_rand;
    writes_seq -= o.writes_seq; writes_rand -= o.writes_rand;
    dirty_evictions -= o.dirty_evictions; forced_writes -= o.forced_writes;
    data_area_requests -= o.data_area_requests;
    return *this;
}
Counters operator-(Counters a, const Counters& b) {
    a.global -= b.global;
    for (std::size_t i = 0; i < a.per_kind.size(); ++i) a.per_kind[i] -= b.per_kind[i];
    for (auto& [f, c] : a.per_file) {
        auto it = b.per_file.find(f);
        if (it != b.per_file.end()) c -= it->second;
    }
    return a;
}

std::uint64_t fnv1a64(std::span<const std::byte> bytes, std::uint64_t basis) {
    std::uint64_t h = basis;
    for (std::byte b : bytes) {
        h ^= static_cast<std::uint8_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

std::uint64_t hash_key(const Key& k, std::uint64_t seed) {
    constexpr std::uint64_t kGolden = 0x9E3779B97F4A7C15ull;   // multiplicativo de Knuth (razão áurea)
    std::uint64_t x;
    if (auto p = std::get_if<std::int32_t>(&k)) {
        x = static_cast<std::uint64_t>(static_cast<std::int64_t>(*p));
    } else if (auto q = std::get_if<std::int64_t>(&k)) {
        x = static_cast<std::uint64_t>(*q);
    } else {
        const auto& s = std::get<std::string>(k);
        x = fnv1a64(std::as_bytes(std::span(s.data(), s.size())));
    }
    x ^= seed * kGolden + 0x632BE59BD9B4E019ull;
    x *= kGolden;
    return x ^ (x >> 29);
}

std::uint32_t next_prime(std::uint32_t n) {
    if (n <= 2) return 2;
    for (std::uint32_t c = n | 1u;; c += 2) {
        bool prime = true;
        for (std::uint32_t d = 3; static_cast<std::uint64_t>(d) * d <= c; d += 2)
            if (c % d == 0) { prime = false; break; }
        if (prime) return c;
    }
}

void fatal(const char* what) noexcept {
    std::fprintf(stderr, "erro fatal: %s\n", what);
    std::fflush(stderr);
    std::abort();
}

}  // namespace cbd

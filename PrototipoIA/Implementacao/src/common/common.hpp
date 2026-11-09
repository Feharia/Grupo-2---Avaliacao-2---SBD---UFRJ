// common/ — valores sem comportamento usados por todas as camadas (decisões 135, 157, 158, 167).
#pragma once

#include <array>
#include <bit>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cbd {

// ------------------------------------------------------------------ identificadores
using FileId        = std::uint32_t;   // 0 = inválido
using FieldId       = std::uint16_t;
using FrameId       = std::uint32_t;
using TableId       = std::uint32_t;
using StructureId   = std::uint32_t;
using ReservationId = std::uint32_t;   // 0 = sem reserva
inline constexpr ReservationId kNoReservation = 0;   // passado sempre por escrito (159)

enum class FileKind      : std::uint8_t { Data, Index, Catalog, Temp };
enum class Area          : std::uint8_t { Data, Index, Catalog, Work };
enum class Policy        : std::uint8_t { Lru, Mru };
enum class StructureKind : std::uint8_t { Heap, BTree, Hash, SecHash, SecBTree };
enum class JoinAlgorithm : std::uint8_t { Bnl, SortMerge, Hash };
enum class FieldType     : std::uint8_t { Int32, Int64, Char, VarChar };   // VarChar (158)
enum class PathKind      : std::uint8_t {
    Scan, PrimaryBTree, PrimaryHash,
    SecondaryHash, SecondaryBTreeClustering, SecondaryBTreeNonClustering
};

const char* to_string(FileKind);
const char* to_string(Policy);
const char* to_string(StructureKind);
const char* to_string(JoinAlgorithm);
const char* to_string(PathKind);

// ------------------------------------------------------------------ BlockId e RID
struct BlockId {
    FileId        file  = 0;
    std::uint32_t block = 0;   // posição no arquivo do SO; 0 é o cabeçalho
    auto operator<=>(const BlockId&) const = default;
};
struct BlockIdHash {
    std::size_t operator()(BlockId b) const noexcept {
        return static_cast<std::size_t>((std::uint64_t{b.file} << 32) | b.block);
    }
};

struct RID {
    std::uint32_t block = 0;
    std::uint16_t slot  = 0;
    std::uint16_t reserved = 0;
    auto operator<=>(const RID&) const = default;
};
static_assert(sizeof(RID) == 8 && std::is_trivially_copyable_v<RID>);
static_assert(std::endian::native == std::endian::little);   // formatos em disco são os do host (167)
inline constexpr RID kInvalidRid{0, 0};                       // o bloco 0 é sempre cabeçalho (167)

inline std::int64_t rid_to_int(RID r) { return (static_cast<std::int64_t>(r.block) << 16) | r.slot; }
inline RID int_to_rid(std::int64_t v) {
    return RID{static_cast<std::uint32_t>(v >> 16), static_cast<std::uint16_t>(v & 0xFFFF)};
}

// ------------------------------------------------------------------ Intent (137)
struct Intent {
    bool reuse      = true;    // false: com toss-immediate, a moldura vira a próxima vítima
    bool sequential = false;   // true: com prefetch, lê adiante no mesmo extent
};
inline constexpr Intent kRandomReuse{true,  false};   // nós de índice
inline constexpr Intent kRandomOnce {false, false};   // fetch por RID
inline constexpr Intent kSeqReuse   {true,  true};    // interna do BNL
inline constexpr Intent kSeqOnce    {false, true};    // externa do BNL, runs, partições

// ------------------------------------------------------------------ chaves e predicados
using Key = std::variant<std::int32_t, std::int64_t, std::string>;   // CR em int32 (× 100)
int         compare_keys(const Key& a, const Key& b);   // lança se os tipos diferem
std::string key_to_string(const Key&);

struct Point    { FieldId field = 0; Key key; };
struct KeyRange {
    FieldId field = 0;
    std::optional<Key> lo, hi;
    bool lo_incl = true, hi_incl = true;
    bool below(const Key& k) const;      // k antes do início da faixa
    bool above(const Key& k) const;      // k depois do fim da faixa
    bool contains(const Key& k) const { return !below(k) && !above(k); }
};
using Predicate = std::variant<Point, KeyRange>;
KeyRange as_range(const Predicate&);
FieldId  predicate_field(const Predicate&);

// valor de um campo na carga (camada 1 → codificação na camada 3)
using Value = std::variant<std::monostate, std::int64_t, std::string>;   // monostate = NULL

// ------------------------------------------------------------------ esquema (157, 158)
struct FieldDef {
    std::string   name;
    FieldType     type = FieldType::Int32;
    std::uint16_t offset = 0, size = 0;   // VarChar: offset = ordinal entre os VarChar; size = máximo
    bool          unique   = false;       // (157)
    bool          nullable = false;       // (157, 158)
};
struct Schema {
    std::vector<FieldDef>     fields;
    std::uint16_t             record_size = 0;   // só campos fixos: exato; com VarChar: máximo
    std::uint16_t             null_bytes  = 0;   // ⌈anuláveis / 8⌉ (25); os offsets fixos já o descontam
    std::uint16_t             var_fields  = 0;   // nº de VarChar: um u16 de fim por campo, após os fixos
    std::uint16_t             fixed_end   = 0;   // fim do prefixo fixo
    std::vector<std::int16_t> null_bit;          // bit do campo no bitmap, ou -1
    const FieldDef& field(FieldId f) const { return fields.at(f); }
    FieldId find(std::string_view name) const;
    bool    has_var() const { return var_fields > 0; }
};
struct FieldSpec {
    std::string   name;
    FieldType     type = FieldType::Int32;
    std::uint16_t size = 0;   // Char/VarChar
    bool          unique = false, nullable = false;
};
Schema make_schema(const std::vector<FieldSpec>&);

struct PrimarySpec {
    StructureKind kind = StructureKind::Heap;
    FieldId       key = 0;              // a chave é ignorada no Heap
    double        fill = 0.7;
    std::uint32_t buckets = 0;          // hash: calculado pela camada 1 (39, 163)
    std::uint32_t hash_function = 0;
};
struct IndexSpec {
    StructureKind kind = StructureKind::SecBTree;
    FieldId       key = 0;
    bool          clustering = false;
};

class RecordStream {
public:
    virtual ~RecordStream() = default;
    virtual std::optional<std::span<const std::byte>> next() = 0;
};

// ------------------------------------------------------------------ contadores (109, 159)
struct IoCounts {
    std::uint64_t requests = 0, hits = 0, misses = 0;   // requests = hits + misses
    std::uint64_t allocations = 0;
    std::uint64_t reads_seq = 0, reads_rand = 0;
    std::uint64_t writes_seq = 0, writes_rand = 0;
    std::uint64_t dirty_evictions = 0, forced_writes = 0;
    std::uint64_t data_area_requests = 0;   // pedidos sem reserva a arquivos Data; 0 nas operações medidas (159)
    IoCounts& operator+=(const IoCounts&);
    IoCounts& operator-=(const IoCounts&);
    std::uint64_t reads()  const { return reads_seq + reads_rand; }
    std::uint64_t writes() const { return writes_seq + writes_rand; }
    double hit_rate() const { return requests ? double(hits) / double(requests) : 0.0; }
};
struct Counters {
    IoCounts global;
    std::array<IoCounts, 4> per_kind{};                  // indexado por FileKind
    std::unordered_map<FileId, IoCounts> per_file;
    friend Counters operator-(Counters a, const Counters& b);
};

// ------------------------------------------------------------------ configuração
struct Config {
    std::uint32_t block_size    = 4096;
    std::uint32_t extent_blocks = 8;
    std::array<std::uint32_t, 4> area_frames{32, 16, 4, 32};   // Data, Index, Catalog, Work
    Policy policy         = Policy::Lru;
    bool   toss_immediate = false;
    bool   prefetch       = false;
    std::filesystem::path data_dir = "dados_motor";
    std::uint32_t frames() const { return area_frames[0] + area_frames[1] + area_frames[2] + area_frames[3]; }
};

// ------------------------------------------------------------------ estatísticas e caminhos
struct StructureStats {
    StructureId   id = 0;
    StructureKind kind = StructureKind::Heap;
    std::uint32_t blocks = 0;
    std::optional<std::uint16_t> height;
    std::optional<std::uint32_t> internal_blocks;
    std::optional<std::uint32_t> leaves, distinct;
    std::optional<std::uint32_t> buckets, overflow_blocks;
    std::optional<double>        load_factor, chain_avg;
    std::optional<std::uint32_t> chain_max;
};
struct TableStats {
    std::uint64_t n = 0;
    std::uint32_t r = 0, bfr = 0;
    std::uint32_t b = 0;   // heap: todos; B+: folhas; hash: buckets + overflow (119)
    std::vector<StructureStats> structures;
};

struct HashPartitioning {
    std::uint32_t function_id = 0;
    std::uint32_t buckets = 0;
    bool operator==(const HashPartitioning&) const = default;
};
struct AccessPath {
    TableId     table = 0;
    StructureId id = 0;
    PathKind    kind = PathKind::Scan;
    FieldId     field = 0;                                   // sem significado no Scan
    bool        unique = false;                              // = schema.field(field).unique; falso no Scan (157)
    bool        supports_point = false, supports_range = false;
    std::optional<FieldId>          ordered_by;              // só PrimaryBTree e SecondaryBTreeClustering
    std::optional<HashPartitioning> hash_partitioned_by;     // só PrimaryHash
};

struct Occupancy {
    std::uint32_t data = 0, overflow = 0, header = 0, directory = 0;
    std::vector<std::uint32_t> index_levels;   // da raiz para baixo
};

// ------------------------------------------------------------------ resultado de operação
struct Cost  { double transfers = 0; double seeks = 0; };
struct Phase { std::string name; Counters delta; };
struct OpResult {
    std::uint64_t      rows = 0;
    std::uint64_t      checksum = 0;
    Counters           delta;          // snapshot final − inicial, antes do flush de fim
    std::uint64_t      flush_writes = 0;
    Cost               estimate;
    std::vector<Phase> phases;
    std::chrono::nanoseconds wall_time{0};
};
// Célula que não roda (128, 170): NaoAplicavel = a célula não existe (casos 1 a 4);
// Inviavel = existe, mas não cabe em M no nosso desenho (plano ou execução, 151).
struct NotApplicable {
    enum class Kind : std::uint8_t { NaoAplicavel, Inviavel };
    std::string reason;
    Kind kind = Kind::NaoAplicavel;
    NotApplicable(std::string r, Kind k = Kind::NaoAplicavel) : reason(std::move(r)), kind(k) {}
};
using Outcome = std::variant<OpResult, NotApplicable>;

// ------------------------------------------------------------------ hash (42, 153)
std::uint64_t fnv1a64(std::span<const std::byte> bytes, std::uint64_t basis = 14695981039346656037ull);
std::uint64_t hash_key(const Key&, std::uint64_t seed);   // inteiros: multiplicativo; textos: FNV-1a
std::uint32_t next_prime(std::uint32_t n);

// leitura e gravação little-endian em bytes crus
template <class T> T load(const std::byte* p) { T v; std::memcpy(&v, p, sizeof v); return v; }
template <class T> void store(std::byte* p, T v) { std::memcpy(p, &v, sizeof v); }

[[noreturn]] void fatal(const char* what) noexcept;   // erro de programação em destrutor (155)

}  // namespace cbd

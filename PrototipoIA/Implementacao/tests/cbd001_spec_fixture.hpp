// Oráculo técnico da fixture descrita em docs/cbd001/CONTRATO_E_RASTREABILIDADE.md.
// Não inclui headers do motor e não chama seus codecs, páginas ou capacidades.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace cbd001_spec {

inline constexpr std::size_t block_bytes = 4096;
inline constexpr std::size_t header_bytes = 24;
inline constexpr std::size_t slot_bytes = 4;
inline constexpr std::size_t fixed_record_bytes = 127;
inline constexpr std::size_t max_leaf_records = 31;
inline constexpr std::size_t min_leaf_records = 16;
inline constexpr std::size_t useful_bytes = 4072;
inline constexpr std::size_t min_variable_bytes = 2036;
inline constexpr std::size_t max_internal_keys = 509;
inline constexpr std::size_t min_internal_keys = 254;
static_assert((4096 - 24) / (127 + 4) == 31);
static_assert((4096 - 24) / 8 == 509);

template <class T>
T native_at(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::out_of_range("fixture CBD-001: leitura fora dos bytes observados");
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

template <class T>
void put_native(std::span<std::byte> bytes, std::size_t offset, T value) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::out_of_range("fixture CBD-001: escrita fora dos bytes da fixture");
    std::memcpy(bytes.data() + offset, &value, sizeof value);
}

inline std::vector<std::byte> record(std::int64_t key, const std::string& text, bool variable = false) {
    if ((!variable && text.size() > 123) || text.size() > 65529 || key < INT32_MIN || key > INT32_MAX)
        throw std::invalid_argument("fixture CBD-001: registro fora do contrato");
    std::vector<std::byte> result(variable ? 6 + text.size() : fixed_record_bytes, std::byte{0});
    put_native<std::int32_t>(result, 0, static_cast<std::int32_t>(key));
    if (variable) put_native<std::uint16_t>(result, 4, static_cast<std::uint16_t>(result.size()));
    std::memcpy(result.data() + (variable ? 6 : 4), text.data(), text.size());
    return result;
}

struct ObservedRecord {
    std::uint16_t slot;
    std::int32_t key;
    std::span<const std::byte> bytes;
};

struct ObservedLeaf {
    std::uint32_t next, prev;
    std::vector<ObservedRecord> records;
    std::size_t used_bytes() const {
        std::size_t used = 0;
        for (const auto& record : records) used += record.bytes.size() + slot_bytes;
        return used;
    }
};

inline ObservedLeaf observe_leaf(std::span<const std::byte> page) {
    if (page.size() != block_bytes) throw std::runtime_error("fixture CBD-001: tamanho de página divergente");
    const auto nslots = native_at<std::uint16_t>(page, 2);
    const auto directory_end = header_bytes + slot_bytes * nslots;
    if (directory_end > block_bytes) throw std::runtime_error("fixture CBD-001: diretório inválido");
    ObservedLeaf result{native_at<std::uint32_t>(page, 8), native_at<std::uint32_t>(page, 12), {}};
    for (std::uint16_t slot = 0; slot < nslots; ++slot) {
        const auto offset = native_at<std::uint16_t>(page, header_bytes + slot_bytes * slot);
        const auto length = native_at<std::uint16_t>(page, header_bytes + slot_bytes * slot + 2);
        if (length == 0) continue;
        if (offset < directory_end || static_cast<std::size_t>(offset) + length > block_bytes)
            throw std::runtime_error("fixture CBD-001: registro fora da área de dados");
        const auto bytes = page.subspan(offset, length);
        result.records.push_back({slot, native_at<std::int32_t>(bytes, 0), bytes});
    }
    if (result.records.size() != native_at<std::uint16_t>(page, 20))
        throw std::runtime_error("fixture CBD-001: contagem live diverge do diretório");
    std::sort(result.records.begin(), result.records.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
    return result;
}

struct ObservedInternal {
    std::uint16_t level;
    std::vector<std::int32_t> keys;
    std::vector<std::uint32_t> children;
};

inline ObservedInternal observe_internal(std::span<const std::byte> page) {
    if (page.size() != block_bytes) throw std::runtime_error("fixture CBD-001: tamanho de nó divergente");
    const auto count = native_at<std::uint16_t>(page, 2);
    if (count > max_internal_keys) throw std::runtime_error("fixture CBD-001: nó excede capacidade declarada");
    ObservedInternal result{native_at<std::uint16_t>(page, 22), {}, {native_at<std::uint32_t>(page, 16)}};
    for (std::size_t i = 0; i < count; ++i) {
        result.keys.push_back(native_at<std::int32_t>(page, header_bytes + 8 * i));
        result.children.push_back(native_at<std::uint32_t>(page, header_bytes + 8 * i + 4));
    }
    return result;
}

inline void remove_slot(std::span<std::byte> page, std::uint16_t slot) {
    if (slot >= native_at<std::uint16_t>(page, 2) ||
        native_at<std::uint16_t>(page, header_bytes + slot_bytes * slot + 2) == 0)
        throw std::logic_error("fixture CBD-001: remoção de slot inexistente");
    put_native<std::uint16_t>(page, header_bytes + slot_bytes * slot + 2, 0);
    const auto live = native_at<std::uint16_t>(page, 20);
    if (live == 0) throw std::logic_error("fixture CBD-001: live inválido");
    put_native<std::uint16_t>(page, 20, static_cast<std::uint16_t>(live - 1));
}

}  // namespace cbd001_spec

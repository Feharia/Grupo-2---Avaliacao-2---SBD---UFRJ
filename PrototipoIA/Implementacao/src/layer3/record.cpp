#include "layer3/record.hpp"

#include <algorithm>

namespace cbd {

RecordView detail::make_view(std::span<const std::byte> b, const Schema* s) { return RecordView(b, s); }

bool RecordView::is_null(FieldId f) const {
    const std::int16_t bit = schema_->null_bit.at(f);
    if (bit < 0) return false;
    const std::size_t byte = static_cast<std::size_t>(bit) / 8;
    if (byte >= bytes_.size()) throw std::out_of_range("RecordView::is_null: registro curto");
    return ((static_cast<unsigned>(bytes_[byte]) >> (bit % 8)) & 1u) != 0;
}

std::span<const std::byte> RecordView::get_var(FieldId f) const {
    const auto& field = schema_->field(f);
    if (field.type != FieldType::VarChar) throw std::out_of_range("get_var em campo fixo");
    if (is_null(f)) throw std::logic_error("get_var: campo NULL");
    const std::size_t table = schema_->fixed_end;
    const std::size_t data0 = table + 2u * schema_->var_fields;
    if (data0 > bytes_.size()) throw std::out_of_range("get_var: registro curto");
    const std::size_t i = field.offset;
    const std::size_t end = load<std::uint16_t>(bytes_.data() + table + 2 * i);
    const std::size_t start = i == 0 ? data0 : load<std::uint16_t>(bytes_.data() + table + 2 * (i - 1));
    if (start < data0 || start > end || end > bytes_.size() || end - start > field.size)
        throw std::out_of_range("get_var: offsets corrompidos");
    return bytes_.subspan(start, end - start);
}

Key RecordView::key(FieldId f) const {
    const auto& field = schema_->field(f);
    if (is_null(f)) throw std::logic_error("RecordView::key: campo NULL (158)");
    switch (field.type) {
        case FieldType::Int32: return get<std::int32_t>(f);
        case FieldType::Int64: return get<std::int64_t>(f);
        case FieldType::Char: {
            if (static_cast<std::size_t>(field.offset) + field.size > bytes_.size())
                throw std::out_of_range("RecordView::key: limite");
            const char* p = reinterpret_cast<const char*>(bytes_.data() + field.offset);
            std::size_t n = 0;
            while (n < field.size && p[n] != '\0') ++n;
            return std::string(p, n);
        }
        case FieldType::VarChar: {
            auto s = get_var(f);
            return std::string(reinterpret_cast<const char*>(s.data()), s.size());
        }
    }
    throw std::logic_error("tipo desconhecido");
}

std::vector<std::byte> encode_record(const Schema& s, const std::vector<Value>& vals) {
    if (vals.size() != s.fields.size()) throw std::invalid_argument("encode_record: nº de valores ≠ nº de campos");
    std::vector<std::byte> out(s.fixed_end + 2u * s.var_fields, std::byte{0});
    std::vector<std::byte> var;
    std::uint16_t var_i = 0;
    for (std::size_t i = 0; i < vals.size(); ++i) {
        const auto& f = s.fields[i];
        const bool is_null = std::holds_alternative<std::monostate>(vals[i]);
        if (is_null) {
            if (!f.nullable) throw std::invalid_argument("NULL em campo NOT NULL: " + f.name);
            const auto bit = s.null_bit[i];
            out[bit / 8] |= std::byte{static_cast<unsigned char>(1u << (bit % 8))};
        }
        switch (f.type) {
            case FieldType::Int32:
                if (!is_null) {
                    const auto v = std::get<std::int64_t>(vals[i]);
                    if (v < INT32_MIN || v > INT32_MAX) throw std::out_of_range("valor fora de int32: " + f.name);
                    store<std::int32_t>(out.data() + f.offset, static_cast<std::int32_t>(v));
                }
                break;
            case FieldType::Int64:
                if (!is_null) store<std::int64_t>(out.data() + f.offset, std::get<std::int64_t>(vals[i]));
                break;
            case FieldType::Char:
                if (!is_null) {
                    const auto& t = std::get<std::string>(vals[i]);
                    if (t.size() > f.size) throw std::invalid_argument("texto maior que o campo: " + f.name);
                    std::memcpy(out.data() + f.offset, t.data(), t.size());
                }
                break;
            case FieldType::VarChar: {
                if (!is_null) {
                    const auto& t = std::get<std::string>(vals[i]);
                    if (t.size() > f.size) throw std::invalid_argument("texto maior que o campo: " + f.name);
                    const auto* p = reinterpret_cast<const std::byte*>(t.data());
                    var.insert(var.end(), p, p + t.size());
                }
                const std::size_t end = s.fixed_end + 2u * s.var_fields + var.size();
                store<std::uint16_t>(out.data() + s.fixed_end + 2 * var_i, static_cast<std::uint16_t>(end));
                ++var_i;
                break;
            }
        }
    }
    out.insert(out.end(), var.begin(), var.end());
    return out;
}

KeyCodec KeyCodec::for_field(const FieldDef& f) {
    if (f.type == FieldType::VarChar) throw std::invalid_argument("chave de índice em VarChar não suportada");
    return KeyCodec{f.type, f.size};
}

void KeyCodec::encode(const Key& k, std::byte* out) const {
    std::fill_n(out, size, std::byte{0});
    switch (type) {
        case FieldType::Int32: store<std::int32_t>(out, std::get<std::int32_t>(k)); break;
        case FieldType::Int64: store<std::int64_t>(out, std::get<std::int64_t>(k)); break;
        default: {
            const auto& s = std::get<std::string>(k);
            std::memcpy(out, s.data(), std::min<std::size_t>(s.size(), size));
        }
    }
}

Key KeyCodec::decode(const std::byte* in) const {
    switch (type) {
        case FieldType::Int32: return load<std::int32_t>(in);
        case FieldType::Int64: return load<std::int64_t>(in);
        default: {
            const char* p = reinterpret_cast<const char*>(in);
            std::size_t n = 0;
            while (n < size && p[n] != '\0') ++n;
            return std::string(p, n);
        }
    }
}

}  // namespace cbd

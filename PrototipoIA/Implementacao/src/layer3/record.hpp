// Camada 3 — formato do registro e leitura verificada (158).
// Registro: [null bitmap][campos fixos][u16 de fim por VarChar][bytes dos VarChar].
#pragma once

#include "common/common.hpp"

#include <span>
#include <vector>

namespace cbd {

struct DuplicateKey : std::runtime_error { using std::runtime_error::runtime_error; };   // (157)

class RecordView;
namespace detail {
RecordView make_view(std::span<const std::byte>, const Schema*);   // só a camada 3 cria visões
}

// Lê os campos de um registro diretamente da moldura, sem copiar; vale enquanto o guard existir.
class RecordView {
public:
    RecordView() = default;

    template <class T> T get(FieldId f) const {             // verificação sempre ligada (158)
        static_assert(std::is_trivially_copyable_v<T>);
        const auto& field = schema_->field(f);
        if (field.type == FieldType::VarChar || sizeof(T) != field.size ||
            static_cast<std::size_t>(field.offset) + sizeof(T) > bytes_.size())
            throw std::out_of_range("RecordView::get: tipo, tamanho ou limite");
        if (is_null(f)) throw std::logic_error("RecordView::get: campo NULL");
        T v;
        std::memcpy(&v, bytes_.data() + field.offset, sizeof v);
        return v;
    }
    bool is_null(FieldId f) const;                          // bit do null bitmap; falso se não anulável
    std::span<const std::byte> get_var(FieldId f) const;    // VarChar; confere os offsets
    Key key(FieldId f) const;                               // lança se o campo for NULL
    std::span<const std::byte> bytes() const { return bytes_; }
    const Schema* schema() const { return schema_; }
    bool empty() const { return schema_ == nullptr; }

private:
    friend RecordView detail::make_view(std::span<const std::byte>, const Schema*);
    RecordView(std::span<const std::byte> b, const Schema* s) : bytes_(b), schema_(s) {}
    std::span<const std::byte> bytes_;
    const Schema* schema_ = nullptr;
};

// Codifica um registro a partir dos valores, na ordem do esquema.
std::vector<std::byte> encode_record(const Schema&, const std::vector<Value>&);

// Chave de tamanho fixo para nós internos e entradas de índice.
struct KeyCodec {
    FieldType     type = FieldType::Int32;
    std::uint16_t size = 4;
    static KeyCodec for_field(const FieldDef&);
    void encode(const Key&, std::byte* out) const;
    Key  decode(const std::byte* in) const;
};

}  // namespace cbd

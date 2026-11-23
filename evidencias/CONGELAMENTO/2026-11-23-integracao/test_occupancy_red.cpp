// Integração das organizações paginadas com BufferPool/FileManager.
// Contrato: uso + folga livre reservada = comprimento lógico (PDF §6.1.1).
// Valores manuais para blocos de 4096 B, extent de oito e um cabeçalho por arquivo.
#include "helpers.hpp"
#include "layer3/structures.hpp"
#include "mini_test.hpp"

using namespace cbd;

namespace {
template<class O>
void expect_extent_counts(const O& o, std::uint64_t total, std::uint64_t free) {
    constexpr bool reports_extents = requires { o.file_total; o.reserved_free; };
    EXPECT_TRUE(reports_extents);
    if constexpr (reports_extents) {
        EXPECT_EQ(o.file_total, total);
        EXPECT_EQ(o.reserved_free, free);
    }
}

std::shared_ptr<const Schema> schema_with_pad(std::uint16_t bytes = 100) {
    return std::make_shared<const Schema>(make_schema({{"k", FieldType::Int32, 0, true},
                                                     {"pad", FieldType::Char, bytes}}));
}
}  // namespace

TEST(Occupancy, HeapDistingueArquivoVazioEPrimeiroExtent) {
    BufferPool pool(test_config("ocup_heap_primeiro"));
    const auto schema = schema_with_pad();
    const FileId file = pool.create("heap", FileKind::Data);
    HeapFile heap(pool, file, schema);
    expect_extent_counts(heap.occupancy(), 1, 0);
    heap.insert(encode_record(*schema, {std::int64_t{1}, std::string("x")}));
    EXPECT_EQ(heap.data_blocks(), 1u);
    expect_extent_counts(heap.occupancy(), 9, 7);
    const auto before = pool.snapshot();
    (void)heap.occupancy();
    const auto delta = pool.snapshot() - before;
    EXPECT_EQ(delta.global.reads(), 0u);
    EXPECT_EQ(delta.global.writes(), 0u);
}

TEST(Occupancy, HeapCresceSemConfundirBlocosLidosComReservados) {
    BufferPool pool(test_config("ocup_heap_cresce"));
    const auto schema = schema_with_pad(4000);  // só um registro de 4004 B por página
    HeapFile heap(pool, pool.create("heap", FileKind::Data), schema);
    for (std::int64_t k = 0; k < 9; ++k) heap.insert(encode_record(*schema, {k, std::string("x")}));
    EXPECT_EQ(heap.data_blocks(), 9u);
    expect_extent_counts(heap.occupancy(), 17, 7);  // cabeçalho + nove usados + sete livres
}

TEST(Occupancy, HashReconciliaOverflowLiberadoEReutilizado) {
    BufferPool pool(test_config("ocup_hash_overflow"));
    const auto schema = schema_with_pad(4000);
    HashFile hash(pool, pool.create("hash", FileKind::Data), schema, 0, 1, true);
    const auto a = hash.insert(encode_record(*schema, {std::int64_t{1}, std::string("x")}));
    const auto b = hash.insert(encode_record(*schema, {std::int64_t{2}, std::string("x")}));
    const auto c = hash.insert(encode_record(*schema, {std::int64_t{3}, std::string("x")}));
    (void)a;
    EXPECT_EQ(hash.data_blocks(), 3u);
    expect_extent_counts(hash.occupancy(), 9, 5);
    hash.erase(b);
    hash.erase(c);
    EXPECT_EQ(hash.data_blocks(), 1u);
    expect_extent_counts(hash.occupancy(), 9, 7);
    hash.insert(encode_record(*schema, {std::int64_t{4}, std::string("x")}));
    EXPECT_EQ(hash.data_blocks(), 2u);
    expect_extent_counts(hash.occupancy(), 9, 6);
}

TEST(Occupancy, BPlusSomaDoisArquivosIncluindoCabecalhos) {
    BufferPool pool(test_config("ocup_bplus_arquivos"));
    const auto schema = schema_with_pad(123);  // registro fixo de 127 B: 31 por folha
    BPlusTreeFile tree(pool, pool.create("dados", FileKind::Data), pool.create("indice", FileKind::Index),
                       schema, 0, true, 0.7);
    expect_extent_counts(tree.occupancy(), 10, 7);  // folha e dois cabeçalhos
    for (std::int64_t k = 0; k < 32; ++k) tree.insert(encode_record(*schema, {k, std::string("x")}));
    EXPECT_EQ(tree.data_blocks(), 2u);
    expect_extent_counts(tree.occupancy(), 18, 13);  // duas folhas + raiz + dois cabeçalhos
}

TEST(Occupancy, HashSecundarioIncluiAFolgaDoArquivo) {
    BufferPool pool(test_config("ocup_sec_hash"));
    const auto schema = schema_with_pad();
    HashIndex index(pool, pool.create("sec_hash", FileKind::Index), KeyCodec::for_field(schema->field(0)), 3, true);
    expect_extent_counts(index.occupancy(), 9, 5);  // três buckets e um cabeçalho
}

TEST(Occupancy, BPlusSecundariaContaPostingELiberacao) {
    BufferPool pool(test_config("ocup_sec_bplus"));
    const auto schema = schema_with_pad();
    BPlusIndex index(pool, pool.create("sec_bplus", FileKind::Index), KeyCodec::for_field(schema->field(0)), false);
    const Key key{std::int32_t{7}};
    expect_extent_counts(index.occupancy(), 9, 7);  // folha raiz vazia e cabeçalho
    for (std::uint32_t b = 1; b <= 70; ++b) index.add(key, RID{b, 0});
    expect_extent_counts(index.occupancy(), 9, 6);  // folha + overflow + cabeçalho
    for (std::uint32_t b = 1; b <= 70; ++b) index.remove(key, RID{b, 0});
    expect_extent_counts(index.occupancy(), 9, 7);
}

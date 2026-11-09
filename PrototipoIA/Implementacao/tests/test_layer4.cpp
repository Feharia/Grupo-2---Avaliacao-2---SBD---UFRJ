// Testes da camada 4: FileManager, BufferPool, políticas, reserva (§10 e decisões 159, 161, 168).
#include "helpers.hpp"
#include "layer4/buffer_pool.hpp"
#include "layer4/file_manager.hpp"
#include "mini_test.hpp"

#include <fstream>
#include <system_error>
#include <vector>

using namespace cbd;

namespace {
FileId file_with_blocks(BufferPool& pool, const std::string& name, FileKind kind, int n) {
    const FileId f = pool.create(name, kind);
    for (int i = 0; i < n; ++i) {
        auto [b, g] = pool.allocate(f, kNoReservation);
        g.write()[100] = std::byte{static_cast<unsigned char>(b.block)};
    }
    pool.flush_all();
    pool.invalidate_all();
    return f;
}
}  // namespace

TEST(FileManager, CriaAbreEConfereCabecalho) {
    const auto cfg = test_config("fm_cabecalho");
    {
        FileManager fm(cfg);
        const FileId f = fm.create("a.dat", FileKind::Data);
        EXPECT_EQ(fm.allocate_block(f), 1u);
        EXPECT_EQ(fm.allocate_block(f), 2u);
        std::vector<std::byte> buf(cfg.block_size, std::byte{7});
        fm.write_block(BlockId{f, 2}, buf);
        fm.free_block(f, 1);
        EXPECT_EQ(fm.flush_headers(), 1u);
        EXPECT_THROW(fm.create("a.dat", FileKind::Data), std::system_error);        // create não sobrescreve (161)
        EXPECT_THROW(fm.open("nao_existe.dat", FileKind::Data), std::system_error);  // open não cria (161)
    }
    FileManager fm(cfg);
    const FileId f = fm.open("a.dat", FileKind::Data);
    EXPECT_TRUE(fm.is_free(f, 1));
    EXPECT_FALSE(fm.is_free(f, 2));
    std::vector<std::byte> buf(cfg.block_size);
    fm.read_block(BlockId{f, 2}, buf);
    EXPECT_TRUE(buf[100] == std::byte{7});
    EXPECT_EQ(fm.allocate_block(f), 1u);   // reaproveita o bloco livre do bitmap
}

TEST(FileManager, BlockSizeDiferenteNaAberturaLanca) {
    auto cfg = test_config("fm_bs");
    {
        FileManager fm(cfg);
        fm.create("a.dat", FileKind::Data);
    }
    cfg.block_size = 8192;
    FileManager fm(cfg);
    EXPECT_THROW(fm.open("a.dat", FileKind::Data), std::runtime_error);
}

TEST(FileManager, CabecalhoTruncadoLancaEmVezDeTravar) {
    const auto cfg = test_config("fm_trunc");
    FileManager fm(cfg);
    { std::ofstream(cfg.data_dir / "t.dat") << "curto"; }
    EXPECT_THROW(fm.open("t.dat", FileKind::Data), std::runtime_error);   // pread devolve 0: erro (161)
}

TEST(FileManager, ClassificaSequencialEAleatorio) {
    const auto cfg = test_config("fm_seq");
    FileManager fm(cfg);
    const FileId f = fm.create("a.dat", FileKind::Data);   // grava o bloco 0
    for (int i = 0; i < 3; ++i) fm.allocate_block(f);
    Counters c0;
    fm.add_counts(c0);
    std::vector<std::byte> buf(cfg.block_size);
    for (std::uint32_t b = 1; b <= 3; ++b) fm.write_block(BlockId{f, b}, buf);   // 1 segue o 0: tudo sequencial
    fm.read_block(BlockId{f, 1}, buf);                                           // volta: aleatória
    fm.read_block(BlockId{f, 2}, buf);                                           // sequencial
    Counters c1;
    fm.add_counts(c1);
    const auto d = (c1 - c0).global;
    EXPECT_EQ(d.writes_seq, 3u);
    EXPECT_EQ(d.writes_rand, 0u);
    EXPECT_EQ(d.reads_rand, 1u);
    EXPECT_EQ(d.reads_seq, 1u);
}

TEST(BufferPool, AcertosFaltasEAlocacoes) {
    BufferPool pool(test_config("bp_hits"));
    const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 3);
    const Counters s0 = pool.snapshot();
    { auto g = pool.fetch(BlockId{f, 1}, kRandomReuse, kNoReservation); }
    { auto g = pool.fetch(BlockId{f, 1}, kRandomReuse, kNoReservation); }
    { auto g = pool.fetch(BlockId{f, 2}, kRandomReuse, kNoReservation); }
    { auto [b, g] = pool.allocate(f, kNoReservation); }
    const auto d = (pool.snapshot() - s0).global;
    EXPECT_EQ(d.requests, 3u);
    EXPECT_EQ(d.hits, 1u);
    EXPECT_EQ(d.misses, 2u);
    EXPECT_EQ(d.requests, d.hits + d.misses);   // allocate não soma em requisições (138)
    EXPECT_EQ(d.allocations, 1u);
    EXPECT_EQ(d.reads(), 2u);
}

TEST(BufferPool, SubpoolNaoPassaDeM) {
    BufferPool pool(test_config("bp_sub"));
    const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 6);
    Reservation r = pool.reserve(4);
    std::vector<PageGuard> held;
    for (std::uint32_t b = 1; b <= 4; ++b) held.push_back(pool.fetch(BlockId{f, b}, kRandomReuse, r.id()));
    EXPECT_EQ(r.pinned(), 4u);
    EXPECT_THROW(pool.fetch(BlockId{f, 5}, kRandomReuse, r.id()), std::runtime_error);
    held.clear();
    EXPECT_EQ(r.peak_pinned(), 4u);
}

TEST(BufferPool, LruEMruEscolhemVitimasDiferentes) {
    for (Policy p : {Policy::Lru, Policy::Mru}) {
        BufferPool pool(test_config(std::string("bp_pol_") + to_string(p), p));
        const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 5);
        Reservation r = pool.reserve(3);
        for (std::uint32_t b = 1; b <= 4; ++b) { auto g = pool.fetch(BlockId{f, b}, kRandomReuse, r.id()); }
        const Counters s0 = pool.snapshot();
        { auto g = pool.fetch(BlockId{f, 1}, kRandomReuse, r.id()); }
        const auto d = (pool.snapshot() - s0).global;
        if (p == Policy::Lru) EXPECT_EQ(d.misses, 1u);   // o bloco 1 foi a vítima
        else EXPECT_EQ(d.hits, 1u);                       // a vítima foi o 3
    }
}

TEST(BufferPool, TossImmediateViraProximaVitima) {
    auto cfg = test_config("bp_toss");
    cfg.toss_immediate = true;
    BufferPool pool(cfg);
    const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 5);
    Reservation r = pool.reserve(3);
    { auto g = pool.fetch(BlockId{f, 1}, kSeqReuse, r.id()); }
    { auto g = pool.fetch(BlockId{f, 2}, kSeqOnce, r.id()); }   // reuse = false
    { auto g = pool.fetch(BlockId{f, 3}, kSeqReuse, r.id()); }
    { auto g = pool.fetch(BlockId{f, 4}, kSeqReuse, r.id()); }  // expulsa o 2
    const Counters s0 = pool.snapshot();
    { auto g = pool.fetch(BlockId{f, 1}, kSeqReuse, r.id()); }
    EXPECT_EQ((pool.snapshot() - s0).global.hits, 1u);
}

TEST(BufferPool, FreeBlockDePaginaFixadaLanca) {
    BufferPool pool(test_config("bp_free"));
    const FileId f = pool.create("a.dat", FileKind::Data);
    auto [b, g] = pool.allocate(f, kNoReservation);
    g.write()[10] = std::byte{9};
    EXPECT_THROW(pool.free_block(b), std::logic_error);                 // (168)
    EXPECT_THROW(pool.free_block(BlockId{f, 0}), std::logic_error);     // cabeçalho
    g.reset();
    EXPECT_NO_THROW(pool.free_block(b));
    auto [b2, g2] = pool.allocate(f, kNoReservation);
    EXPECT_EQ(b2.block, b.block);                                       // reaproveitado
    EXPECT_TRUE(g2.read()[10] == std::byte{0});                         // sem dados antigos
}

TEST(BufferPool, FlushAllContaEDepoisZera) {
    BufferPool pool(test_config("bp_flush"));
    const FileId f = pool.create("a.dat", FileKind::Data);
    { auto [b, g] = pool.allocate(f, kNoReservation); }
    { auto [b, g] = pool.allocate(f, kNoReservation); }
    EXPECT_EQ(pool.flush_all(), 3u);   // 2 páginas + cabeçalho
    EXPECT_EQ(pool.flush_all(), 0u);
}

TEST(BufferPool, InvalidateAllComCatalogoFixado) {
    BufferPool pool(test_config("bp_inval"));
    const FileId cat = pool.create("c.cat", FileKind::Catalog);
    auto [cb, cg] = pool.allocate(cat, kNoReservation);   // fica fixada (144)
    const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 2);
    pool.flush_all();
    EXPECT_NO_THROW(pool.invalidate_all());
    auto g = pool.fetch(BlockId{f, 1}, kRandomReuse, kNoReservation);
    EXPECT_THROW(pool.invalidate_all(), std::logic_error);
}

TEST(BufferPool, ReservaLiberadaLancaLogicError) {
    BufferPool pool(test_config("bp_rel"));
    const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 2);
    ReservationId id;
    {
        Reservation r = pool.reserve(2);
        id = r.id();
    }
    EXPECT_THROW(pool.fetch(BlockId{f, 1}, kRandomOnce, id), std::logic_error);   // (155)
}

TEST(BufferPool, PedidoSemReservaEmDadosEContado) {
    BufferPool pool(test_config("bp_area"));
    const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 2);
    const FileId ix = file_with_blocks(pool, "i.idx", FileKind::Index, 1);
    const Counters s0 = pool.snapshot();
    { auto g = pool.fetch(BlockId{ix, 1}, kRandomReuse, kNoReservation); }
    Reservation r = pool.reserve(2);
    { auto g = pool.fetch(BlockId{f, 1}, kRandomOnce, r.id()); }
    EXPECT_EQ((pool.snapshot() - s0).global.data_area_requests, 0u);
    { auto g = pool.fetch(BlockId{f, 2}, kRandomOnce, kNoReservation); }
    EXPECT_EQ((pool.snapshot() - s0).global.data_area_requests, 1u);   // (159)
}

TEST(BufferPool, MigracaoParaOSubpoolContaComoAcerto) {
    BufferPool pool(test_config("bp_mig"));
    const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 2);
    { auto g = pool.fetch(BlockId{f, 1}, kRandomReuse, kNoReservation); }
    Reservation r = pool.reserve(2);
    const Counters s0 = pool.snapshot();
    { auto g = pool.fetch(BlockId{f, 1}, kRandomOnce, r.id()); }
    const auto d = (pool.snapshot() - s0).global;
    EXPECT_EQ(d.hits, 1u);
    EXPECT_EQ(d.reads(), 0u);
}

TEST(BufferPool, PrefetchLeAteOFimDoExtent) {
    auto cfg = test_config("bp_pref");
    cfg.prefetch = true;
    BufferPool pool(cfg);
    const FileId f = file_with_blocks(pool, "a.dat", FileKind::Data, 10);
    Reservation r = pool.reserve(8);
    const Counters s0 = pool.snapshot();
    { auto g = pool.fetch(BlockId{f, 1}, kSeqOnce, r.id()); }
    EXPECT_EQ((pool.snapshot() - s0).global.reads(), 8u);   // o bloco e mais extent_blocks − 1
    { auto g = pool.fetch(BlockId{f, 2}, kSeqOnce, r.id()); }
    EXPECT_EQ((pool.snapshot() - s0).global.hits, 1u);
}

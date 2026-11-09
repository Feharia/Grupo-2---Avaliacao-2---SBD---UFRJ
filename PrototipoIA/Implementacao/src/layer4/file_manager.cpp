#include "layer4/file_manager.hpp"

#include <system_error>

namespace cbd {

namespace {
constexpr std::uint32_t kMagic = 0x31444243;   // "CBD1"
constexpr std::uint16_t kVersion = 1;
constexpr std::uint32_t kHeaderFixed = 24;     // campos fixos antes do bitmap
}  // namespace

FileManager::FileManager(const Config& cfg)
    : block_size_(cfg.block_size), extent_blocks_(cfg.extent_blocks), dir_(cfg.data_dir) {
    if (block_size_ < 512 || (block_size_ & (block_size_ - 1)) != 0)
        throw std::invalid_argument("block_size precisa ser potência de 2 ≥ 512");
    if (extent_blocks_ == 0) throw std::invalid_argument("extent_blocks = 0");
    std::filesystem::create_directories(dir_);
    files_.emplace_back();   // FileId 0 inválido
}

FileManager::~FileManager() {
    try {
        flush_headers();
    } catch (...) {
        // destrutor não lança (155); o cabeçalho pode ficar desatualizado
    }
}

FileManager::OpenFile& FileManager::file(FileId f) {
    if (f == 0 || f >= files_.size() || !files_[f].alive) throw std::logic_error("FileId inválido ou removido");
    return files_[f];
}
const FileManager::OpenFile& FileManager::file(FileId f) const {
    if (f == 0 || f >= files_.size() || !files_[f].alive) throw std::logic_error("FileId inválido ou removido");
    return files_[f];
}

std::uint32_t FileManager::bitmap_capacity() const { return (block_size_ - kHeaderFixed) * 8; }

FileId FileManager::create(const std::string& name, FileKind kind) {
    OpenFile of;
    of.path = dir_ / name;
    of.name = name;
    of.kind = kind;
    of.fd = create_excl(of.path);
    of.alive = true;
    of.blocks = 1;              // bloco 0 = cabeçalho (temporários também o reservam, sem gravá-lo)
    of.free_map.assign(1, false);
    ftruncate_to(of.fd.get(), offset_of(1));
    files_.push_back(std::move(of));
    const FileId id = static_cast<FileId>(files_.size() - 1);
    if (kind != FileKind::Temp) write_header(id);   // inicializa e grava o bloco 0 (161)
    return id;
}

FileId FileManager::open(const std::string& name, FileKind kind) {
    if (kind == FileKind::Temp) throw std::logic_error("temporários só são criados, nunca reabertos");
    OpenFile of;
    of.path = dir_ / name;
    of.name = name;
    of.kind = kind;
    of.fd = open_existing(of.path);
    of.alive = true;
    files_.push_back(std::move(of));
    const FileId id = static_cast<FileId>(files_.size() - 1);
    try {
        read_header(id);
    } catch (...) {
        files_[id].alive = false;
        files_[id].fd.reset();
        throw;
    }
    return id;
}

void FileManager::write_header(FileId id) {
    OpenFile& of = file(id);
    std::vector<std::byte> buf(block_size_, std::byte{0});
    store<std::uint32_t>(buf.data() + 0, kMagic);
    store<std::uint16_t>(buf.data() + 4, kVersion);
    store<std::uint8_t>(buf.data() + 6, static_cast<std::uint8_t>(of.kind));
    store<std::uint32_t>(buf.data() + 8, block_size_);
    store<std::uint32_t>(buf.data() + 12, extent_blocks_);
    store<std::uint32_t>(buf.data() + 16, of.blocks);
    for (std::uint32_t b = 0; b < of.blocks; ++b) {
        if (!of.free_map[b]) {   // bit 1 = em uso
            buf[kHeaderFixed + b / 8] |= std::byte{static_cast<unsigned char>(1u << (b % 8))};
        }
    }
    pwrite_full(of.fd.get(), buf, offset_of(0));
    count(BlockId{id, 0}, true);
    of.header_dirty = false;
}

void FileManager::read_header(FileId id) {
    OpenFile& of = file(id);
    std::vector<std::byte> buf(block_size_);
    pread_full(of.fd.get(), buf, offset_of(0));
    count(BlockId{id, 0}, false);
    if (load<std::uint32_t>(buf.data()) != kMagic) throw std::runtime_error("cabeçalho inválido (magic): " + of.name);
    if (load<std::uint16_t>(buf.data() + 4) != kVersion) throw std::runtime_error("versão de formato diferente: " + of.name);
    if (load<std::uint32_t>(buf.data() + 8) != block_size_)
        throw std::runtime_error("block_size do arquivo difere do Config: " + of.name);
    of.blocks = load<std::uint32_t>(buf.data() + 16);
    if (of.blocks == 0 || of.blocks > bitmap_capacity()) throw std::runtime_error("nº de blocos inválido: " + of.name);
    of.free_map.assign(of.blocks, true);
    for (std::uint32_t b = 0; b < of.blocks; ++b) {
        const auto byte = static_cast<unsigned char>(buf[kHeaderFixed + b / 8]);
        of.free_map[b] = ((byte >> (b % 8)) & 1u) == 0;
    }
    of.free_map[0] = false;
}

void FileManager::grow(OpenFile& of) {
    const std::uint32_t nb = of.blocks + extent_blocks_;
    if (of.kind != FileKind::Temp && nb > bitmap_capacity())
        throw std::runtime_error("arquivo excede a capacidade do cabeçalho de 1 bloco: " + of.name);
    ftruncate_to(of.fd.get(), offset_of(nb));
    of.free_map.resize(nb, true);
    of.blocks = nb;
}

std::uint32_t FileManager::allocate_block(FileId id) {
    OpenFile& of = file(id);
    std::uint32_t b = 0;
    for (std::uint32_t i = 1; i < of.blocks; ++i) {
        if (of.free_map[i]) { b = i; break; }
    }
    if (b == 0) {
        const std::uint32_t first_new = of.blocks;
        grow(of);
        b = first_new;
    }
    of.free_map[b] = false;
    if (of.kind != FileKind::Temp) of.header_dirty = true;
    return b;
}

void FileManager::free_block(FileId id, std::uint32_t b) {
    OpenFile& of = file(id);
    if (b == 0 || b >= of.blocks) throw std::logic_error("free_block: bloco fora do arquivo ou cabeçalho");
    of.free_map[b] = true;
    if (of.kind != FileKind::Temp) of.header_dirty = true;
}

void FileManager::remove(FileId id) noexcept {
    if (id == 0 || id >= files_.size() || !files_[id].alive) return;
    OpenFile& of = files_[id];
    of.fd.reset();
    std::error_code ec;
    std::filesystem::remove(of.path, ec);
    of.alive = false;
    of.free_map.clear();
}

void FileManager::count(BlockId b, bool write) {
    const bool seq = last_.valid && last_.file == b.file && b.block == last_.block + 1;   // (102)
    auto bump = [&](IoCounts& c) {
        if (write) (seq ? c.writes_seq : c.writes_rand) += 1;
        else       (seq ? c.reads_seq  : c.reads_rand)  += 1;
    };
    bump(io_.global);
    bump(io_.per_kind[static_cast<std::size_t>(files_[b.file].kind)]);
    bump(io_.per_file[b.file]);
    last_.file = b.file;
    last_.block = b.block;
    last_.valid = true;
}

void FileManager::read_block(BlockId b, std::span<std::byte> out) {
    OpenFile& of = file(b.file);
    if (b.block >= of.blocks) throw std::out_of_range("read_block: bloco fora do arquivo");
    pread_full(of.fd.get(), out.first(block_size_), offset_of(b.block));
    count(b, false);
}

void FileManager::write_block(BlockId b, std::span<const std::byte> in) {
    OpenFile& of = file(b.file);
    if (b.block >= of.blocks) throw std::out_of_range("write_block: bloco fora do arquivo");
    pwrite_full(of.fd.get(), in.first(block_size_), offset_of(b.block));
    count(b, true);
}

std::uint64_t FileManager::flush_headers() {
    std::uint64_t n = 0;
    for (FileId id = 1; id < files_.size(); ++id) {
        if (files_[id].alive && files_[id].header_dirty && files_[id].kind != FileKind::Temp) {
            write_header(id);
            ++n;
        }
    }
    return n;
}

std::uint32_t FileManager::header_blocks(FileId id) const {
    return file(id).kind == FileKind::Temp ? 0 : 1;
}
std::uint32_t FileManager::blocks(FileId id) const { return file(id).blocks; }
bool FileManager::is_free(FileId id, std::uint32_t b) const {
    const auto& of = file(id);
    return b < of.blocks ? static_cast<bool>(of.free_map[b]) : true;
}
FileKind FileManager::kind(FileId id) const { return file(id).kind; }
const std::string& FileManager::name(FileId id) const { return file(id).name; }

void FileManager::add_counts(Counters& c) const {
    auto add_io = [](IoCounts& dst, const IoCounts& src) {
        dst.reads_seq += src.reads_seq;
        dst.reads_rand += src.reads_rand;
        dst.writes_seq += src.writes_seq;
        dst.writes_rand += src.writes_rand;
    };
    add_io(c.global, io_.global);
    for (std::size_t i = 0; i < 4; ++i) add_io(c.per_kind[i], io_.per_kind[i]);
    for (const auto& [f, io] : io_.per_file) add_io(c.per_file[f], io);
}

}  // namespace cbd

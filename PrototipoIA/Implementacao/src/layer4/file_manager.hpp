// Camada 4 — Gerente de arquivos: único ponto que conta leituras e gravações físicas (102, 154, 161).
#pragma once

#include "common/common.hpp"
#include "layer4/posix_io.hpp"

#include <string>
#include <vector>

namespace cbd {

class FileManager {
public:
    explicit FileManager(const Config&);
    ~FileManager();
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    FileId        create(const std::string& name, FileKind);   // grava o cabeçalho (exceto temp)
    FileId        open(const std::string& name, FileKind);     // lê e confere o cabeçalho
    std::uint32_t allocate_block(FileId);                      // nunca devolve 0
    void          free_block(FileId, std::uint32_t);
    void          remove(FileId) noexcept;
    void read_block (BlockId, std::span<std::byte> out);
    void write_block(BlockId, std::span<const std::byte> in);
    std::uint64_t flush_headers();                             // grava cabeçalhos sujos
    std::uint32_t header_blocks(FileId) const;
    std::uint32_t blocks(FileId) const;                        // inclui o cabeçalho
    bool          is_free(FileId, std::uint32_t) const;
    FileKind      kind(FileId) const;
    const std::string& name(FileId) const;
    std::uint32_t block_size() const { return block_size_; }
    void add_counts(Counters&) const;

private:
    struct OpenFile {
        UniqueFd              fd;
        std::filesystem::path path;
        std::string           name;
        FileKind              kind = FileKind::Data;
        std::uint32_t         blocks = 0;          // inclui o cabeçalho
        std::vector<bool>     free_map;            // espelho do bitmap do bloco 0 (true = livre)
        bool                  header_dirty = false;
        bool                  alive = false;
    };
    OpenFile&       file(FileId);
    const OpenFile& file(FileId) const;
    off_t offset_of(std::uint32_t block) const {
        return static_cast<off_t>(block) * static_cast<off_t>(block_size_);
    }
    void grow(OpenFile&);
    void write_header(FileId);
    void read_header(FileId);
    void count(BlockId, bool write);
    std::uint32_t bitmap_capacity() const;

    std::uint32_t block_size_;
    std::uint32_t extent_blocks_;
    std::filesystem::path dir_;
    std::vector<OpenFile> files_;        // índice = FileId; 0 é reservado
    struct { FileId file = 0; std::uint32_t block = 0; bool valid = false; } last_;
    Counters io_;
};

}  // namespace cbd

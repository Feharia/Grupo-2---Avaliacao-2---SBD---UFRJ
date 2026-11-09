// Camada 4 — dono de descritor e chamadas POSIX encapsuladas (156, 161).
#pragma once

#include <sys/types.h>

#include <filesystem>
#include <span>
#include <utility>

namespace cbd {

static_assert(sizeof(off_t) == 8, "off_t precisa ter 64 bits");

class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    UniqueFd(UniqueFd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    UniqueFd& operator=(UniqueFd&& o) noexcept {
        if (this != &o) { reset(); fd_ = std::exchange(o.fd_, -1); }
        return *this;
    }
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    ~UniqueFd() { reset(); }
    int  get() const noexcept { return fd_; }
    void reset() noexcept;
private:
    int fd_ = -1;
};

[[noreturn]] void throw_errno(const char* what);
UniqueFd create_excl(const std::filesystem::path&);     // O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC (161)
UniqueFd open_existing(const std::filesystem::path&);   // O_RDWR | O_CLOEXEC, sem O_CREAT (161)
void pread_full (int fd, std::span<std::byte> out, off_t offset);        // retorno 0 lança (161)
void pwrite_full(int fd, std::span<const std::byte> in, off_t offset);
void ftruncate_to(int fd, off_t size);

}  // namespace cbd

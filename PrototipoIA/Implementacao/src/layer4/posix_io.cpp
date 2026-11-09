#include "layer4/posix_io.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <system_error>

namespace cbd {

void UniqueFd::reset() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

UniqueFd create_excl(const std::filesystem::path& p) {
    const int fd = ::open(p.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) throw_errno(("create_excl " + p.string()).c_str());
    return UniqueFd{fd};
}

UniqueFd open_existing(const std::filesystem::path& p) {
    const int fd = ::open(p.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) throw_errno(("open_existing " + p.string()).c_str());
    return UniqueFd{fd};
}

void pread_full(int fd, std::span<std::byte> out, off_t offset) {
    std::size_t done = 0;
    while (done < out.size()) {
        const ssize_t n = ::pread(fd, out.data() + done, out.size() - done, offset + static_cast<off_t>(done));
        if (n < 0) {
            if (errno == EINTR) continue;
            throw_errno("pread");
        }
        if (n == 0) throw std::runtime_error("pread: fim de arquivo antes do bloco completo");   // (161)
        done += static_cast<std::size_t>(n);
    }
}

void pwrite_full(int fd, std::span<const std::byte> in, off_t offset) {
    std::size_t done = 0;
    while (done < in.size()) {
        const ssize_t n = ::pwrite(fd, in.data() + done, in.size() - done, offset + static_cast<off_t>(done));
        if (n < 0) {
            if (errno == EINTR) continue;
            throw_errno("pwrite");
        }
        if (n == 0) throw std::runtime_error("pwrite: nenhum byte gravado");
        done += static_cast<std::size_t>(n);
    }
}

void ftruncate_to(int fd, off_t size) {
    while (::ftruncate(fd, size) != 0) {
        if (errno == EINTR) continue;
        throw_errno("ftruncate");
    }
}

}  // namespace cbd

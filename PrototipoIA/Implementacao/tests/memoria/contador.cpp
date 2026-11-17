// operator new/delete globais com contagem (ver contador.hpp). Cada bloco leva um cabeçalho de 16 B
// com o tamanho pedido e a marca de que foi alocado dentro de um escopo, para a liberação subtrair
// só o que o escopo somou. 16 B mantêm o alinhamento do malloc.
#include "contador.hpp"

#include <algorithm>
#include <cstdlib>
#include <new>

namespace {

struct Estado {
    bool        armado = false;
    std::size_t vivos = 0, pico = 0;
};
thread_local Estado g;

constexpr std::size_t kCabecalho = 16;

void* alocar(std::size_t n) {
    auto* base = static_cast<std::size_t*>(std::malloc(n + kCabecalho));
    if (!base) return nullptr;
    base[0] = n;
    base[1] = g.armado ? 1 : 0;
    if (g.armado) {
        g.vivos += n;
        g.pico = std::max(g.pico, g.vivos);
    }
    return reinterpret_cast<std::byte*>(base) + kCabecalho;
}

void liberar(void* p) noexcept {
    if (!p) return;
    auto* base = reinterpret_cast<std::size_t*>(static_cast<std::byte*>(p) - kCabecalho);
    if (base[1] && g.armado) g.vivos -= std::min(g.vivos, base[0]);
    std::free(base);
}

}  // namespace

namespace contador {
Escopo::Escopo() { g = Estado{true, 0, 0}; }
Escopo::~Escopo() { g.armado = false; }
std::size_t Escopo::pico() const { return g.pico; }
std::size_t Escopo::vivos() const { return g.vivos; }
}  // namespace contador

void* operator new(std::size_t n) {
    if (void* p = alocar(n)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return alocar(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return alocar(n); }
void operator delete(void* p) noexcept { liberar(p); }
void operator delete[](void* p) noexcept { liberar(p); }
void operator delete(void* p, std::size_t) noexcept { liberar(p); }
void operator delete[](void* p, std::size_t) noexcept { liberar(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { liberar(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { liberar(p); }

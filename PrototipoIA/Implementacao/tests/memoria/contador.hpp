// Contador de alocações para os testes de memória auxiliar (CBD-003/004/005).
//
// O executável testes_memoria troca o operator new/delete globais (contador.cpp). Enquanto um
// Escopo está vivo, toda alocação feita na thread soma aos bytes vivos, e cada liberação de um
// bloco alocado dentro do escopo subtrai. O pico é o máximo de bytes vivos desde a abertura do
// escopo: mede o heap que o operador usa fora das molduras, inclusive a sobreposição momentânea
// do buffer antigo e do novo quando um vetor cresce.
//
// Fica num executável próprio para não mudar o comportamento da suíte principal sob ASan.
#pragma once

#include <cstddef>

namespace contador {

class Escopo {
public:
    Escopo();
    ~Escopo();
    Escopo(const Escopo&) = delete;
    Escopo& operator=(const Escopo&) = delete;
    std::size_t pico() const;    // máximo de bytes vivos alocados dentro do escopo
    std::size_t vivos() const;   // bytes alocados no escopo e ainda não liberados
};

}  // namespace contador

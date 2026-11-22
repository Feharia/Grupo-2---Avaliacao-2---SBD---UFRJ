# Matriz física dos experimentos

Plano de execução autorizado pelo usuário nesta conversa. Implementação
direta no worktree atual, com revisão ao final e commits apenas quando
autorizados.

Objetivo: atender às configurações físicas dos §§5.1 e 5.2 do PDF revisado
de 04/11/2026 e impedir aprovação de uma matriz apenas pelos rótulos.

Arquitetura: conservar as quatro camadas e os operadores. A bancada recebe
quatro heaps sem índices adicionais às variantes indexadas existentes.
Metadados de métodos vêm de `Monitor::access_paths`; outro CSV identifica
os campos e tabelas de cada combinação. O validador cruza esses arquivos
com as linhas executadas e suas repetições.

Tecnologias: C++20, CMake/CTest, Python padrão, SQLite.

## Sequência

- [x] Consolidar correções anteriores e verificar C++/Python.
- [x] Acrescentar testes negativos de configuração física, campos ausentes,
  métodos em outro campo, repetições incompletas e ausência de execução.
- [x] Observar falhas no validador atual, preservando logs.
- [x] Em `bench.cpp`, acrescentar variantes sem índice e publicar
  `metodos.csv`/`configuracoes.csv` com dados das interfaces reais.
- [x] Em `verificar_matriz.py`, validar as condições a–e e a–c para os
  campos declarados, verificar repetições reais e motivos de inviabilidade.
- [x] Atualizar o perfil de I/O para v2 com inventário explícito de 168
  combinações renomeadas. Preservar v1 e recusar comparação entre protocolos.
- [x] Executar suíte Python/C++, smoke com tuplas e rodada integral com
  SQLite, matriz e tabelas. Preservar resultados anteriores de exemplo.
- [x] Revisar o diff e registrar mudanças, comandos e limitações no
  POST_MORTEM.md e em evidências próprias.

## Condições verificadas

Método em outro campo não conta como índice do predicado. Ordem dos
operandos pode mudar entre algoritmos. Clustering em seleção c pode
coexistir com organização primária no campo. Rodada reduzida pode testar
função, mas não certifica o porte da matriz integral. Status inviável exige
motivo e mantém metadados físicos; sucesso requer as repetições efetivas.

Os casos adicionais do `compare.py` foram corrigidos em seguida, com
testes específicos de saída vazia e `--so`. O perfil comum de I/O
continuará compilando o mesmo driver contra as duas bibliotecas.

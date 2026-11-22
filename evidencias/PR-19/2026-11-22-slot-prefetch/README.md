# Regressões de SlottedPage e prefetch do PR #19

Base `f8177711ac3f6816f321948dff8bef798018b934`. Branch local
`fix/pr19-slot-prefetch`, em worktree próprio `pr19-slot-prefetch`.
Ambiente Linux, GCC 13.3.0, CMake 3.28.3 e GoogleTest real instalado no
prefixo `~/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395`.

## Contratos e regressões

- `SlottedPage.RejeitaSlotNaRegiaoLivre` é unitário. O layout público de
  `page.hpp` define `free_hi` como início da área de registros. Um registro
  inserido nessa fronteira é válido. Um slot vivo no início ou no último
  byte da região livre deve ser recusado na leitura, compactação e inserção,
  preservando a página quando a operação falha.
- `BufferPool.PrefetchIgnoraBlocosLivres` é de integração entre BufferPool,
  FileManager e arquivos temporários de teste. Blocos livres do extent,
  tanto nunca alocados quanto liberados, devem continuar disponíveis para
  alocação. A antecipação deve continuar até o bloco alocado posterior à
  lacuna. O teste confere conteúdo, cache e leituras calculadas à mão.

As expectativas vêm do layout e do ciclo de alocação. O cenário inicial
foi identificado em `/tmp/cbd-pr19-review-CUFYlj/review_checks.cpp` e
transformado em regressões permanentes nas suítes existentes.

## Evidência antes e depois

`testes.patch` preserva os testes adicionados sobre a base. Antes de tocar
nos fontes, `git diff --exit-code -- PrototipoIA/Implementacao/src` terminou
com sucesso. O build com esses testes está em `build-red.log`.

`red.log` registra duas falhas e código de saída 1. A SlottedPage aceita o
slot na região livre. O prefetch realiza oito leituras onde bastam uma ou
duas, e a alocação seguinte lança `allocate: bloco novo já estava no pool`.

`green.log` registra as duas regressões aprovadas após a mudança.
`suite.log` e `suite-details.log` registram a suíte Release completa,
com 89 testes no executável principal e 11 no de memória, todos aprovados.

`asan.log` e `asan-details.log` registram os mesmos 100 testes aprovados em
Debug com ASan/UBSan, em 199,80 segundos. LeakSanitizer foi desativado.
O build emite avisos preexistentes em testes sobre inicializadores e macros
em condicionais, preservados em `build-asan.log`.

O patch de produção troca `free_lo` por `free_hi` na validação do slot,
ajusta a mensagem de erro e consulta `is_free` antes de antecipar um bloco.
Os testes usam saídas próprias em `/tmp`.

## Comandos executados

A partir da raiz deste worktree, com logs salvos neste diretório.

```bash
cmake -S PrototipoIA/Implementacao -B /tmp/cbd-pr19-slot-prefetch-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395" \
  -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE
cmake --build /tmp/cbd-pr19-slot-prefetch-release --parallel 3
CBD_TEST_DIR=/tmp/cbd-pr19-slot-prefetch-red \
  /tmp/cbd-pr19-slot-prefetch-release/testes \
  --gtest_filter='SlottedPage.RejeitaSlotNaRegiaoLivre:BufferPool.PrefetchIgnoraBlocosLivres'
# Após aplicar a correção e recompilar, repetir o filtro com CBD_TEST_DIR distinto.
CBD_TEST_DIR=/tmp/cbd-pr19-slot-prefetch-suite \
  ctest --test-dir /tmp/cbd-pr19-slot-prefetch-release --output-on-failure

cmake -S PrototipoIA/Implementacao -B /tmp/cbd-pr19-slot-prefetch-asan \
  -DCMAKE_BUILD_TYPE=Debug -DCBD_SANITIZE=ON \
  -DCMAKE_PREFIX_PATH="$HOME/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395" \
  -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE
cmake --build /tmp/cbd-pr19-slot-prefetch-asan --parallel 3
CBD_TEST_DIR=/tmp/cbd-pr19-slot-prefetch-asan-data \
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/cbd-pr19-slot-prefetch-asan --output-on-failure
```

Para repetir a etapa anterior à correção, exportar a base em outro
diretório e aplicar somente `testes.patch`. Usar diretórios de build,
dados e evidências novos para cada reprodução.

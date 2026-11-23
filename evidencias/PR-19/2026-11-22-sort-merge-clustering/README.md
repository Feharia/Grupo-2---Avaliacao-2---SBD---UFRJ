# Sort-merge com fontes clustering sobre hash

Correção local na branch `fix/pr19-slot-prefetch`. A referência é a árvore
de `2f0148c` com a correção anterior de VarChar aplicada, preservada em
`../2026-11-22-sort-merge-variavel/correcao.patch`. A alteração deste item
está isolada em `correcao.patch`. Os fontes do Monitor imediatamente
anteriores também estão em `monitor-before.cpp` e `monitor-before.hpp`.

## Falha e contrato

O cursor clustering sobre hash conserva uma folha do índice e uma página
de dados simultaneamente. Duas fontes distintas precisam de quatro
molduras, mais o espaço do grupo de duplicatas quando utilizado.
O planejamento contava cada fonte como uma página. Com M=3 e chaves
UNIQUE, aceitava o plano e a execução lançava o erro de todas as molduras
fixadas. O mesmo ocorria com M=4 e grupo de uma página para duplicatas.

No clustering sobre B+ primária, a busca inicial solta o iterador do índice
antes de percorrer as folhas de dados. Esse cursor precisa de uma moldura
enquanto entrega seus registros. Fontes de temporários também conservam
uma página por run.

## Alteração

`JoinPlan.ordered_extra_frames` registra a moldura adicional de cada fonte
clustering/hash que será utilizada diretamente em ordem. O planejador a
desconta do fan-in final, verifica a viabilidade e inclui o custo em
`frames_used()`. Essa cobrança atua na fase final, quando os cursores são
abertos. Geração das runs e passes intermediários conservam seus limites.
O grupo de duplicatas e os descritores permanecem cobrados em M.

`bench.cpp` registra o fan-in final e o novo parâmetro em `parametros.csv`.
O campo novo é acessado com `requires` no template, mantendo a compilação
do driver contra headers de referência anteriores. Os consumidores e a
ordem de construção dos cursores foram conferidos em revisão adicional
restrita ao diff, sem defeitos apontados.

## Regressões e resultados

Dois testes de integração foram incorporados em `tests/test_layer2.cpp`.

- `SortMergeClusteringHashContaIndiceEDados` exige retorno Inviavel
  controlado abaixo de quatro molduras para UNIQUE e de cinco para o grupo
  de duplicatas deste cenário. Nos limites suficientes retorna três e
  nove pares, respectivamente. As contagens vêm das chaves de entrada.
- `SortMergeDistingueClusteringHashDeClusteringBPlus` cobre clustering/B+,
  fonte hash com B+ primária, hash com heap materializado e uma entrada
  heap que requer passe adicional para liberar a moldura do índice.

`red-initial.log` registra o primeiro teste falhando antes do patch e o
controle inicial passando. Após ampliar o teste da fonte heap, ambos os
testes falham contra a biblioteca anterior, em `red.log` (exit code 1).
`green.log` registra ambos passando. `suite.log` e `suite-details.log`
registram 94 testes principais e 11 de memória aprovados em Release.
`asan.log` e `asan-details.log` registram os mesmos 105 testes aprovados
com ASan/UBSan, em 52,79 segundos.

Os testes anteriores do PR continuam na suíte. Os avisos preexistentes de
compilação ficam nos logs. Builds e dados usam diretórios separados em
`/tmp`; `workdir.txt` identifica a pasta desta comparação.

## I/O

`comparacao.csv` e `comparacao.json` registram 168 cenários do perfil
existente, sem alterações em leituras, escritas ou flush. Ambos os lados
foram conferidos contra SQLite, incluindo tuplas (168 combinações e zero
divergências). Os casos dirigidos de clustering desta correção são
adicionais ao inventário do perfil, cobertos pelos dois novos testes.

As duas medições informam o mesmo SHA base, pois os dois patches locais
ainda estão sem commit. Os hashes em `medicao-before.json` e
`medicao-after.json`, com os patches preservados, identificam as árvores
efetivamente medidas. O compilador foi `/usr/bin/c++` nos dois lados.

## Comandos

Da raiz do worktree, com logs neste diretório.

```bash
cmake --build /tmp/cbd-pr19-slot-prefetch-release --parallel 3
CBD_TEST_DIR=/tmp/cbd-pr19-clust-final \
  /tmp/cbd-pr19-slot-prefetch-release/testes \
  --gtest_filter='Monitor.SortMergeClusteringHashContaIndiceEDados:Monitor.SortMergeDistingueClusteringHashDeClusteringBPlus'
CBD_TEST_DIR=/tmp/cbd-pr19-clust-suite-final \
  ctest --test-dir /tmp/cbd-pr19-slot-prefetch-release --output-on-failure
cmake --build /tmp/cbd-pr19-slot-prefetch-asan --parallel 3
CBD_TEST_DIR=/tmp/cbd-pr19-clust-asan \
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/cbd-pr19-slot-prefetch-asan --output-on-failure
```

O build de sanitizers usa Debug com `-g -O1`, `CBD_SANITIZE=ON` e GoogleTest
real. LeakSanitizer está desativado. Os comandos completos das medições de
I/O e conferências constam dos logs e metadados. Para repetir, exportar
`2f0148c`, aplicar o patch anterior de VarChar e depois, no candidato,
`correcao.patch` deste diretório. Usar pastas novas de build e resultados.

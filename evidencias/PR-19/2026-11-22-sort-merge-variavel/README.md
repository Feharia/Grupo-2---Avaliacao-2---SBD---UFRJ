# Revisão funcional do sort-merge com registros variáveis

Base da comparação `4091762d65a52495c02c9cf63da5bc630998ff4d`.
Candidato `2f0148c0ce16517bcf80de1c8bfe828e0b8b686b`, derivado do PR #19
em `f8177711ac3f6816f321948dff8bef798018b934`. As duas correções locais
anteriores tratam de SlottedPage e prefetch. O planejamento do sort-merge
permanece igual ao do PR revisado.

## Reprodução

`probe.cpp` é um teste de integração dirigido das camadas 2, 3 e 4.
Cada entrada contém 1.000 registros com chave Int32 UNIQUE de 0 a 999 e
texto `x`. O resultado esperado, calculado a partir dessas chaves, é de
1.000 pares. Cada registro codificado tem 7 bytes e cada heap ocupa três
blocos. Varia apenas o máximo declarado do VarChar, entre 24, 1.000 e
4.050. Todas essas larguras máximas cabem no formato de página de 4 KiB.

| Máximo VarChar | M | Base | Candidato |
|---|---|---|---|
| 24 | 5 | 1.000 pares | 1.000 pares |
| 24 | 16 | 1.000 pares | 1.000 pares |
| 1.000 | 5 | 1.000 pares | Planejamento retorna inviável |
| 1.000 | 16 | 1.000 pares | 1.000 pares |
| 4.050 | 5 | 1.000 pares | Planejamento retorna inviável |
| 4.050 | 16 | 1.000 pares | Execução excede o limite previsto de runs |

Em cada caso, uma chamada direta ao mesmo algoritmo do candidato com
reserva de cinco páginas e plano testemunha retorna 1.000 pares e o mesmo
checksum das execuções válidas. A geração de runs usa duas páginas de
scratch, uma de entrada, uma de saída e uma retida para descritores.
O merge usa fan-in 3 e a etapa final até quatro entradas. O vetor fica
limitado a quatro descritores por lado. O teste confirma viabilidade
algorítmica dentro dessa reserva. Não mede o pico de heap do processo.

`base.log` termina com sucesso. `current.log` termina com código 1,
esperado porque três dos seis casos pela interface Monitor falham.
O processo só retorna 2 se o plano testemunha produzir contagem errada.

## Causa localizada

- `src/layer2/monitor.cpp`, linhas 201–209, chama `capacity_for` usando
  o esquema e converte a capacidade em limite de runs e molduras de
  descritores. A redução das páginas disponíveis realimenta essa conta.
- `src/layer3/handles.cpp`, linhas 153–158, calcula essa capacidade usando
  `schema.record_size`, o máximo declarado. Os dados do exemplo têm
  tamanho efetivo muito menor. O plano pode gastar o orçamento numa lista
  de runs que o conjunto de dados não precisa.
- Em uma página, um registro máximo de 4.056 bytes mais prefixo e folga
  inicial de diretório produz capacidade zero. `ceil_div(n, 0)` retorna
  zero em `monitor.cpp:14`, e o `max(1, ...)` transforma isso em uma run.
  `make_runs`, em `algorithms.cpp:84–86`, aplica esse limite na execução,
  rejeitando a segunda run dos registros curtos.

O caso é uma regressão funcional de viabilidade relacionada à issue #7,
que trata da lista de runs em M. A contabilidade dos descritores deve ser
preservada. A solução precisa dimensionar a geração de runs com base nos
registros efetivos ou adaptar a intercalação dentro do limite de memória.
Usar apenas o tamanho médio também requer cuidado com distribuições e
fragmentação. A viabilidade de um registro concreto deve ser distinguida
da capacidade calculada para o máximo declarado.

## Comandos executados

Na raiz do worktree, bibliotecas Release já compiladas das revisões
indicadas. Os logs de compilação ficam neste diretório.

```bash
cbd_case=evidencias/PR-19/2026-11-22-sort-merge-variavel
g++ -std=c++20 -O2 \
  -I /tmp/cbd-pr19-review-CUFYlj/base/PrototipoIA/Implementacao/src \
  -I /tmp/cbd-pr19-review-CUFYlj/base/PrototipoIA/Implementacao/tests \
  "$cbd_case/probe.cpp" /tmp/cbd-pr19-review-CUFYlj/base-build/libcbd_motor.a \
  -o /tmp/cbd-pr19-var-base
CBD_TEST_DIR=/tmp/cbd-pr19-var-base-data /tmp/cbd-pr19-var-base
g++ -std=c++20 -O2 -DCBD_REVIEW_DIRECT \
  -I PrototipoIA/Implementacao/src -I PrototipoIA/Implementacao/tests \
  "$cbd_case/probe.cpp" /tmp/cbd-pr19-slot-prefetch-release/libcbd_motor.a \
  -o /tmp/cbd-pr19-var-current
CBD_TEST_DIR=/tmp/cbd-pr19-var-current-data /tmp/cbd-pr19-var-current
```

Usar novos caminhos para futuras execuções. Os logs anteriores preservam o
diagnóstico inicial. A continuação abaixo registra a correção local.

## Correção local e validação

Base da correção `2f0148c`; candidato é a árvore local dessa base com
`correcao.patch` aplicado. O SHA informado nas duas medições de I/O é o da
base, pois a correção ainda não tem commit. Os hashes de fontes em
`medicao-before.json` e `medicao-after.json` identificam as duas árvores.
`FONTES.sha256` preserva o diagnóstico inicial, com snapshots dos três
fontes anteriores em `fontes-antes/`. `CORRECAO.sha256` identifica a
implementação e as novas evidências.

- `TableStats.max_record_bytes` informa o maior tamanho codificado
  observado. CountingStream coleta o máximo durante as cargas existentes,
  insert o atualiza e reorganize o recalcula. Após remoções ele conserva
  um limite superior até reorganize. `r` mantém o máximo do esquema.
- O planejamento usa o máximo observado para limitar a capacidade das
  runs. A nova sobrecarga de `ScratchSpace::capacity_for` recebe o tamanho
  em bytes e reutiliza a mesma conta. Capacidade zero para entrada não
  vazia resulta em plano inviável explicitamente. Os limites da lista de
  runs e as molduras retidas continuam sendo respeitados.
- O máximo é um limite conservador, não uma previsão exata para toda
  distribuição de tamanhos. Não houve nova varredura de planejamento nem
  alteração nas fronteiras dos contadores. A estatística acrescenta um
  campo de tamanho constante por tabela. A atualização do catálogo neste
  protótipo ocorre em memória durante a execução.
- Os consumidores de TableStats foram conferidos; a suíte recompila as
  camadas e o driver, e a ferramenta de I/O compila cada versão com seus
  próprios headers. A API anterior de capacity_for foi preservada.

As três regressões em `tests/test_layer2.cpp` cobrem os máximos declarados
24/1.000/4.050 com M=5/16, uma mistura de textos curtos e longos após
inserção/reorganização e remoção/reorganização, e capacidade zero com
registro efetivamente grande. São testes de integração. Os pares esperados
vêm das chaves únicas da entrada, independentemente do planejamento.

`red.log` registra os três testes falhando antes da correção.
`green.log` registra os três aprovados. `suite.log` e `suite-details.log`
registram 92 testes principais e 11 de memória aprovados em Release.
`asan.log` e `asan-details.log` registram os mesmos 103 aprovados com
ASan/UBSan em 57,73 s, Debug com `-g -O1`, `CBD_SANITIZE=ON` e
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`.
`fixed.log` registra os seis casos originais aprovados, com checksum igual
ao da base. Os avisos de compilação anteriores permanecem nos logs.

Uma revisão adicional restrita ao diff confirmou os consumidores, a
manutenção da estatística e a cobrança do diretório, e reexecutou os três
testes dirigidos com sucesso em uma pasta própria.

### Comparação de leituras e escritas

Perfil existente `io-rapido-v1`, 168 cenários na escala 0,1. Ambos os lados
passaram pela conferência SQLite, incluindo tuplas. A comparação registrou
zero alterações em leituras, gravações e flush. Os cenários especiais que
eram inviáveis antes estão cobertos pelas regressões e pelo reproducer,
fora desse inventário de I/O.

A primeira tentativa de comparação foi recusada porque os metadados dos
compiladores registravam `/usr/bin/g++` e `/usr/bin/c++`. As medições foram
repetidas com `CXX=/usr/bin/c++` em ambos. Os resultados válidos estão em
`comparacao.csv`, `comparacao.json`, `resumo.md` e `medicao-*.json`.
Os logs de execução e conferência SQLite também estão preservados aqui.

```bash
# Na raiz do worktree, os três testes antes/depois usam o mesmo filtro.
cmake --build /tmp/cbd-pr19-slot-prefetch-release --parallel 3
CBD_TEST_DIR=/tmp/cbd-pr19-var-green \
  /tmp/cbd-pr19-slot-prefetch-release/testes \
  --gtest_filter='Monitor.SortMergeComVarCharCurtoIndependeDoMaximoDeclarado:Monitor.SortMergeVarCharAposManutencao:Monitor.SortMergeCapacidadeZeroViraInviavelNoPlano'
CBD_TEST_DIR=/tmp/cbd-pr19-var-suite \
  ctest --test-dir /tmp/cbd-pr19-slot-prefetch-release --output-on-failure

cmake -S PrototipoIA/Implementacao -B /tmp/cbd-pr19-slot-prefetch-asan \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS_DEBUG='-g -O1' -DCBD_SANITIZE=ON \
  -DCMAKE_PREFIX_PATH="$HOME/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395" \
  -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE
cmake --build /tmp/cbd-pr19-slot-prefetch-asan --parallel 3
CBD_TEST_DIR=/tmp/cbd-pr19-var-asan \
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/cbd-pr19-slot-prefetch-asan --output-on-failure

# io-workdir.txt contém a pasta isolada das medições desta execução.
cbd_io_dir=/tmp/cbd-pr19-var-io-panFVP
CXX=/usr/bin/c++ python3 -I PrototipoIA/Implementacao/tools/medir_io.py \
  --source "$cbd_io_dir/before/PrototipoIA/Implementacao" \
  --commit 2f0148c0ce16517bcf80de1c8bfe828e0b8b686b --saida "$cbd_io_dir/before-matched"
CXX=/usr/bin/c++ python3 -I PrototipoIA/Implementacao/tools/medir_io.py \
  --source "$PWD/PrototipoIA/Implementacao" \
  --commit 2f0148c0ce16517bcf80de1c8bfe828e0b8b686b \
  --build-dir /tmp/cbd-pr19-slot-prefetch-release --saida "$cbd_io_dir/after-matched"
python3 -I PrototipoIA/Implementacao/tools/comparar_io.py \
  "$cbd_io_dir/before-matched" "$cbd_io_dir/after-matched" "$cbd_io_dir/comparison-matched"
```

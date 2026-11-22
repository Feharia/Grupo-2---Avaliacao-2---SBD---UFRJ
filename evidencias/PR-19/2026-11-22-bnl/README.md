# BNL com diretório limitado e páginas parcialmente consumidas

Correção local sobre a árvore de `2f0148c` com os patches anteriores de
VarChar e clustering. O diff incremental deste item está em `correcao.patch`.
`monitor-before.cpp` e `algorithms-before.cpp` preservam os fontes anteriores.
`workdir.txt` identifica o checkout temporário usado como referência.

## Reprodução e correção

O executor já consegue retomar uma página externa quando seu diretório
enche, conservando o cursor e os guards em `carry`. O plano rejeitava uma
página densa quando a estimativa de registros por página excedia o diretório,
mesmo com três molduras suficientes para externa, interna e diretório.

O plano agora limita o diretório às molduras disponíveis ao chegar a um
chunk de uma página. O executor continua dividindo a página entre rodadas.
As molduras do diretório ficam retidas e a capacidade do vetor permanece
limitada a `dir_entries`. Vale `chunk + 1 + dir_frames <= M`.

Quando uma página termina exatamente no limite do diretório, sua retomada
pode produzir uma rodada vazia. O executor agora pula a varredura interna
nesse caso. A mesma condição elimina rodadas externas só com chaves NULL,
que não produzem pares.

## Testes e contadores

- `Monitor.BnlDividePaginaDensaComTresMolduras` usa 400 Int32 únicos por
  entrada e M=3, em LRU e MRU. A saída é de 400 pares. São três rodadas,
  quatro requisições de páginas e duas leituras, pois a página interna
  permanece em cache. Antes, o plano retornava inviável.
- `Monitor.BnlDiretorioExatamenteCheioNaoRevarreInternaVazia` usa 169, 170
  ou 171 linhas externas, contra 600 internas. Na ABI desta bancada, uma
  moldura comporta 170 RecordViews de 24 bytes. Os casos 169/170 exigem
  três requisições e leituras; 171 exige cinco.
- `MemoriaAuxiliar.BnlDiretorioDentroDasMolduras` passou a exigir M mínimo
  de três, com medição real de alocações para registros densos e variáveis.
  O pico de heap foi de 4.860 e 5.002 bytes em M=3, incluindo diretório de
  4.080 bytes. O controle adicional ficou em 780 e 922 bytes, abaixo do
  limite declarado de 1.408. Isso não representa o RSS do processo.

`red.log` registra os dois novos testes falhando antes da alteração.
`red-memory.log` registra a falha do requisito M=3 na medição de memória.
`green.log` e `green-memory.log` registram os resultados aprovados.

Comparação dirigida, preservada em `boundary-before.log` e `green.log`:

| Linhas externas | Antes | Depois |
|---|---|---|
| 169 | 169 pares, 3 leituras | 169 pares, 3 leituras |
| 170 | 170 pares, 5 leituras | 170 pares, 3 leituras |
| 171 | Inviável | 171 pares, 5 leituras |

As expectativas vêm das chaves e da sequência de páginas calculada à mão.
São testes de integração; o teste da fórmula em `test_estimativas.cpp` é
unitário. A versão antes da correção com logs adicionais foi compilada
contra os headers e a biblioteca da árvore de referência.

## Estimativa

A revisão apontou uma premissa relacionada aos casos recém-aceitos.
`estimate_join` previa rodadas somente por `ceil(b_outer/chunk)`. Em 171
linhas externas o diretório exige duas rodadas, com custo de cinco
transferências neste cenário, enquanto a fórmula previa três.

`red-estimate.log` registra essa falha e o teste unitário calculado à mão.
A fórmula passou a usar o maior de `ceil(b_outer/chunk)` e
`ceil(n_outer/dir_entries)`. Planos sintéticos sem diretório conservam a
fórmula anterior. A execução permanece igual.

É um modelo sem cache e ainda aproximado quanto a arredondamentos por
fronteiras de página/chunk. Por exemplo, duas páginas de 400 registros e
diretório de 170 exigem seis rodadas, enquanto o limite global prevê cinco.
O modelo declara essa limitação; distribuição uniforme sozinha não
garante exatidão. No caso denso de 400 linhas por entrada, cache explica
as duas leituras medidas, apesar de quatro requisições.

## Validação completa e I/O

`suite.log` e `suite-details.log` registram 96 testes principais e 11 de
memória aprovados em Release. Os avisos anteriores de compilação estão nos
logs. A revisão adicional do diff confirmou pares, guards, limite do
diretório e a nova fórmula, com a aproximação acima explicitada.
`asan.log` e `asan-details.log` registram os mesmos 107 testes aprovados
com ASan/UBSan, em 54,14 segundos. `asan-before-estimate.log` preserva
também a validação intermediária anterior à atualização da fórmula.

O perfil existente de 168 cenários preservou todas as leituras, gravações
e flush, com zero divergências SQLite incluindo tuplas nos dois lados.
`comparacao.*`, `resumo.md`, `sqlite-*.log` e `medicao-*.json` preservam a
comparação. Os casos dirigidos são adicionais ao inventário desse perfil.

As medições usam o SHA base `2f0148c` com hashes das fontes efetivas, pois
os patches ainda são locais e sem commit. O driver é o mesmo nos dois
lados, compilado contra os respectivos headers. O compilador foi
`/usr/bin/c++` em ambos.

## Reprodução

Da raiz do worktree, usar diretórios novos para novas rodadas.

```bash
cmake --build /tmp/cbd-pr19-slot-prefetch-release --parallel 3
CBD_TEST_DIR=/tmp/cbd-pr19-bnl-green-final \
  /tmp/cbd-pr19-slot-prefetch-release/testes \
  --gtest_filter='Estimativas.Bnl:Monitor.BnlDividePaginaDensaComTresMolduras:Monitor.BnlDiretorioExatamenteCheioNaoRevarreInternaVazia'
CBD_TEST_DIR=/tmp/cbd-pr19-bnl-suite-complete \
  ctest --test-dir /tmp/cbd-pr19-slot-prefetch-release --output-on-failure
cmake --build /tmp/cbd-pr19-slot-prefetch-asan --parallel 3
CBD_TEST_DIR=/tmp/cbd-pr19-bnl-asan-final \
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/cbd-pr19-slot-prefetch-asan --output-on-failure
```

O build ASan/UBSan usa Debug com `-g -O1`, `CBD_SANITIZE=ON` e GoogleTest
real. LeakSanitizer está desativado. Para reproduzir a versão anterior,
exportar `2f0148c` e aplicar somente os patches anteriores de VarChar e
clustering. No candidato, aplicar também `correcao.patch` deste diretório.

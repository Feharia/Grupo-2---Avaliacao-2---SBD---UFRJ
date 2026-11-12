# CBD-001 — correção, validação e desempenho

## Histórico preservado

- Branch ativo: `fix/cbd-001-ocupacao-bplus`.
- Commit do estado anterior: `4d5f1b639f144b6781eaf054fab7fc9c79062a7c`,
  `test: adiciona regressões de integração para CBD-001`.
- O commit inclui o estado local solicitado: testes, logs RED, post-mortem,
  script de preparação de GoogleTest e configuração de skills existente.
- O motor nesse commit é idêntico a `b61b87b4c3363e84eb199a21afaaee063a74e775`.
  A correção está na working tree, sem segundo commit e sem push.
- As evidências RED anteriores continuam intactas. Seu hash do fonte de
  testes identifica a versão do commit acima, não o arquivo ampliado atual.

## Alteração implementada

- `fill` limitado inferiormente pela ocupação mínima.
- Folhas fixas: fusão ou redistribuição da cauda; separadores e links atualizados.
- Variáveis: a primeira página subocupada é identificada durante a carga,
  sem uma varredura adicional para localizá-la. Quando necessário, o sufixo é
  repartido por prefixos de bytes e atingibilidade, recuando fronteiras até
  encontrar solução válida. Payloads são copiados página a página pelo pool;
  as folhas substituídas são liberadas e a primeira folha/raiz é atualizada.
- Internos: raiz usa sua capacidade completa quando isso evita um nível;
  os demais agrupamentos respeitam mínimo e máximo de filhos.
- Verificador também examina a última folha não raiz.
- Não há callbacks de manutenção enquanto os RIDs da carga ainda não foram
  publicados; os índices secundários são reconstruídos depois, como já ocorre
  em StorageManager::bulk_load/reorganize.
- Headers públicos, layout, M, políticas e contadores não foram alterados.

## Testes e revisão

Foram ampliados os sete testes anteriores para **14 testes de integração**.
Além de 22/7.500 registros, cobrem 7.518 sem cauda subocupada, 10.731 para
internos não raiz reais, fills 0,1/0,5/0,7/1,0, dados variáveis e índices.

A revisão independente encontrou dois casos de VarChar de tamanhos distintos
que a primeira tentativa não resolvia. As regressões foram escritas e
executadas antes do segundo ajuste (`var-red.log`). A versão final resolve
ambos. A revisão final foi aprovada no escopo funcional.

Resultados canônicos da versão final:

| Verificação | Resultado | Evidência |
|---|---|---|
| CMake Release + CTest | 54/54 aprovados | `validated-release-cases.log` |
| Debug + ASan/UBSan | 54/54 aprovados | `validated-sanitizers-cases.log` |
| Bancada rápida com tuplas + SQLite | 108 linhas, zero divergências | `validated-compare.log` |
| Oráculo determinístico independente | 2.500 casos: 1.045 válidos e 1.455 impossíveis, todos corretos | `validated-review-oracle.log` |
| Sintaxe do fonte com substituto local | Aprovada | comando no post-mortem |

LSan desativado. A bancada rápida não substitui a matriz experimental final.
O oráculo é preservado em `review-oracle.cpp` e foi recompilado/executado
pelo agente principal contra a biblioteca final. Ele confere conteúdo,
buscas, links, contagem, invariantes e aceitação/rejeição por atingibilidade.
Logs sem `validated-` incluem rodadas intermediárias, anteriores aos últimos
ajustes. Os arquivos RED não indicam falha pendente na versão final.

## Protocolo de desempenho

Driver comum: `tools/benchmark_cbd001.cpp`; executor:
`tools/comparar_cbd001.py`, ambos sob `PrototipoIA/Implementacao/`.

- GCC 13.3, C++20, bibliotecas CMake Release `-O3 -DNDEBUG`, sem sanitizers.
  A biblioteca baseline foi compilada e preservada antes da edição do motor.
- Duas versões somente: baseline e correção. Duas rodadas independentes de
  confirmação, cada uma com dez repetições pareadas por cenário, ordem
  alternada. Total: 280 execuções do driver.
- Mesmos dados, fill 0,7, 4 KiB, LRU e áreas Data/Index/Catalog/Work=32/16/4/32.
- Quatro cargas fixas: 22, 32, 7.518 e 20.000 registros de 127 bytes.
  Três variáveis: os dois casos encontrados na revisão (64 e cinco registros)
  e 20.000 registros variáveis com payload uniforme.
- Carga inclui `bulk_load` e flush de páginas/headers. Criação dos arquivos e
  inicialização da raiz ficam fora da janela. As formas/validações também.
- Pontos: lote determinístico de mil buscas, com invalidação do pool uma vez
  antes do lote. Inclui conferência do RID retornado.
- Varredura: pool invalidado antes; inclui conferência de ordem e bytes contra
  oráculo. Logo os tempos incluem essa validação comum às duas versões.
- Tempos de criação/remoção dos diretórios, inicialização do processo e
  invalidação não são incluídos nas fases.
- Leituras/gravações são transferências arquivo↔buffer, não I/O físico do SSD.
  Quantidades de folhas/internos são páginas usadas, não bytes físicos nem
  extents reservados. A cache do SO não foi esvaziada.
- A baseline é estruturalmente inválida nos cenários medidos, mas os pontos e
  multiconjuntos são conferidos nas duas versões. Toda execução corrigida
  precisa passar também a checagem estrutural independente (`valid=1`).

Valores brutos, medianas, mínimo/máximo e CV:
`desempenho/` (primeira rodada) e `confirmacao/` (segunda rodada).

### Exemplos da confirmação

| Cenário/fase | Antes | Depois | Observação |
|---|---:|---:|---|
| 22 fixos, carga | 20,44 µs | 13,41 µs | escritas 5 → 2 |
| 22 fixos, varredura | 2 leituras | 1 leitura | folhas 2 → 1, altura 2 → 1 |
| 32 fixos, carga | 20,56 µs | 34,96 µs | +70,1%; redistribuição necessária, mesmas cinco escritas |
| 7.518 fixos, mil pontos | 4.251,41 µs | 5.861,18 µs | +37,9%; leituras 1.051 → 1.049; altura 3 → 2 |
| 64 variáveis, carga | 34,20 µs | 67,08 µs | +96,1%; folhas 3 → 2 e distribuição agora válida |
| 20.000 fixos, carga | 5.304,98 µs | 5.269,68 µs | diferença pequena; escritas 959 → 958 |

Não houve melhora uniforme. O aumento do lote de pontos de 7.518 registros
apareceu nas duas rodadas (+28,9% e +37,9%). Uma hipótese, inferida do código
e ainda sem profiling, é o custo da busca linear nos separadores: a raiz
mais larga exige mais comparações, apesar de menos níveis. Não se preserva
uma árvore inválida para evitar esse custo e não se introduziu otimização
de busca fora do escopo do CBD-001.

Algumas séries excederam CV de 5%, sobretudo fases muito curtas. Os dados não
justificam alegação de convergência ou significância para pequenas diferenças.
As contagens de transferências e formas da árvore foram determinísticas nas
repetições; o tempo é complementar e sua dispersão está no JSON.

### Custo de memória e limite de escopo

O reparo variável usa metadados proporcionais aos registros do sufixo
(RIDs, prefixos, custos e predecessores), além das entradas de folhas. Não
materializa os payloads em RAM. Se precisar recuar muitas folhas, esse sufixo
pode ser grande e o planejamento pode ser repetido. Isso acontece na carga,
sem alterar a quota M de operadores de consulta. A comparação não mede pico
de RAM e não resolve o CBD-002. Não houve benchmark integral da matriz final.

## Reprodução a partir da raiz do checkout

Exemplo Linux, sem alterar HEAD nem reutilizar diretórios de saída:

```bash
set -euo pipefail
cbd_work="$(mktemp -d /tmp/cbd001-repro.XXXXXX)"
cbd_prefix="$HOME/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395"
mkdir "$cbd_work/baseline-source"
git archive 4d5f1b6 PrototipoIA/Implementacao | tar -xf - -C "$cbd_work/baseline-source"
cmake -S "$cbd_work/baseline-source/PrototipoIA/Implementacao" \
  -B "$cbd_work/baseline-build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$cbd_work/baseline-build" --target cbd_motor --parallel 2
cmake -S PrototipoIA/Implementacao -B "$cbd_work/fixed-build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$cbd_prefix" \
  -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE
cmake --build "$cbd_work/fixed-build" --parallel 2
CBD_TEST_DIR="$cbd_work/tests" ctest --test-dir "$cbd_work/fixed-build" --output-on-failure
for version in baseline fixed; do
  g++ -std=c++20 -O3 -DNDEBUG -IPrototipoIA/Implementacao/src \
    PrototipoIA/Implementacao/tools/benchmark_cbd001.cpp \
    "$cbd_work/$version-build/libcbd_motor.a" -o "$cbd_work/$version"
done
python3 -I PrototipoIA/Implementacao/tools/comparar_cbd001.py \
  "$cbd_work/baseline" "$cbd_work/fixed" "$cbd_work/resultados" --reps 10
```

Para sanitizers, criar outro build Debug com `-DCBD_SANITIZE=ON` e os mesmos
parâmetros de GoogleTest; executar CTest com
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` e
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

Builds usados nesta sessão: `/tmp/cbd001-fix-fqENRi/`. Os logs e resultados
relevantes estão preservados aqui, fora dos diretórios temporários.
`SHA256SUMS` identifica fonte final, drivers e evidências.

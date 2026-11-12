# CBD-001 — Evidência da fase RED

Baseline de produção: `b61b87b4c3363e84eb199a21afaaee063a74e775`.
Somente testes foram acrescentados; o motor, os headers, o CMake e o Makefile
permanecem iguais à baseline.

## Classificação e cobertura

Os sete casos de `BPlusBulkLoadIntegration` são **testes de integração**:
combinam BPlusEngine, SlottedPage, BufferPool e FileManager com arquivos reais.
Cada cenário cria um diretório exclusivo por `mkdtemp`, removido por RAII
depois do fechamento do pool. Nenhuma rede é usada durante os testes.

Parâmetros: páginas de 4.096 bytes, registros fixos de 127 bytes, chave Int32
UNIQUE crescente e preenchimento solicitado de 0,7. Capacidade da folha: 31;
mínimo por folha não raiz: 16. Máximo de chaves internas: 509;
mínimo de chaves em interno não raiz: 254.

| Caso | Comportamento verificado | Resultado na baseline |
|---|---|---|
| 22 registros | Todas as folhas respeitam o mínimo | Falha: última folha com 1 registro |
| 7.500 registros | Internos e folhas respeitam os mínimos | Falha: internos com 178 chaves e cauda com 3 registros |
| Verificador | Rejeita última folha reduzida a 15 registros | Falha: `verify(true)` aceita |
| Exceção da raiz | Raiz folha com 0, 1 ou 15 registros é válida | Passa |
| Controle válido | Carga de 42 registros preserva invariantes e conteúdo | Passa |
| Limites das folhas | Cargas de 21, 23, 31, 32, 33, 42 e 43 registros | Falha em cargas com cauda abaixo de 16 |
| Limites dos internos | Cargas de 7.499 e 7.501 registros | Falha por ocupação mínima |

O teste do verificador parte de uma árvore válida de 42 registros e remove
somente registros finais da última folha diretamente pela página. Mantém
links e separadores válidos e deixa 15 registros. Assim isola a omissão do
verificador, sem depender da permanência do erro no bulk load.

As cargas também são conferidas por um oráculo que percorre a cadeia de
folhas e compara quantidade, ordem e bytes de todos os registros com a
sequência esperada. Buscas pontuais conferem primeira, intermediária e última
chave. Não se exige uma distribuição exata nem altura específica da árvore:
uma correção pode redistribuir, fundir folhas ou reduzir a altura.

## Resultados executados

- `baseline-40.log`: binário da baseline anterior à inclusão dos novos testes;
  40 testes aprovados.
- `final-targeted-normal.log`: sete testes novos; dois controles aprovados e
  cinco casos falhando por asserções do CBD-001. Código de saída 1.
- `final-full-normal.log`: 47 testes; 42 aprovados, cinco falhando.
- `final-full-sanitizers.log`: 47 testes; 42 aprovados, os mesmos cinco
  falhando. Motor e testes instrumentados com ASan/UBSan; nenhum diagnóstico
  desses sanitizers. LSan desativado.
- Os arquivos `.exit` registram o código de saída correspondente. Nas rodadas
  completas, CTest retorna 8 porque o executável registrado falhou. O resumo
  `0% / 1 teste falhou` refere-se ao único executável registrado, não aos 47
  casos internos do GoogleTest.
- Logs sem o prefixo `final-` incluem execuções intermediárias de elaboração
  dos testes. Os resultados finais acima correspondem ao fonte entregue.
- A sintaxe do novo fonte também foi verificada sem `CBD_USE_GTEST`, para
  preservar a compatibilidade com o substituto usado pelo Makefile direto.

Não há correção do motor nem fase GREEN nesta entrega. As cinco falhas não
foram desativadas, marcadas como sucesso esperado ou convertidas em testes
que aceitam o comportamento incorreto.

## Reprodução

Execute da raiz do checkout, com a instalação local do GoogleTest preparada
pelo script `PrototipoIA/Implementacao/tools/preparar_googletest.sh`:

```bash
cbd_build="$(mktemp -d /tmp/cbd001-repro.XXXXXX)"
cbd_gtest="$HOME/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395"
cmake -S PrototipoIA/Implementacao -B "$cbd_build" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$cbd_gtest" \
  -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE
cmake --build "$cbd_build" --target testes --parallel 2
CBD_TEST_DIR="$cbd_build/dados" "$cbd_build/testes" \
  --gtest_filter='BPlusBulkLoadIntegration.*'
```

A última linha deve retornar 1 enquanto o CBD-001 permanecer sem correção.
Para a suíte completa, execute:

```bash
CBD_TEST_DIR="$cbd_build/dados" ctest --test-dir "$cbd_build" --output-on-failure
```

Para repetir com ASan/UBSan, use outro diretório novo, acrescente
`-DCBD_SANITIZE=ON` à configuração e execute os testes com
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` e
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

Builds desta execução: `/tmp/cbd001-red-BvnNfm/build-normal` e
`/tmp/cbd001-red-BvnNfm/build-sanitizers`. São temporários; os logs relevantes
foram preservados neste diretório do repositório.

`SHA256SUMS` identifica o fonte e os logs finais. Verifique da raiz do checkout
com `sha256sum --check evidencias/CBD-001/2026-11-12-red-zCo7RS/SHA256SUMS`.

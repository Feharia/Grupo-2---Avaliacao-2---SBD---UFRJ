# Validação local do gate de correção

Data: 13/11/2026. Branch `ci/gate-correcao`, criado sobre
`edb9c84502bcfbb79b5fb974ee866b46374214f3`. Mudanças de CI ainda sem commit.
Os arquivos usados estão identificados em `FONTES.sha256`, preservado como
registro original. O workflow foi ampliado depois pela suíte de I/O. Na
preparação para integração, sua versão anterior foi reconstruída em
`correcao-original.yml` e conferida contra o SHA256 já registrado: coincidência
exata, sem alterar o manifesto original. `FONTES_PRESERVADAS.sha256` aponta
para esse snapshot, permitindo conferir as evidências desta etapa da raiz:

```bash
sha256sum -c evidencias/CI/2026-11-13/FONTES_PRESERVADAS.sha256
```

O manifesto da entrega posterior de I/O está em
`evidencias/IO/2026-11-13/FONTES_PRESERVADAS.sha256`; o workflow corrigido
após a primeira execução hospedada está identificado em
`evidencias/CI/2026-11-13-contextos/FONTES.sha256`.

| Verificação | Resultado |
|---|---|
| CTest Release, com GoogleTest real | 59 casos, zero falhas; CTest 0,77 s |
| CTest Debug ASan/UBSan | 59 casos, zero falhas; CTest 65,18 s |
| Unitários Python do inventário/dumps | 10 testes aprovados, incluindo 34 subcasos de cenário ausente |
| Bancada reduzida com tuplas e SQLite | 108 execuções conferidas, 132 linhas não aplicáveis, zero divergências |
| GoogleTest indisponível | CMake rejeitou a configuração (exit 1) |
| Smoke vazio | Comparador anterior devolveu 0; novo verificador devolveu 1 |
| YAML e shell | PyYAML leu o YAML; todos os scripts passaram em `bash -n` |
| Agregador | Aceitou somente `success`; rejeitou `failure`, `cancelled` e `skipped` |

Os blocos `run` foram extraídos do próprio YAML e executados em bash com
`-e -o pipefail`, nas duas configurações. Localmente, checkout/instalação apt
e upload de artifacts foram omitidos. O GoogleTest já instalado foi localizado
por `CMAKE_PREFIX_PATH=/home/carlos/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395`.
Diretórios novos de build, teste e smoke: `/tmp/cbd-ci-kzV5kR/cbd-normal/` e
`/tmp/cbd-ci-kzV5kR/cbd-sanitizers/`. Os temporários não são evidência permanente;
os logs e XML relevantes foram copiados para este diretório.

Comandos de configuração/teste efetivamente usados:

```bash
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCBD_SANITIZE="$SANITIZE" -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE
cmake --build "$BUILD_DIR" --parallel 2
ctest --test-dir "$BUILD_DIR" --output-on-failure --no-tests=error \
  --output-junit "$EVIDENCE_DIR/ctest.xml"
python3 -I -m unittest discover -s tests/python -v
"$BUILD_DIR/bancada" --rapido --tuplas --saida "$SMOKE_DIR"
python3 -I tools/reference.py "$SMOKE_DIR" --tuplas
python3 -I tools/validar_smoke_ci.py "$SMOKE_DIR"
python3 -I tools/compare.py "$SMOKE_DIR" --tuplas
```

Configuração normal: Release/OFF. Sanitizers: Debug/ON, apenas CTest,
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` e
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. `GTEST_OUTPUT=xml:.../gtest.xml`
e `CBD_TEST_DIR` foram definidos em ambas as configurações como no workflow.

Para reproduzir o caso vazio: criar `resultados.csv` apenas com o cabeçalho
`politica,operacao,variante,algoritmo,rep,status`, `referencia.csv` apenas com
`operacao,rep,linhas,checksum`, e uma pasta vazia `tuplas_motor/`. Executar
`compare.py SAIDA --tuplas` e `validar_smoke_ci.py SAIDA`. A aprovação anterior
com zero execuções é a razão para o guard de cobertura no CI; o comparador
original não foi alterado.

A revisão independente não encontrou defeitos bloqueantes. Não houve execução
hospedada do GitHub Actions. `actionlint` não está instalado; parsing YAML e
validação bash não equivalem a executar o workflow no serviço. ASan/UBSan não
inclui LeakSanitizer neste protocolo. A matriz experimental integral e a
comparação de desempenho de I/O não fazem parte desta entrega.

# Gate de correção

O workflow [correcao.yml](../.github/workflows/correcao.yml) executa em pushes,
pull requests e acionamento manual, em Ubuntu 24.04 com GCC. O código continua
em `PrototipoIA/Implementacao/`.

| Job | Verificações |
|---|---|
| `verificar (normal)` | Release, GoogleTest real, toda a suíte CTest, testes Python, bancada reduzida comparada com SQLite e comparação de I/O contra referência |
| `verificar (sanitizers)` | Debug, GoogleTest real e toda a suíte CTest com ASan/UBSan |
| `gate-correcao` | Exige sucesso dos dois jobs; falha também se forem cancelados ou pulados |

O GoogleTest é instalado pelo pacote `libgtest-dev`. A configuração usa
`CMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE`: a ausência da dependência falha, em
vez de trocar silenciosamente para o shim local. Essa opção requer CMake
3.22+, disponível no runner escolhido. O projeto continua aceitando seu
fluxo local anterior; o gate não muda o CMake nem o motor.

O CTest registra um executável (`testes`), que atualmente contém **59 casos
GoogleTest**. O XML `gtest.xml` permite consultar cada caso. O outro XML,
`ctest.xml`, representa o executável registrado no CTest. Há também 28 testes
Python do guard de cobertura e das ferramentas de comparação de I/O,
sem dependências externas.

Nos sanitizers, `halt_on_error=1` torna os diagnósticos fatais; UBSan imprime
a pilha. `detect_leaks=0` mantém o protocolo já usado pelo projeto: esta rodada
verifica ASan/UBSan e não valida vazamentos com LeakSanitizer.

## Cobertura da bancada reduzida

O comando é `bancada --rapido --tuplas --saida DIRETORIO_NOVO`. Depois, a
referência SQLite é gerada, o inventário de execuções é verificado e
`compare.py --tuplas` compara contagens, checksums e multiconjuntos de tuplas.

O contrato mínimo de CI, em **ambas as políticas LRU e MRU**, exige:

- Seleções: `sel_pt_dre`, `sel_pt_cpf`, `sel_rg_dre_baixa`, `sel_rg_cr_baixa`,
  `sel_rg_curso_baixa`, `sel_rg_dre_alta`, `sel_rg_cr_alta`, `sel_rg_curso_alta`.
- Junções: `j_mestre_detalhe`, `j_pequenas` e `j_grande_pequena`, cada uma
  executada com `bnl`, `hash` e `sort_merge`.

Cada combinação precisa ter ao menos uma linha `ok`; cada linha `ok`,
inclusive de operações adicionais, precisa de seu dump. O verificador
rejeita duplicatas e status desconhecidos. Linhas `nao_aplicavel` e `inviavel`
são permitidas, mas não satisfazem o inventário obrigatório. Quantidades
exatas de linhas/resultados não são fixadas, para permitir novas variantes.

Convenção dos dumps: `POLITICA__OPERACAO__VARIANTE_ALGORITMO__REP.txt`, com
caracteres não alfanuméricos da variante substituídos por `_`. Nas seleções,
o algoritmo fica vazio no CSV e usa `sel` no nome do dump.

Os testes do verificador usam fixtures sintéticas e inventário escrito
explicitamente, sem obter expectativas executando o motor ou importando o
inventário do próprio verificador. É uma verificação de cobertura/presença;
a correção dos valores é verificada pelo comparador SQLite existente.

Essa rodada reduzida não certifica a matriz experimental integral. A
[suíte de I/O](IO.md) acrescenta comparação entre revisões com inventário
versionado de 108 execuções e alerta para aumentos, sem budget bloqueante.
O [consumo do Actions](CUSTO_ACTIONS.md) foi avaliado separadamente.

## Evidências e reprodução

Os artifacts `correcao-normal-N` e `correcao-sanitizers-N` preservam logs de
configuração/build/testes, XML, cache CMake e CSVs/tuplas disponíveis, inclusive
em falhas. `N` é a tentativa da execução. A retenção é de **14 dias**; baixar
e preservar evidências importantes da entrega antes disso. Arquivos de dados
internos do motor e os binários de build não são enviados.

Para reproduzir os comandos, consultar os blocos `run` do workflow. Exemplo
de uma configuração local normal, a partir de `PrototipoIA/Implementacao/`,
com GoogleTest instalado e diretório temporário novo:

```bash
ci_local=$(mktemp -d /tmp/cbd-ci-local-XXXXXX)
cmake -S . -B "$ci_local/build" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE
cmake --build "$ci_local/build" --parallel 2
CBD_TEST_DIR="$ci_local/dados-testes" \
  ctest --test-dir "$ci_local/build" --output-on-failure --no-tests=error
python3 -I -m unittest discover -s tests/python -v
"$ci_local/build/bancada" --rapido --tuplas --saida "$ci_local/smoke"
python3 -I tools/reference.py "$ci_local/smoke" --tuplas
python3 -I tools/validar_smoke_ci.py "$ci_local/smoke"
python3 -I tools/compare.py "$ci_local/smoke" --tuplas
```

Se o GoogleTest estiver em um prefixo próprio, acrescentar
`-DCMAKE_PREFIX_PATH=/caminho/do/prefixo` ao CMake. Para os sanitizers, usar
outro build com `-DCMAKE_BUILD_TYPE=Debug -DCBD_SANITIZE=ON` e executar CTest
com `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` e
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

## Tornar obrigatório antes de merges

O YAML executa verificações; a exigência de aprovação para merge é uma
configuração separada no GitHub. Depois de enviar este branch e obter uma
execução, criar/editar a regra ou ruleset da `main`, exigir pull requests e
selecionar **`gate-correcao`** como status obrigatório. Recomenda-se exigir que
o branch esteja atualizado com a base antes do merge. Se a regra usar merge
queue, adicionar também o evento `merge_group` ao workflow antes de ativá-la.

Não há filtros de caminhos: mudanças apenas de documentação também recebem
um resultado do gate. O workflow usa permissões de leitura, checkout sem
credenciais persistidas e actions fixadas por SHA. A atualização dessas
versões deve ser explícita.

Documentação oficial: [status checks](https://docs.github.com/en/pull-requests/reference/status-checks),
[eventos](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows)
e [artifacts](https://docs.github.com/en/actions/tutorials/store-and-share-data).

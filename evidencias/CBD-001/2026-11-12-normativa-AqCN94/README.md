# CBD-001 — revisão da especificação e dos oráculos de teste

## Escopo e autoridade

Esta etapa responde à pergunta sobre testes derivados da implementação.
Não altera o motor corrigido na etapa anterior. A igualdade do fonte é
verificada por `motor-antes.sha256`.

O usuário esclareceu que tem delegação prévia de todo o grupo para decidir
esses parâmetros. Não se exige aprovação coletiva adicional. Essa autoridade
e as decisões adotadas estão em
`docs/cbd001/CONTRATO_E_RASTREABILIDADE.md`, com fonte para cada propriedade.

Foram consultadas as seções pertinentes do PDF original de 04/11 e dos dois
DOCX de decisões. `fontes.sha256` identifica os arquivos do pacote de contexto
no diretório pai. O PDF exige definir e manter a ocupação mínima, mas não
estabelece percentual. A tabela histórica #32–38 contém “Valor sugerido”.
Os mínimos numéricos e a política variável são decisões autorizadas de
projeto, não citações do enunciado.

## O que foi revisado

- Cabeçalho auxiliar `tests/cbd001_spec_fixture.hpp`: somente biblioteca
  padrão, sem includes do motor. Constrói bytes e interpreta páginas conforme
  o formato técnico declarado no contrato.
- Esperados literais e contas independentes para 31/16 registros, 509/254 chaves
  e 2036 bytes. Nenhum esperado é obtido de `sp_capacity` ou `internal_cap`.
- Leitor de slots/internos próprio, com limites de acesso, contagem live,
  ordenação Int32, níveis, links e igualdade das folhas referenciadas na
  árvore com a cadeia. Não usa `read_header`, `sp_record`, `sp_used_bytes`
  ou `sorted_slots` como oráculo.
- Entradas construídas pelo teste, sem `encode_record`. Conteúdo comparado
  aos bytes conhecidos da fixture; APIs públicas comparadas a modelos
  `std::map`/`std::multiset` com valores lógicos conhecidos.
- Três testes da fachada StorageManager: UNIQUE com todas as buscas até
  15.811 linhas, intervalo inclusivo [20,42] sobre chaves pares e 48 linhas
  NOT UNIQUE com seis duplicatas por chave, antes/depois de reorganizar.
- 15.811 substituiu 10.731 no teste que requer três níveis: 31×510=15.810 é
  o máximo em dois níveis nessa fixture. Não se depende do arredondamento
  particular usado pelo bulk load para a meta de 70%.
- Negativo do verificador variável: 20 registros de custo 100 bytes ocupam
  2000<2036, apesar de 20>=16. Confere o critério em bytes separadamente do fixo.
- Rejeição por ausência de partição ordenada válida e por registro maior que
  a página. `bad_alloc` e `out_of_range` não são aceitos como prova de rejeição
  por inviabilidade. A API não possui código estruturado para todas as causas;
  essa limitação permanece explícita.

São **19 testes de integração**: 16 de estrutura/diagnóstico e três de
comportamento público. Com os 40 testes preexistentes, a suíte contém 59 casos.

## Resultados finais

| Execução | Resultado | Evidência |
|---|---|---|
| Biblioteca original preservada, 19 testes revisados | 13 falhas por asserções do contrato; seis controles passam; saída 1 | `final-red-baseline.log` |
| Motor corrigido, suíte inteira Release |59/59 passam | `full-release-cases.log` |
| Motor corrigido, suíte inteira Debug ASan/UBSan |59/59 passam | `full-sanitizers-cases.log` |
| Fixture compilada sem include path do motor | Compila | comando abaixo |
| Fonte de testes com substituto local | Sintaxe aprovada | comando abaixo |
| Hash do motor antes/depois | Igual | `motor-antes.sha256` |

LSan desativado. A rodada normal levou aproximadamente 0,73 s, e a instrumentada
64,90 s; esses são tempos de testes, não novas medidas de desempenho do produto.
Logs sem `final-`/`full-` podem representar versões intermediárias da revisão.

Os resultados anteriores de desempenho e SQLite continuam associados ao
mesmo fonte do motor. Não foram refeitos nesta etapa, pois não houve mudança
de produção nem do driver medido. Esta revisão não afirma conformidade com
toda a avaliação nem substitui a matriz experimental obrigatória.

## Revisão independente

O revisor recebeu primeiro as fontes normativas, sem abrir implementação ou
testes. Derivou o checklist de propriedades e, depois, revisou os testes contra
esse checklist. Identificou um esperado remanescente de 10.731 na troca do
cenário e a ausência de um negativo variável do verificador. Ambos foram
corrigidos antes da validação final.

Também motivou a checagem de níveis/folhas referenciadas e a distinção entre
erro de viabilidade e exceções acidentais. A revisão estática final não deixou
pendência material e aprovou os oráculos/rastreabilidade no escopo descrito.
O revisor não executou a suíte; os resultados acima foram obtidos pelo agente
principal.

## Reprodução

Na raiz do checkout, com GoogleTest local instalado:

```bash
cbd_gtest="$HOME/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395"
cbd_build="$(mktemp -d /tmp/cbd001-norma-repro.XXXXXX)"
cmake -S PrototipoIA/Implementacao -B "$cbd_build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$cbd_gtest" \
  -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE
cmake --build "$cbd_build" --parallel 2
CBD_TEST_DIR="$cbd_build/dados" ctest --test-dir "$cbd_build" --output-on-failure
```

Para a configuração instrumentada, outro diretório novo, Debug e
`-DCBD_SANITIZE=ON`, com `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` e
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` na execução.

Verificações de independência de compilação e de compatibilidade:

```bash
printf '#include "cbd001_spec_fixture.hpp"\n' | \
  c++ -std=c++20 -IPrototipoIA/Implementacao/tests -x c++ -fsyntax-only -
c++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow \
  -IPrototipoIA/Implementacao/src -IPrototipoIA/Implementacao/tests \
  -fsyntax-only PrototipoIA/Implementacao/tests/test_bplus_bulk_load_integration.cpp
```

Para reproduzir RED sem mudar HEAD, extrair `PrototipoIA/Implementacao` do
commit 4d5f1b6 com `git archive`, compilar apenas o target `cbd_motor` em outro
diretório e ligar o fonte de testes atual à biblioteca antiga e ao
`libgtest_main.a`/`libgtest.a`, definindo `CBD_USE_GTEST`. Foi usado esse
procedimento de ligação contra a biblioteca antiga preservada em
`/tmp/cbd001-fix-fqENRi/baseline-release/`, sem recompilá-la a partir do checkout
corrigido.

## Preservação e limites

- `testes-antes-revisao.cpp` preserva a versão anterior dos 14 testes. Seu hash
  corresponde ao fonte registrado na evidência GREEN anterior. Os manifestos
  históricos não foram reescritos para ocultar a mudança de oráculos.
- O antigo probe exploratório de 2.500 casos permanece como evidência anterior;
  ele não foi convertido em fundamento normativo nem integra estes oráculos
  revisados.
- O conhecimento do layout continua nos testes estruturais, explicitamente
  como contrato de fixture. Isso é diferente de deduzir o comportamento
  esperado do algoritmo ou de repetir suas funções de verificação.
- Os componentes reais do motor continuam sendo exercitados. Independência
  do oráculo não significa remover o sistema sob teste da integração.
- Builds desta etapa: `/tmp/cbd001-normativa-CAfI7a/` e os builds existentes
  `/tmp/cbd001-fix-fqENRi/fixed-release/` e `fixed-sanitizers/`.
- Não houve commit novo, push ou mudança de branch nesta etapa.

# Post-mortem — Avaliação 2 de Construção de Banco de Dados

## 2026-11-12 — Diagnóstico inicial com auxílio do Codex

- Baseline examinada: `b61b87b4c3363e84eb199a21afaaee063a74e775`.
- Working tree estava limpa antes e depois das verificações. Nenhum código de
  produção ou teste foi alterado; não houve commit, push ou PR.
- O pacote de contexto foi extraído no diretório pai do checkout. Foram lidos
  `AGENTS.md`, os contextos 00–03 e, antes das execuções, o contexto 05. As
  referências extensas não foram usadas para estabelecer novos requisitos.

### Ambiente e verificações executadas

- Linux x86_64, GCC 13.3, Make 4.3, CMake/CTest 3.28.3, Python 3.11.14 e
  SQLite do Python 3.45.1.
- GoogleTest externo não foi encontrado. O CMake selecionou o substituto
  `tests/mini_test.hpp`, com executor `tests/mini_main.cpp`.
- Build oficial CMake em Debug concluído, com avisos de inicializadores
  incompletos nos testes. CTest aprovou os 40 testes existentes.
- Bancada `--rapido --tuplas`, seguida de `tools/reference.py --tuplas`,
  `tools/compare.py --tuplas` e `tools/tabelas.py`: 108 linhas medidas,
  zero divergências e 13 tabelas geradas. Os 132 casos sem execução foram
  classificados pela bancada como `nao_aplicavel`; isso não comprova a
  cobertura obrigatória completa. A escala reduzida não valida a matriz final.
- A bancada integral e a suíte inteira com sanitizers não foram executadas
  nesta sessão.

### Defeitos reproduzidos na baseline

As provas reutilizaram os fontes dos probes do pacote de contexto, sem
incorporá-los à suíte permanente:

1. **CBD-001 — B+ bulk load:** 22 registros de 127 bytes geraram duas folhas,
   com cauda de um registro para mínimo de 16; `verify(true)` não reportou
   erro. Com 7.500 registros, reportou nó interno abaixo do mínimo.
2. **CBD-007 — Sort-Merge:** relações com dez registros de mesma chave em
   cada lado produziram dez pares com `group_pages=0`, contra cem com
   `group_pages=2`. O resultado esperado é cem pares.
3. **CBD-008 — página malformada:** bloco de 4.096 bytes com `nslots=65535`
   causou leitura inválida em `slot_at` (`src/layer3/page.cpp:19`). ASan
   reportou SEGV por leitura. O probe e `page.cpp` foram instrumentados com
   ASan/UBSan; o restante do motor veio da biblioteca Debug sem instrumentação.
4. **CBD-006 — bloco liberado:** o probe confirmou
   `fetch_after_free_accepted=1`.

Observações adicionais, sem classificação automática como novos defeitos:
o probe de memória registrou pedido auxiliar de 54.560 bytes para scratch
de 8.192 bytes; a conclusão depende do contrato de quota. O probe de reserva
registrou três guards simultâneos com dois na reserva. O teste dirigido dos
contadores apresentou 3 requests, 2 hits, 1 miss, 1 leitura e 1 escrita no flush.

### Evidências e próximos registros

- Builds, dados e logs desta execução: `/tmp/cbd-diagnostico-20261112-ReI4Zj/`.
  Esse diretório é temporário; sua conservação não é garantida.
- Fontes das provas: `codex_context/evidencias/auditoria_independente/probe.cpp`
  e `codex_context/evidencias/work_execucao/audit_probe.cpp`, no diretório pai.
- Nenhuma correção foi realizada: ainda não existem evidências de resultado
  após patch ou diff de correção nesta sessão.
- Foi recomendada integração do GoogleTest com revisão fixa via CMake.
  Essa integração ainda não foi implementada. A configuração existente já
  permite usar uma instalação externa via `find_package(GTest)`; o Makefile
  direto continua usando o substituto local.
- Por instrução explícita do usuário, alterações e verificações relevantes
  posteriores devem continuar sendo registradas neste arquivo, distinguindo
  resultados executados de recomendações e ações pendentes.

## 2026-11-12 — Script para preparar e validar GoogleTest

- A pedido do usuário, o procedimento de terminal foi convertido no arquivo
  `PrototipoIA/Implementacao/tools/preparar_googletest.sh`.
- O script determina os caminhos a partir de sua própria localização, instala
  a revisão fixa `03597a01ee50ed33e9dfd640b249b4be3799d395` no usuário,
  sem sudo, e utiliza a integração `find_package(GTest)` existente. Não altera
  o CMakeLists.txt, o Makefile ou os testes do motor.
- A configuração exige GoogleTest real e CMake >= 3.22; não aceita fallback
  silencioso. Prepara builds e dados separados para os testes normais e com
  ASan/UBSan; LSan fica desativado, conforme a rotina existente do projeto.
- Cada execução preserva seus logs em diretório temporário próprio e registra
  neste arquivo a última etapa e o código de saída, inclusive em caso de falha
  após o início do procedimento. Logs temporários precisam ser conservados
  para integrar evidências permanentes da entrega.
- Validação desta adição: sintaxe com `bash -n` e execução de `--help`.
  Instalação, acesso à rede e execução dos testes por este script não foram
  realizados pelo Codex; aguardam execução do usuário.

## 2026-11-12T14:48:07-03:00 — Preparação do GoogleTest pelo usuário
- SHA do projeto: b61b87b4c3363e84eb199a21afaaee063a74e775
- Script: PrototipoIA/Implementacao/tools/preparar_googletest.sh
- Revisão do GoogleTest: 03597a01ee50ed33e9dfd640b249b4be3799d395
- Instalação local: /home/carlos/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395
- Última etapa: concluído: testes normais e ASan/UBSan aprovados; código de saída: 0.
- Builds e logs: /tmp/cbd-googletest.XZl0qX (diretório temporário; conservar se necessário).
- LSan desativado na rodada com ASan/UBSan.

### Conferência dos resultados pelo Codex

- Os arquivos `build-normal/Testing/Temporary/LastTest.log` e
  `build-sanitizers/Testing/Temporary/LastTest.log`, sob o diretório acima,
  confirmam 40 testes de oito suítes em cada execução, todos aprovados.
- Os flags de ambas as compilações contêm `CBD_USE_GTEST`; a linkedição
  normal aponta para `libgtest_main.a` e `libgtest.a` da instalação local.
  Portanto, esta rodada utilizou GoogleTest real.
- O resumo `1/1` do CTest corresponde ao único executável registrado por
  `add_test(NAME testes COMMAND testes)`, e não a um único caso de teste.
  Não houve nova execução dos testes para esta conferência dos logs.

## 2026-11-12 — CBD-001: testes de regressão, fase RED

- Pedido: escrever testes que acusem o CBD-001 usando a skill `cpp-testing`.
  Nesta etapa não foi autorizada nem implementada a correção do motor.
- SHA de produção reconfirmado: `b61b87b4c3363e84eb199a21afaaee063a74e775`.
  Os probes anteriores voltaram a reproduzir a cauda de um registro com 22
  registros e o interno abaixo do mínimo com 7.500.
- Acrescentado `PrototipoIA/Implementacao/tests/test_bplus_bulk_load_integration.cpp`.
  Os sete testes são de integração, pois exercitam BPlusEngine, SlottedPage,
  BufferPool e FileManager reais. Não são testes unitários isolados.
- Cobertura: mínimos de folhas e internos para 22 e 7.500 registros; limites
  adjacentes; exceção da raiz; carga válida de controle; e rejeição de última
  folha deliberadamente reduzida de uma árvore inicialmente válida.
- O oráculo confere quantidade, ordem e bytes dos registros, além de buscas
  pontuais. As asserções de ocupação independem de `verify()`. Não se exige
  distribuição exata ou altura específica que impeça uma correção válida.
- Cada cenário possui diretório temporário exclusivo e limpeza por RAII.
  O novo fonte é incluído pelos globs existentes, sem modificar build ou API.
- Build CMake Debug com GoogleTest real: concluído. A verificação de sintaxe
  do novo fonte sem `CBD_USE_GTEST` também passou, mantendo compatibilidade
  com o substituto local. Nenhum aviso foi encontrado na compilação final
  incremental do novo arquivo.
- Verificações finais: sete testes novos, dois controles aprovados e cinco
  falhando por asserções esperadas. A suíte completa executou 47 testes,
  com 42 aprovados e os mesmos cinco falhando, tanto no build normal quanto
  com ASan/UBSan. Assim, os 40 testes preexistentes continuam aprovados.
  Não houve diagnóstico de ASan/UBSan; LSan ficou desativado.
- Evidência concreta: folha com 1 registro para mínimo 16; internos com 178
  chaves para mínimo 254; e `verify(true)` aceitando última folha com 15
  registros, apesar de `verify(false)` confirmar as demais invariantes.
- CTest retorna 8 porque o executável contém falhas; a execução dirigida do
  GoogleTest retorna 1. As falhas permanecem habilitadas: esta entrega é
  intencionalmente RED. Ainda não existe evidência após correção (GREEN).
- Fontes de produção, headers, Makefile e CMakeLists.txt continuam inalterados.
  Nenhum commit, push ou PR foi realizado. Arquivos preexistentes do usuário
  (`.agents/` e `skills-lock.json`, entre outros) foram preservados.
- Logs persistentes, classificação e comandos para reprodução:
  `evidencias/CBD-001/2026-11-12-red-zCo7RS/README.md`.

## 2026-11-12 — Proposta de solução do CBD-001, ainda não implementada

- A revisão do fonte confirmou três causas: a carga encerra a última folha
  sem ajustar a cauda; o número de internos depende apenas da meta `fill`,
  sem respeitar o mínimo por nó ou aproveitar a capacidade da raiz; e
  `verify(true)` exclui a última folha pelo teste `h.next != 0`.
- Proposta para folhas fixas: respeitar o mínimo também na meta de abertura
  de folhas e, ao fim do fluxo, ajustar a última folha com sua predecessora.
  Fundir se a soma couber em uma página; caso contrário, redistribuir mantendo
  ambas entre mínimo e máximo. Exemplo: 22 registros devem poder permanecer
  em uma única folha raiz; 32 podem ser distribuídos em 16 e 16.
- Proposta para internos: se todos os filhos couberem em uma raiz, criar
  somente essa raiz. Caso contrário, escolher o número de nós dentro do
  intervalo viável de mínimo/máximo de filhos e distribuir por quociente e
  resto. Para a configuração reproduzida, são 255 a 510 filhos por interno
  não raiz. `fill` deve ser uma meta subordinada às invariantes.
- Proposta para o verificador: remover a exclusão da última folha e manter
  a exceção de ocupação apenas para a raiz.
- Preservar links, primeiras chaves usadas como separadores, callbacks de
  mudança de RID e liberação de páginas fundidas. Reaproveitar os mecanismos
  existentes (`move_record`/`unlink_leaf`) quando adequados. Ocupação de
  registros variáveis deve continuar usando o critério em bytes; os sete
  testes atuais exercitam apenas registros fixos.
- Investigação adicional: 7.518 registros (358 folhas de 21) reproduzem o
  interno com 178 chaves sem nenhuma folha subocupada. Evidência em
  `evidencias/CBD-001/2026-11-12-red-zCo7RS/proposta-internos-7518.log`.
  Recomenda-se acrescentar esse caso à regressão: a fusão da cauda de 7.500
  registros pode, por si só, reduzir o número de folhas e esconder o erro
  independente de planejamento dos internos.
- Arquivos previstos para implementação: `src/layer3/bplus.cpp`, o fonte de
  testes de integração do CBD-001 e este post-mortem, além das evidências.
  A proposta não alterou código nem testes; não houve nova alegação de GREEN.

### Impactos de desempenho previstos — hipóteses, sem medição após patch

- A correção proposta altera a organização física e pode alterar ocupação,
  quantidade de folhas e internos, altura, RIDs e contadores de transferências.
  Esses efeitos precisam constar da interpretação dos experimentos.
- No exemplo de 22 registros, a fusão proposta reduz duas folhas para uma
  raiz folha. No exemplo de 7.518, os 358 filhos cabem numa única raiz interna
  de capacidade 510, permitindo reduzir a altura de três níveis para dois.
  São consequências do algoritmo proposto, não resultados já implementados.
- Ajustar a cauda acrescenta trabalho de movimentação, atualização de links
  e callbacks. O custo total de CPU e transferências durante a carga pode
  subir ou cair, pois também podem ser evitadas páginas e níveis internos.
- Menos páginas ou níveis podem beneficiar buscas e varreduras, mas caches,
  política de substituição e padrão de acesso impedem garantir antecipadamente
  melhora de tempo ou redução uniforme das leituras medidas.
- Validação proposta: comparar baseline e correção com os mesmos registros,
  ordem, fill, configuração de buffers, política, consultas e limites de fase;
  separar carga/manutenção de execução; registrar forma da árvore, contadores
  e tempos com repetições, além de exigir resultados corretos nos testes.
  Não modificar agora o formato dos resultados nem o contrato dos contadores.
- A baseline reproduzida viola invariantes. Eventual aumento de custo precisa
  ser descrito como diferença em relação a essa baseline inválida, sem usar
  desempenho como justificativa para preservar o defeito. Não foi executado
  benchmark novo nem implementada a correção nesta análise.

## 2026-11-12 — CBD-001 implementado, validado e medido

### Branch e baseline preservada

- Autorização explícita do usuário: criar branch, mover HEAD, stagear e
  commitar estado atual, depois implementar a proposta, testar e medir.
- Branch: `fix/cbd-001-ocupacao-bplus`.
- Commit: `4d5f1b639f144b6781eaf054fab7fc9c79062a7c`, mensagem
  `test: adiciona regressões de integração para CBD-001`.
- Foram incluídos os arquivos locais existentes, inclusive configuração de
  skills, script GoogleTest e evidências. O motor desse commit é a baseline
  original. Não houve push, PR, merge ou reescrita de histórico.
- A correção, os testes ampliados e as evidências novas permanecem na working
  tree para revisão; não foi feito um segundo commit automático.

### Correção e revisão de IA

- Mudanças de produção limitadas a `src/layer3/bplus.cpp`: metas de fill não
  ficam abaixo dos mínimos, cauda fixa é fundida/redistribuída, internos são
  agrupados dentro dos limites e o verificador examina a última folha.
- A revisão dos consumidores mostrou que callbacks de manutenção não podem
  ser usados para registros ainda não publicados do bulk load: índices podem
  estar vazios e são reconstruídos depois por StorageManager. Preservado o
  comportamento anterior de não emitir callbacks durante a carga.
- A skill `requesting-code-review` foi aplicada com um revisor independente,
  somente leitura. Ele encontrou dois contraexemplos de VarChar desigual:
  carga válida rejeitada ao ajustar só duas últimas folhas e folha
  intermediária subocupada por falta de espaço para o próximo registro.
- Ambas as regressões foram adicionadas e executadas em RED (`var-red.log`)
  antes do ajuste. O ramo variável passou a planejar uma partição válida do
  sufixo por bytes, recuando fronteiras quando necessário. Reconstrói folhas
  pelo Buffer Pool e libera as antigas, sem copiar todos os payloads para RAM.
  A posição da primeira folha problemática é registrada durante a carga,
  evitando varredura extra para localizá-la.
- Revisão final aprovada no escopo funcional. O agente principal recompilou
  e executou o probe independente contra a biblioteca final: 2.500 casos,
  1.045 válidos e 1.455 realmente impossíveis, todos conferidos contra oráculo
  de atingibilidade, conteúdo, links, buscas e invariantes.
- São agora 14 testes de integração do CBD-001: 54 testes no total. Não houve
  mudança em API pública, RID, layout, quota M, políticas ou contadores.

### Validação final executada

- CMake Release/CTest: 54/54 aprovados, em aproximadamente 0,54 s.
- Debug + ASan/UBSan: 54/54 aprovados, em aproximadamente 45,94 s.
  LSan desativado; não há alegação de ausência de vazamentos.
- Bancada rápida com tuplas, `reference.py` e `compare.py`: 108 linhas
  conferidas, zero divergências e 132 `nao_aplicavel`. Não é cobertura da
  matriz experimental integral.
- O fonte ampliado também passou na verificação de sintaxe sem GoogleTest:
  `c++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Isrc -Itests
  -fsyntax-only tests/test_bplus_bulk_load_integration.cpp`.
- Os builds completos mantêm avisos preexistentes de inicializadores em
  `tests/test_layer3.cpp`; eles não foram alterados nesta correção.

### Comparação de desempenho

- Driver comum `tools/benchmark_cbd001.cpp`, compilado contra bibliotecas
  Release da baseline e da correção, com GCC 13.3, `-O3 -DNDEBUG`.
- Executor `tools/comparar_cbd001.py`: duas rodadas independentes, cada uma
  com dez repetições pareadas e ordem alternada, sete cenários. Total de 280
  execuções. Fases separadas: carga com flush, lote de mil pontos e varredura.
  Mesmos dados, fill 0,7, páginas 4 KiB, LRU e frames 32/16/4/32.
- Medianas da confirmação: 22 fixos, carga 20,44 → 13,41 µs, escritas 5 → 2;
  varredura 2 → 1 leitura. Altura 2 → 1.
- 32 fixos, carga 20,56 → 34,96 µs (+70,1%), com cinco escritas em ambas:
  custo adicional de redistribuição para tornar a árvore válida.
- 7.518 fixos, altura 3 → 2, internos 3 → 1 e leituras do lote 1.051 → 1.049;
  o tempo do lote aumentou 4.251,41 → 5.861,18 µs (+37,9%). A primeira rodada
  também mostrou aumento (+28,9%). Busca linear em um nó mais largo é uma
  hipótese a partir do código, ainda sem profiling, não uma causa provada.
- 64 variáveis desiguais, carga 34,20 → 67,08 µs (+96,1%), folhas 3 → 2:
  custo de corrigir a distribuição, que antes violava invariantes.
- 20.000 fixos, carga 5.304,98 → 5.269,68 µs (diferença pequena), escritas
  959 → 958; folhas 953 → 952. Não houve ganho uniforme de tempo.
- Contadores de transferências e formas foram determinísticos. Algumas
  séries de tempo excederam CV 5%; não se afirma convergência ou significância
  para deltas pequenos. I/O é arquivo↔buffer, não I/O físico do SSD.
- O reparo variável mantém metadados O(registros do sufixo), que pode crescer
  e ser replanejado se fronteiras precisarem recuar. Essa memória pertence à
  carga e não altera M das consultas; pico de RAM não foi medido. CBD-002
  permanece uma tarefa independente.
- A baseline medida viola invariantes, embora seus resultados relacionais
  sejam conferidos pelo driver. A correção deve ser avaliada contra essa
  referência explicitamente inválida, sem conservar o defeito por desempenho.
- Fontes, comandos, CSV bruto, variabilidade e limites estão preservados em
  `evidencias/CBD-001/2026-11-12-green-voCJqx/README.md`, `desempenho/` e
  `confirmacao/`. Builds temporários: `/tmp/cbd001-fix-fqENRi/`.

### Esclarecimento sobre a origem e independência dos testes

- Ao ser questionado pelo usuário, o Codex confirmou que leu a implementação
  para construir fixtures, acessar páginas e elaborar regressões. As
  expectativas foram baseadas principalmente no backlog, nas reproduções
  anteriores e nos contratos encontrados no contexto/headers. Não houve
  derivação direta dos testes a partir do PDF normativo e das decisões
  aprovadas originais; falta essa rastreabilidade normativa explícita.
- Há dependências do código de produção também nas verificações: capacidades
  via `sp_capacity`/`internal_cap`, leitura via `read_header`/`sp_record`,
  ocupação variável via `sp_used_bytes`, ordenação via `sorted_slots` e bytes
  esperados via `encode_record`, usado também para gerar as entradas.
  Literais como 31, 509 e 2036 oferecem checagens adicionais, mas não removem
  todo risco de um erro compartilhado entre implementação e teste.
- A expressão anterior "oráculo independente" precisa ser qualificada:
  o oráculo de conteúdo é independente de `BPlusEngine::verify`, não de
  todas as rotinas da implementação. O probe separado de atingibilidade
  implementa sua própria decisão de partição, porém adota os mesmos limites
  estruturais; não prova por si só que esses limites representam a norma.
- As execuções RED/GREEN e os resultados relatados continuam factuais e
  demonstram detecção das falhas reproduzidas. Não devem ser apresentados
  como prova de conformidade integral com uma especificação independente.
- Próxima revisão recomendada: relacionar cada propriedade à fonte normativa
  ou decisão aprovada, derivar expectativas sem usar o motor e separar
  verificações da representação física dos testes de comportamento público.
  Esta anotação não alterou código, testes ou resultados existentes.

## 2026-11-12 — Revisão normativa e independência dos testes CBD-001

- Pedido do usuário: revisar as propriedades contra a especificação, rastrear
  suas fontes e reduzir dependências da implementação no oráculo.
- O usuário esclareceu: “Tenho aprovação prévia de todo o grupo para decidir
  sozinho esse e outros pontos similares.” Não há necessidade de ata nem de
  confirmação adicional do grupo para as decisões desta correção.
- Foram lidos os trechos pertinentes do PDF original de 04/11 (§3, §3.4,
  §4.2, recomendação de referência após §5.2 e §7.1), o registro D01–D22 e a
  tabela #32–38 do DOCX de decisões. O PDF exige definir/manter mínimo e
  encadear folhas, mas não fixa percentual, fan-out ou arredondamento.
- A tabela histórica diz “Valor sugerido”; não foi tratada como aprovação.
  Formalizados, sob a autoridade delegada ao usuário: metade da capacidade
  em registros fixos, metade da área útil em bytes vivos+slots para variáveis,
  mínimos por filhos em internos, exceção da raiz, fill como meta e rejeição
  controlada quando nenhuma partição ordenada válida existir. Não se promete
  rollback transacional nem se atribuem esses números ao professor.
- Contrato e matriz por teste escritos antes da revisão do fonte:
  `docs/cbd001/CONTRATO_E_RASTREABILIDADE.md`.
- Acrescentado `tests/cbd001_spec_fixture.hpp`, independente de headers do
  motor, com montagem de bytes e leitor de páginas próprios. Os mínimos são
  literais derivados no contrato; não vêm de `sp_capacity`/`internal_cap`.
- Os testes deixaram de usar `encode_record` para gerar entradas/esperados e
  `read_header`, `sp_record`, `sp_used_bytes` ou `sorted_slots` como oráculo.
  A remoção dirigida para testar o verificador também usa escrita própria.
- Os testes físicos continuam conhecendo um layout explícito de fixture,
  separado do enunciado. As APIs e o BufferPool são o sistema observado, não
  calculadores de expectativas.
- Acrescentados três testes públicos com modelos lógicos `std::map` e
  `std::multiset`: conteúdo/pontos UNIQUE, intervalo[20,42] e duplicatas NOT
  UNIQUE antes/depois de reorganizar. O caso grande consulta todas as 15.811
  chaves. RecordView é resultado observado; o modelo esperado não usa codecs.
- O teste de três níveis passou de 10.731 para 15.811 registros: 31×510=15.810
  é o máximo em dois níveis da fixture. A exigência de altura deixou de
  depender do arredondamento de fill particular da implementação.
- Incluídos casos de rejeição de partição impossível e registro maior que a
  página. `bad_alloc`/`out_of_range` não contam como rejeição semântica; a API
  atual não distingue todas as causas por código estruturado, limite mantido
  explícito sem impor mensagens concretas do motor.
- A revisão independente começou pelas fontes, sem abrir implementação ou
  testes. Depois identificou esperado antigo 10.731 remanescente e ausência
  de um negativo variável do verificador. Corrigidos antes da validação final.
- Novo negativo variável: última folha com 20 registros de custo 100 bytes
  totaliza 2000<2036, embora 20>=16. O leitor próprio comprova a ocupação;
  verify(false) aceita as demais invariantes e verify(true) deve rejeitar.
- A inspeção própria confere níveis pai/filho e igualdade entre as folhas
  referenciadas nos internos e a cadeia. A revisão estática final aprovou
  a independência/rastreabilidade nesse escopo, sem pendências materiais.
- Resultado final: 59/59 testes aprovados no Release/CTest (~0,73 s) e no
  Debug ASan/UBSan (~64,90 s, LSan desativado). São 40 preexistentes e 19 deste
  grupo (16 estruturais/diagnósticos e três públicos), todos de integração.
- Os mesmos 19 testes, ligados à biblioteca original preservada, produziram
  13 falhas de contrato e seis controles aprovados. Isso reconfirma RED
  sem retirar a correção da working tree nem alterar HEAD.
- A fixture compila sem include path do motor; o fonte de testes também
  passou na verificação de sintaxe usando o substituto local de GoogleTest.
- O hash de `src/layer3/bplus.cpp` não mudou nesta etapa. Não se repetiram
  benchmarks/SQLite: resultados anteriores continuam associados ao mesmo
  fonte de produção. Não houve novo commit, push ou mudança de branch.
- Fontes de teste anteriores preservadas; logs, hashes de fontes normativas,
  reprodução e limites em
  `evidencias/CBD-001/2026-11-12-normativa-AqCN94/README.md`.

### Conferência para encerramento do CBD-001

- Reexecutado CTest Release: suíte aprovada (~0,72 s). Confirmadas integridade
  dos hashes da revisão normativa e ausência de erros em `git diff --check`.
- A última execução ASan/UBSan preservada continua sendo59/59 aprovada e
  corresponde aos mesmos fontes conferidos pelos hashes; não foi repetida
  nesta conferência sem mudanças de código.
- Não há pendência técnica conhecida no escopo da correção. Desempenho misto
  e limites de medição/memória permanecem documentados; não se exigiu ganho
  uniforme nem otimização adicional para encerrar o CBD-001.
- Pendência de versionamento: o commit4d5f1b6 contém a baseline de testes;
  correção, revisão de testes, documentação e novas evidências ainda precisam
  de stage e commit final autorizado. Nada foi commitado ou enviado agora.

## 2026-11-12 — Versionamento final do CBD-001 autorizado

- O usuário autorizou explicitamente stage e commit da entrega final, mantendo
  o branch separado de `main`, pois há outras pendências independentes.
- Entrada preparada para acompanhar o commit
  `fix: corrige ocupação mínima da B+ no bulk load (CBD-001)` no branch
  `fix/cbd-001-ocupacao-bplus`: correção, testes revisados, contrato, ferramentas
  de comparação e evidências RED/GREEN/desempenho.
- Conferência anterior ao commit: hashes da revisão normativa válidos,
  `git diff --check` sem erros e CTest Release aprovado novamente (~0,74 s).
  A evidência ASan/UBSan de 59/59 permanece vinculada aos mesmos fontes.
- Ao stagear os CSVs brutos, o check do índice apontou seus finais CRLF,
  gerados pelo escritor CSV padrão. Os dois arquivos e hashes foram
  preservados. O check final passou com `cr-at-eol` explicitamente permitido,
  mantendo as verificações de espaços finais, linhas vazias finais e espaços
  antes de tabs; nenhum outro arquivo stageado contém CRLF.
- Os registros anteriores de alterações ainda sem commit descrevem o estado
  à época de cada execução; esta autorização encerra essa pendência local.
- Merge com `main`, push e resolução de outros problemas não fazem parte
  desta autorização. Não foram executados.

## 2026-11-13 — Avaliação de melhorias antes do merge, sem implementação

- O usuário pediu avaliar custo/benefício de CI, organização do repositório
  e acompanhamento de leituras/gravações por push/merge. Não solicitou
  implementação nem autorizou merge ou configuração remota nesta etapa.
- Checkout consultado: branch `fix/cbd-001-ocupacao-bplus`, HEAD `edb9c84`.
  Não há `.github/workflows` neste checkout. Não foi consultada a configuração
  administrativa remota para afirmar se já existem regras de proteção.
- Recomendação de alta prioridade: CI CMake com GoogleTest real obrigatório,
  suíte completa, configuração ASan/UBSan e validação funcional com SQLite;
  configurar os checks de correção como requisitos de merge da main. A
  proteção cobre os cenários declarados, não garante ausência de qualquer bug.
- Recomendação de alta prioridade: relatório de I/O por cenário comparando
  referência e candidato no mesmo protocolo, com dados/consultas/semente,
  M, política, fill, início de cache e fronteiras de flush fixados. Guardar
  leituras, gravações e total separadamente por carga, consulta e manutenção.
  Resultados incorretos, casos ausentes ou casos que deixam de executar devem
  ser tratados antes da comparação de desempenho.
- Começar aumentos de I/O como alertas para investigação, sem bloqueio
  automático universal. Correções de validade, memória ou contadores podem
  legitimamente aumentar o custo. Budgets futuros devem ter referência
  aprovada/versionada e atualização explícita, nunca sobrescrita silenciosa.
- O driver atual do CBD-001 pode fornecer a mecânica de comparação, mas só
  cobre B+. Uma suíte de regressão do sistema deve incluir seleções, os três
  joins e organizações/caminhos representativos, com inventário de cobertura.
  A rodada reduzida não certifica toda a matriz experimental obrigatória.
- Usar eventos de pull request para detectar problemas antes do merge e push
  para conferir novos estados; conservar SHAs comparados, CSV/JSON e resumo
  legível em artifacts. Retenção deve ser declarada; selecionar marcos para
  evidências permanentes da entrega, em vez de commitar cada execução de CI.
- Recomendação de prioridade intermediária: mover código ativo para uma raiz
  convencional (CMakeLists, src, tests, tools) e documentação histórica para
  docs, em alteração mecânica separada. Atualizar caminhos, scripts, README
  e referências históricas, sem reescrever camadas ou apagar a baseline da IA.
  O Git já preserva o código original. O README ainda cita 40 testes e Linux
  não testado; essas informações precisam de atualização independentemente
  de uma mudança de diretórios.
- Sequência sugerida: estabelecer CI, criar acompanhamento de I/O e realizar
  reorganização sob essas verificações, em mudanças separadas. Não deixar
  trabalho cosmético competir com requisitos obrigatórios ainda pendentes.
- Referências oficiais consultadas: GitHub Docs, Status checks
  (https://docs.github.com/en/pull-requests/reference/status-checks), eventos
  (https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows)
  e artifacts (https://docs.github.com/en/actions/tutorials/store-and-share-data).
- Esta seção registra propostas, não decisões implementadas. Somente o
  post-mortem foi atualizado; não houve commit, push, merge ou mudança remota.

## 2026-11-13 — Implementação local do gate de correção autorizada

- Após a avaliação acima, o usuário autorizou implementar a primeira melhoria,
  o CI de correção. Criado o branch local `ci/gate-correcao` sobre
  `edb9c84502bcfbb79b5fb974ee866b46374214f3`, preservando a entrega CBD-001.
  Nenhum commit, push, merge, PR ou ajuste administrativo remoto foi feito.
- Adicionado `.github/workflows/correcao.yml`: push, pull request e acionamento
  manual; Ubuntu 24.04/GCC; GoogleTest real obrigatório pelo CMake; suíte CTest
  Release e Debug ASan/UBSan. O check agregado `gate-correcao` aceita somente
  sucesso da matriz, também rejeitando falha, cancelamento ou skip. Logs, XML
  e resultados disponíveis são publicados como artifacts por 14 dias.
- No job normal, a bancada reduzida é executada em saída nova com `--tuplas`,
  referência SQLite e comparação de contagens/checksums/multiconjuntos.
  Não foram alterados o motor, contratos públicos ou contadores de I/O.
- Falha de validação reproduzida: `compare.py --tuplas` sozinho retorna sucesso
  com CSVs só de cabeçalhos e nenhum dump (zero linhas conferidas). Criado
  `tools/validar_smoke_ci.py` para exigir inventário mínimo explícito por
  política/seleção/join/algoritmo e dump para cada execução ok; o mesmo caso
  vazio agora termina com exit 1 antes da comparação. O comparador existente
  foi preservado. Este guard é específico do contrato de cobertura do CI.
- Os testes Python foram escritos antes do verificador, com inventário e
  fixtures explícitos, sem importar expectativas do motor ou constantes do
  verificador. A primeira execução falhou pela ausência do módulo. Na
  validação com dumps reais, corrigida a convenção das seleções: CSV deixa
  algoritmo vazio, mas o nome do dump usa `sel`. A fixture corrigida falhou
  antes do ajuste no verificador e passou depois.
- Validação fresca, executando os blocos shell extraídos do próprio YAML:
  Release, 59/59 casos GoogleTest aprovados (CTest 0,77 s); Debug ASan/UBSan,
  59/59 (65,18 s); 10 testes Python aprovados, incluindo remoção individual
  dos 34 cenários mínimos; smoke com 108 execuções conferidas, 132 linhas
  não aplicáveis e zero divergências SQLite, inclusive tuplas. LeakSanitizer
  permanece desativado (`detect_leaks=0`), conforme o protocolo anterior.
- Caso negativo adicional: configuração sem GoogleTest disponível falhou
  com exit 1; não houve fallback para o shim. YAML lido com PyYAML, scripts
  conferidos por `bash -n` e agregador testado com success/failure/cancelled/
  skipped. Revisão independente somente leitura sem achados bloqueantes.
- Evidências permanentes: `evidencias/CI/2026-11-13/`, com logs, XML e hashes
  dos arquivos usados. Builds/saídas temporários em `/tmp/cbd-ci-kzV5kR/`.
  Localmente foi usado o GoogleTest já instalado via CMAKE_PREFIX_PATH;
  checkout, apt e upload de artifacts não foram executados no host.
- Atualizados README e `docs/CI.md`, incluindo interpretação de 1 executável
  CTest versus 59 casos GoogleTest e ativação posterior da proteção da main.
  O YAML local ainda precisa ser enviado e executado no GitHub; depois,
  configurar `gate-correcao` como status obrigatório na regra da main.
  Criar o workflow não altera automaticamente essa regra. `actionlint` não
  está instalado; parsing e bash não substituem execução hospedada.
- Limites: o gate cobre os cenários declarados e não prova ausência de todo
  defeito nem certifica a matriz integral. Reorganização de diretórios e
  regressão de desempenho de I/O continuam fora desta implementação.

## 2026-11-13 — Suíte pequena de comparação de I/O e avaliação de consumo

- O usuário autorizou a suíte pequena de leituras/escritas e a análise do
  consumo do GitHub Actions. Depois confirmou que o repositório é público.
  Trabalho mantido no branch `ci/gate-correcao`, HEAD `edb9c84`; alterações
  anteriores de CI preservadas. Sem commit, push, PR, merge ou alterações
  administrativas/de cobrança.
- Adicionados `tools/medir_io.py`, `comparar_io.py`, `config_io.cpp` e
  `io_cenarios_v1.json`. O perfil usa a rodada reduzida existente, 108
  execuções com ambas as políticas. São testes de integração das quatro
  camadas, conferidos por SQLite e multiconjuntos de tuplas. O runner compila
  o mesmo driver da bancada contra cada biblioteca e seus respectivos
  headers, fixando fill, entradas e preparação do cache. Não há motor novo.
- Assinatura exige mesmos dados, lotes, esquema, SQL, constantes, M, semente,
  escala, repetições, Config e flags reais da biblioteca (compile_commands).
  O inventário versionado exige todas as 108 variantes. Resultados,
  status, duplicatas/ausências e integridade do CSV são conferidos antes
  dos deltas. Contadores e checksums não passam por float; porcentagem de
  aumento com base zero é indefinida.
- Leituras, escritas e total têm deltas por cenário; flush final e total com
  flush ficam separados. Aumentos são alertas sem budget bloqueante. Erro
  funcional, protocolo incompatível ou infraestrutura falha interrompe o
  job. A versão mede consultas; carga e manutenção ficam explicitamente fora
  porque a bancada atual não exporta suas janelas individuais.
- Testes Python foram escritos antes do comparador, com valores sintéticos
  independentes dos algoritmos. O inventário inicial foi escolhido a partir
  das consultas reduzidas já validadas: define casos a medir, não respostas
  esperadas. O oráculo funcional continua sendo SQLite.
- A revisão independente reproduziu duas lacunas na primeira versão:
  remover a mesma variante dos dois lados deixava passar 107 cenários; as
  flags do manifesto não incluíam todas as opções reais. Novos testes RED
  foram executados antes das correções. Incluídos inventário estrito e
  extração das flags reais; corrigida também a ausência de fill no manifesto
  usando um único driver nos dois lados. A revisão final confirmou a
  resolução e não encontrou novo defeito concreto.
- Validação final: 28 testes Python aprovados; CTest Release novamente com
  59/59 GoogleTest aprovados (0,77 s); bloco de I/O atual do YAML executado
  localmente com build fresco da biblioteca de referência. ASan/UBSan do
  motor inalterado permanece documentado na entrega de CI anterior.
- Comparação real `b61b87b` versus `edb9c84`: 108 consultas em cada lado,
  zero divergências SQLite, 132 linhas não aplicáveis; leituras totais
  45.480→45.456 (-24, redução de uma leitura em 24 cenários), escritas
  5.432→5.432, flush final zero, nenhum aumento. Repetição do candidato em
  outra saída produziu todos os deltas zero. Isso não mede o efeito da
  correção sobre carga nem permite generalizar para toda a matriz.
- Integração apenas no job normal existente: PR compara base SHA com merge
  testado; push compara before/after; branch nova usa pai; manual permite
  referência explícita. Sem pai, há comparação de controle com aviso.
  Resumo no GitHub e CSV/JSON/metadados/logs em artifacts por 14 dias;
  builds e novos dumps da suíte não são enviados. Biblioteca candidata é
  reutilizada; somente a biblioteca da referência recebe novo build.
- Evidências em `evidencias/IO/2026-11-13/`: dados brutos de resultados,
  relatórios, metadados, hashes, testes e validação do YAML/bash/seleção/gate.
  Temporários novos em `/tmp/cbd-io-00fhvO/`. O bloco completo de medição
  levou 30,245 s na referência (inclui build) e 12,912 s no candidato.
- Documentado em `docs/IO.md`, `CI.md` e `CUSTO_ACTIONS.md`; README atualizado.
  Segundo fontes oficiais consultadas em 13/11, runners padrão em
  repositórios públicos não descontam a franquia de computação: portanto
  não há limite de 1.000 minutos aplicável a este workflow público. A tabela
  atual do Free expressa US$12/mês de computação incluída para usos elegíveis
  (equivalente a cerca de 2.000 minutos do Linux de 2 cores a US$0,006).
  Para cenário privado, estimativa conservadora sem histórico hosted de
  8–12 minutos somados por execução; push+PR e reexecuções multiplicam isso.
- Armazenamento avaliado separadamente: aproximação local do artifact normal
  completo = 15.297.798 bytes ZIP (182 arquivos, nível 6). A maior parte vem
  dos dumps de correção já existentes. A franquia publicada de artifacts é
  500 MB, compartilhada com Packages; acompanhar uso e retenção. Retenção
  permaneceu 14 dias. Não foi consultado faturamento autenticado da conta.
- Fontes oficiais: https://docs.github.com/en/billing/concepts/product-billing/github-actions,
  https://docs.github.com/en/billing/reference/product-usage-included,
  https://docs.github.com/en/billing/reference/actions-runner-pricing e
  https://docs.github.com/en/actions/how-tos/write-workflows/choose-where-workflows-run/choose-the-runner-for-a-job.
- Pendente para ativação: envio autorizado e primeira execução hosted, e
  posterior regra obrigatória `gate-correcao` na main. A reorganização do
  repositório continua fora do escopo. Não foi alegada proteção remota ativa.

## 2026-11-13 — Preparação do índice para integração

- O usuário pediu organizar o que falta para merge/push, citando alterações
  não stageadas. A instrução anterior de autorização explícita para commits
  foi preservada: preparado o stage da entrega conjunta CI/I/O, sem executar
  commit, merge ou push. Mensagem proposta:
  `ci: adiciona gate de correção e comparação de I/O`.
- Checkout `ci/gate-correcao`, HEAD `edb9c84`. `main` local em `b61b87b` é
  ancestral de HEAD; os dois commits CBD-001 já estão no branch. A integração
  local admite fast-forward depois de versionar o conjunto stageado.
- Verificação fresca: 28 testes Python aprovados, 59 GoogleTest aprovados
  novamente via CTest Release (0,78 s), hashes da entrega I/O válidos. O
  motor não recebeu novas alterações. Revisões independentes de CI e I/O
  já registradas nas etapas anteriores, com achados resolvidos.
- O manifesto histórico de CI identifica o workflow anterior à suíte I/O.
  Preservado esse snapshot em `correcao-original.yml`, reconstruído a partir
  do atual e confirmado idêntico ao SHA256 original `3a00729d...`. Adicionado
  `FONTES_PRESERVADAS.sha256` para conferir o snapshot e os demais fontes
  históricos, mantendo `FONTES.sha256` original intacto. O manifesto I/O
  continua identificando a versão atual do workflow e ferramentas.
- Dois CSVs de referência SQLite têm CRLF, gerados pelo escritor padrão;
  conteúdo e hashes foram preservados. O log bruto `sem-gtest.log` termina
  com linha vazia produzida pelo CMake. Adicionado `.gitattributes` para
  preservar os bytes de CSV/logs de evidências, permitir CRLF nesses arquivos
  e tolerar linha vazia final apenas nos logs. Whitespace de código e
  documentação permanece com as verificações padrão.
  O conjunto preparado contém código, documentação e evidências textuais,
  sem builds, dumps adicionais de tuplas ou binários.
- Tentativa de atualizar a referência remota com `git fetch origin main`
  falhou por DNS (`Could not resolve hostname github.com`). Não foi possível
  confirmar o estado remoto atual: refazer fetch e verificar divergência
  antes de integrar. Nenhuma alteração foi enviada ao remoto.
- Pendências posteriores ao stage: autorização e commit; fetch bem-sucedido;
  envio do branch para a primeira execução hosted e conferência do gate;
  integração/envio da main autorizados e proteção `gate-correcao` configurada
  no GitHub. A reorganização de diretórios permanece uma melhoria opcional,
  não uma condição técnica imposta para concluir estas duas entregas.

## 2026-11-13 — Commit e envio do branch autorizados

- O usuário executou `git fetch origin` com exit 0 e autorizou as três
  etapas pendentes: commit, conferência do remoto e envio do branch com
  acompanhamento do Actions. Merge com main não integra essas três etapas.
- Conferida a referência local atualizada pelo fetch do usuário:
  `origin/main` em `b61b87b`, sem commits ausentes em `ci/gate-correcao`;
  antes deste commit, o branch estava dois commits à frente (CBD-001).
- Validação fresca anterior ao commit: 28 testes Python aprovados, CTest
  Release com 59 casos GoogleTest aprovado em 0,79 s, manifestos históricos
  e atuais válidos, `git diff --cached --check` aprovado, nenhuma alteração
  fora do índice. Autor Git já configurado como Carlos, com o endereço
  noreply usado nos commits anteriores.
- Commit preparado com a mensagem autorizada
  `ci: adiciona gate de correção e comparação de I/O`, incluindo o conjunto
  stageado e este registro. Sem alteração do motor nesta etapa.
- Uma nova tentativa de fetch pelo ambiente do agente ainda falhou em DNS
  (`Could not resolve hostname github.com`), apesar do fetch bem-sucedido no
  terminal do usuário. Esse limite pode impedir o push e a consulta do
  Actions pelo agente; não é falha de configuração do origin nem evidência
  de credenciais inválidas. Nenhuma aprovação de CI hospedado foi alegada.
- Resultado: criado `d3a7249b509c1e5fbc367f31e7fe4ad36c721d91` com CI/I/O,
  85 arquivos e autoria Carlos. A tentativa autorizada de
  `git push -u origin ci/gate-correcao` terminou com exit 128 por DNS de
  github.com. `gh run list` também não conseguiu conectar a api.github.com.
  Assim, o envio e a conferência hosted permanecem pendentes, sem merge.
  Este resultado é registrado em commit documental separado para manter a
  árvore limpa sem reescrever o commit já criado.

## 2026-11-13 — Primeiro push e falha hospedada de contexto no workflow

- O usuário executou o push de `ci/gate-correcao` com exit 0. HEAD e
  `origin/ci/gate-correcao` em `50d4278`, árvore inicialmente limpa. A
  conferência local repetiu 28 testes Python e 59 GoogleTest aprovados,
  validou hashes e as 108 medições preservadas, sem aumentos de I/O.
- A consulta do agente ao GitHub continuou indisponível por rede. O usuário
  forneceu a execução `43456714396`, concluída com `failure`, o resumo
  indicando problema de workflow, ausência de logs e as anotações exatas:
  cinco `Unrecognized named-value: 'runner'` nas linhas 37–41 de job.env.
  Portanto a falha ocorreu antes de compilar ou executar testes hospedados.
- Causa: o workflow criado pelo agente usava `${{ runner.temp }}` no `env`
  do job, onde o GitHub não admite o contexto runner. A validação local
  anterior de YAML/Bash não verificava essa restrição. Não foi um defeito
  reproduzido no motor ou uma reprovação dos 59 casos C++.
- Corrigidas as cinco definições em um step que usa `$RUNNER_TEMP` e grava
  em `$GITHUB_ENV`. Preservados caminhos e demais parâmetros. Acrescentada
  regressão dirigida derivada da regra oficial de disponibilidade de
  contextos: RED com cinco falhas antes, GREEN depois.
- Revisão independente encontrou um caminho de falha introduzido pela
  mudança: upload com `always()` e checkout falho poderia expandir caminho
  vazio em `/`. Corrigido para exigir sucesso do step `diretorios`, com
  teste RED/GREEN adicional. Revisão final sem novo defeito concreto.
- Validação final: 30 testes Python aprovados; step real de inicialização
  executado em Bash nos dois modos com os cinco caminhos corretos; YAML e
  sintaxe de todos os scripts conferidos. Motor/medição inalterados; os
  testes C++ e ASan anteriores permanecem vinculados aos mesmos fontes.
- Evidências em `evidencias/CI/2026-11-13-contextos/`, incluindo metadados,
  anotações fornecidas pelo usuário, RED/GREEN e validação local. Preservados
  snapshots exatos de workflow/docs anteriores em `evidencias/IO/.../fontes`
  e adicionado manifesto histórico sem reescrever os hashes originais.
- O log RED preserva o espaço final emitido pelo progresso de unittest
  antes dos subtests; exceção de whitespace limitada a esse arquivo em
  `.gitattributes`, sem alterar a saída bruta nem as verificações do código.
- Correção preparada para novo commit no mesmo branch, dentro da conclusão
  do CI autorizado; sem merge. Nova execução no GitHub ainda pendente de
  envio/consulta: não foi declarada aprovação hospedada após a correção.
- Referência normativa desta correção:
  https://docs.github.com/en/actions/reference/workflows-and-actions/contexts.

## 2026-11-13 — Integração na main autorizada

- O usuário informou que enviou `6ef8fbe6791025406750c2c351a75baddfda7185`
  e conferiu o Actions aprovado. As referências locais confirmaram o mesmo
  SHA em `ci/gate-correcao` e `origin/ci/gate-correcao`, com árvore limpa.
  A aprovação hospedada é informação do usuário; o agente não conseguiu
  consultá-la diretamente por rede.
- O usuário autorizou integrar na main, registrar esta conclusão, enviar
  a main, conferir seu Actions e tornar `gate-correcao` obrigatório.
- Antes da integração: 30 testes Python aprovados novamente e 59 GoogleTest
  via CTest Release (1,06 s), manifestos atuais e históricos íntegros.
  Revisão independente do diff completo `b61b87b..6ef8fbe` sem achados
  bloqueantes, incluindo consumidores da B+ e a configuração final do CI.
- Merge local concluído por fast-forward de `main` (`b61b87b`) para
  `ci/gate-correcao` (`6ef8fbe`), sem conflitos nem reescrita de histórico.
  Este registro acompanha um commit documental na main, após a integração;
  a nova revisão documental ainda precisa de validação hospedada.
- SSH e API pelo ambiente do agente continuam indisponíveis: fetch retorna
  falha de resolução de github.com, e gh não conecta a api.github.com.
  Não foi presumido que main remota ou proteção já estejam atualizadas.
  Existe plugin GitHub disponível, mas não está conectado nesta sessão.
- Preparado `/tmp/concluir-integracao-github.sh` para o terminal do usuário:
  exige a main limpa no SHA preparado, refaz fetch e confere ancestralidade,
  verifica o gate aprovado do branch, faz push sem force, aguarda o workflow
  da main no SHA exato e só então exige o check com a origem GitHub App
  observada. Se já houver proteção, altera apenas status checks, preservando
  os demais checks, app IDs e regras. Se não houver, cria proteção mínima
  de status checks com strict e enforce_admins, sem introduzir revisores.
- O helper guarda backup das regras anteriores e lê novamente a proteção
  para confirmar o gate e os checks herdados. Operações administrativas
  dependem dos privilégios já existentes da conta; o script não concede
  acesso nem remove regras para contornar rejeições. Sintaxe e três cenários
  de payload conferidos localmente, com revisão independente sem bloqueios.
- Permanecem pendentes até confirmação remota: envio da main, execução
  hospedada do commit documental final e ativação de status obrigatório.
  O helper foi preparado para executar essas etapas em sequência; este
  registro não declara que já ocorreram.

## 2026-11-16 — Correções da auditoria de 10/11 (P01–P15) com o Claude

### Origem, escopo e decisões

- Fonte: `PROBLEMAS_IDENTIFICADOS_CBD_2026-11-10.md` (P01 a P15), auditoria
  do commit `b61b87b`, conferida em 13/11 contra a main `4091762` e contra o
  enunciado de 04/11/2026 (v2: seletividade baixa de 1%, NATURAL JOIN
  obrigatório, equijoins e faixa em UNIQUE opcionais, política escolhida por
  teste).
- Ferramenta: Claude (Claude Code, modelo Opus 5.5). Trabalho no branch local
  `fix/auditoria-p01-p15`, um commit por item, sem push até esta entrada.
- O CBD-001 da main não é o P01 da auditoria: o CBD-001 corrigiu a ocupação
  da B+ no bulk load; o P01 é a memória auxiliar do `ScratchSpace`, que
  continuava sem correção.
- Decisões do usuário: M inclui as páginas de índice (P02); os equijoins
  opcionais já implementados ficam; o P02 vai num commit isolado, para poder
  ser revertido sozinho; os commits sobem um por vez, por autorização.
- Relação com a revisão de 06/11 (`revisao_interfaces_claude_cbd_2026-11-06.md`):
  ela foi tratada em 08/11 (decisões 157 a 169), exceto o P0-2 (cota única de
  M), aceito só em parte na época (159). A auditoria reencontrou o problema
  (P01, P02, P03), e ele foi resolvido agora (decisões 172 e 173). O P0-4 da
  revisão deixou o resíduo do P06; o teste de "acesso malformado" do P0-3
  continua pendente (issue #3, CBD-008).
- Issues do GitHub: #2, #4, #8, #10, #11, #12 e #13 têm os critérios de aceite
  atendidos; #5, #6, #7, #9, #14, #15, #16, #17 e #18 só em parte (ver "Não
  afirmado"); #1 (CBD-007) e #3 (CBD-008) não foram tratadas.

### Commits e evidências

1. `66205e3` (portabilidade): `test_bplus_bulk_load_integration.cpp` chamava
   `::mkdtemp` sem `<unistd.h>`. Os testes da main não compilavam no macOS; no
   glibc a declaração vem de `stdlib.h`, por isso o CI passava.
2. `2ff46c5` (P06): `BufferPool::fetch` passa a recusar bloco livre no bitmap.
   RED: `FetchDeBlocoLivreLanca` falhou na baseline com "allocate: bloco novo
   já estava no pool" (o bloco liberado tinha voltado ao pool); GREEN depois.
3. `f151672` (P07, P09): `convergencia.csv` registra repetições, dispersões e
   parada por convergência ou pelo teto; `caminhos.csv` registra o acesso
   executado. O `resultados.csv` não mudou, porque o CI o lê.
4. `ffb3411` (P01): o diretório do `ScratchSpace` (12 B por registro) passou a
   ser cobrado das páginas, com ordenação no lugar. No pior caso do E03 (2
   páginas, registros de um Int32), a capacidade caiu de 1.364 para 448
   registros, e `record_bytes() + aux_bytes() <= 8.192`. Na escala 1, sort-merge
   e hash mudaram até 1,3%; BNL e seleções, nada.
5. `41a402a` (P03): o diretório do BNL guarda só `RecordView` nas m − 1 − chunk
   molduras restantes. Com registros pequenos (teste `DiretorioDoBnlCabeEmM`),
   o chunk cai abaixo de 16; com M = 32, as tabelas da base mantêm 16.
6. `42146fa` (P04): seletividade baixa de 1%, com faixas de grupos inteiros de
   valores. Efetivas na escala 1: 1,00% (dre), 1,00% (cr), 1,03% (curso); 50%
   entre 49,84% e 50,00%.
7. `0827d0f` (P05, P15): as 8 células que faltavam no NATURAL JOIN. Cobertura
   na escala 1: 60 células obrigatórias, 54 executadas, 6 não aplicáveis
   (caso 3), nenhuma ausente. Inventário do CI de E/S de 108 para 168
   identidades, só acréscimos.
8. `bb6f5cc` (P08): premissas das estimativas. Aluno ⋈ Detalhes: hash de
   +257,4% para +1,7%, sort-merge de +60,6% para −5,1%; faixa de 50% na B+ de
   −32,2% para −0,2%; clustering sobre hash com 50% de −51,5% para −43,7%
   (colisões de buckets não modeladas). Não houve calibração.
9. `b9e1c1a` (P10, P12): clustering sobre hash contra oráculo (30 chaves em 3
   buckets, antes e depois de manutenção e reorganize) e E/S dos temporários
   dentro da medição com M = 4. Os dois passaram: não havia bug.
10. `b3826e6` e `97f4903` (documentação): resultados de exemplo, READMEs,
    handoff (decisões 173 a 175, blocos copiados dos headers reais, nota do
    LSan), catálogos de decisões e enunciado v2 no repositório.
11. `02bd998` (P02, decisão 172): as páginas de índice vêm da reserva durante
    a operação. RED intermediário: a primeira versão precisou de 3 molduras na
    B+ secundária com posting list em overflow, e
    `RidsEstaveisComDivisoesEReorganize` falhou com "todas as molduras da
    região estão fixadas"; a folha passou a ser solta durante a cadeia de
    overflow. Efeito na escala 1: B+ primária pontual de 1.045 para 3.041
    blocos, com a estimativa h + 1 passando de +187,1% para −1,3%; hash
    secundário pontual de 1.901 para 1.992; junções iguais.

### Erros da IA corrigidos durante o trabalho

- Erro de sintaxe ao editar `fmt_vezes` no `tools/tabelas.py` (aspas não
  fechadas), visto na compilação do script.
- Número errado na mensagem do commit do P08 (+0,3% em vez de +1,7%),
  corrigido com `--amend` antes de qualquer push.
- O primeiro teste do P12 usou `c_heap`, cujo grupo de duplicatas é inviável
  de propósito no sort-merge; foi trocado por A ⋈ B.
- O primeiro teste do P02 exigia leituras de índice por chave; numa B+ de
  altura 2 a raiz fica nas 2 molduras, e o certo era contar pedidos.
- A primeira tabela de cobertura mostrava uma coluna "pontual" nas faixas,
  que não tem sentido, e contava os papéis trocados como variante.
- O README afirmou que a configuração d custa o mesmo que a b em todos os
  tamanhos; isso só vale quando as duas têm a mesma organização (P×P e G×G).
- Uma tag local criada sem pedido foi removida antes de qualquer push.

### Validação executada

- macOS, Apple clang 21, `make` e CMake/Ninja (do CLion): 67 testes com o
  shim, também com ASan/UBSan (`detect_leaks=0`); 30 testes Python.
- Rodada integral (escala 1, LRU e MRU): 555 linhas conferidas com o SQLite
  sem o P02 e 534 com o P02, zero divergências. Smoke `--rapido --tuplas`:
  168 execuções, cobertura do CI completa.
- Comparação de E/S do CI rodada localmente contra `4091762`: 168 cenários
  pareados; 22 alertas sem o P02 (+0,2% a +1,1%, sort-merge e hash) e 29 com
  o P02 (mais 7 nas seleções por índice); nenhuma falha de protocolo.
- A árvore resultante de reverter o P02 é a do `97f4903`: compila e passa nos
  65 testes daquele ponto.

### Não afirmado

- GCC 13 e GoogleTest real não foram executados localmente; ficam para o CI
  de cada commit.
- O LeakSanitizer não foi validado.
- Critérios parciais das issues: #5 mede a memória auxiliar pela capacidade
  dos vetores, não por alocações instrumentadas; #6 não testa registros
  variáveis isoladamente; #7 não tem testes dirigidos de skew e recursão; #9
  não usa uma sequência calculada à mão; #14 é uma tabela, não um verificador
  que reprova, e não tem testes negativos; #15 não conta combinações
  ausentes no `compare.py`; #16 não tem teste determinístico de série que
  não converge; #17 não tem testes dos estimadores com valores manuais; #18
  tem manifesto com "b3826e6 + P02" e precisa ser regenerado com o SHA final.
- Os resultados desta entrada são locais; a validação hospedada de cada
  commit ainda não ocorreu.

## 2026-11-16 — Correções das issues abertas, um commit por issue, com o Claude

### Origem e escopo

- Ponto de partida: o branch `fix/auditoria-p01-p15` em `b60c03d`, já
  enviado. Nele, #2, #8, #10, #11, #12 e #13 têm os critérios de aceite
  atendidos. A #4, que a entrada anterior dava como atendida, fica aberta:
  a decisão 173 dizia que tudo o que ficava fora de M era O(m) e ocupava
  menos de 1 moldura, o que não vale para a lista de runs.
- Plano aprovado pelo usuário: um commit por issue, na ordem #1, #3, #5,
  #6, #7, #4, #9, #16, #17, #14, #15 e #18. Cada mensagem termina com
  `Closes #N`; a issue só fecha quando o PR entrar na `main`.
- Decisões do usuário: a lista de runs do sort-merge passa a ser cobrada
  dentro de M; o `sorted_slots` (cópia ordenada da folha corrente nas
  leituras da B+) fica fora de M, declarado como overhead limitado a uma
  folha.
- Ferramenta: Claude (Claude Code, modelo Opus 5.5). Sem push: os envios
  ficam com o usuário, um commit por vez.

### Commits e evidências

1. #1 (CBD-007), sort-merge com `group_pages = 0`. O plano usa grupo zero
   quando o campo da interna é declarado UNIQUE, mas, sem estrutura que o
   indexe, o UNIQUE não é verificado na inserção (157). No ramo sem grupo, o
   executor emitia os pares com o primeiro registro da interna e descartava
   as repetições. Agora ele confere a chave seguinte da interna e lança
   `std::logic_error` (não `DoesNotFit`, para não virar célula inviável).
   Custo: uma comparação em memória por registro da interna, sem E/S. O
   planejador continua confiando no UNIQUE declarado; exigir unicidade
   garantida por estrutura mudaria o plano das variantes heap. RED:
   `SortMergeRejeitaUniqueViolado` contra `b60c03d` devolveu 10 pares onde
   o certo seria 100 (ou erro); GREEN depois. NOT UNIQUE 10 × 10 dá 100 pares
   e o UNIQUE válido continua com grupo zero.
2. #3 (CBD-008), página com slots malformada. As funções `sp_*` usavam o
   `nslots` e os slots lidos do disco sem conferir os limites; com
   `nslots = 65535` numa página de 4.096 B, `sp_used_bytes` lia fora do
   bloco, e uma soma de registros inválida dava underflow em `sp_free_bytes`
   (o que deixaria o `sp_insert` gravar fora da página). Agora um cabeçalho
   validado (`kPageHeader + 4·nslots ≤ free_lo ≤ free_hi ≤ bloco`,
   `live ≤ nslots`) e cada slot vivo (dentro da área de registros) são
   conferidos antes de qualquer acesso, e a soma dos registros não pode
   passar do bloco. O erro é `std::out_of_range`, como o que o `sp_record`
   já usava. O `read_header` puro não muda, porque os nós internos da B+ têm
   outro layout e checagem própria. RED: `PaginaMalformadaFalhaSemAcessoInvalido`
   contra `5fdc364` com ASan parou em heap-buffer-overflow em `slot_at`
   (chamado por `sp_used_bytes`); GREEN depois, com 7 páginas malformadas.
   `CasosLimiteContinuamValidos` cobre diretório cheio, slot apagado e
   reaproveitado, registros de tamanhos diferentes e compactação. Fecha
   também o teste de acesso malformado do P0-3 da revisão de 06/11.
3. #5 (CBD-003), memória auxiliar do `ScratchSpace` medida por alocações
   reais. Novo executável `testes_memoria` (`tests/memoria/`), que troca o
   `operator new`/`delete` globais por versões que contam bytes vivos e pico
   num escopo armado; fica separado da suíte principal para não mudar o
   comportamento dela sob ASan. Registrado no CMake (GTest e shim) e no
   Makefile; o CI o roda pelo `ctest`. O `GTEST_OUTPUT` do workflow passou a
   apontar para um diretório (`evidencias/gtest/`), com um XML por executável;
   com um arquivo só, o segundo executável sobrescreveria o primeiro. O teste arma o contador só nas
   chamadas do operador (add, sort_by, build_hash, probe) e exige
   `bytes dos registros + pico do heap ≤ páginas × bloco`. Resultados no
   branch: E03 (Int32, 2 páginas) 448 registros, 2.688 B + 5.376 B de pico
   para 8.192 B; 1 página 1.344 + 2.688 para 4.096; registro de 204 B
   7.598 + 544 para 8.192; RIDs 1.840 + 1.408 para 4.096. Com chave VarChar,
   a comparação e a sonda decodificam a chave numa `std::string`
   temporária (no máximo três vivas): o teste admite essa folga O(1), que a
   decisão 173 passa a declarar. RED: o mesmo teste contra `4091762` (antes
   do P01) mediu, no E03, 1.364 registros e pico de 62.752 B de heap para
   8.192 B de orçamento, e todos os casos estouraram. Um erro do próprio
   teste: a primeira versão do autoteste do contador usava um vetor sem uso,
   cuja alocação o clang suprimiu; passou a chamar `::operator new` direto.
4. #6 (CBD-004), memória auxiliar do BNL, com M mínimo e registros densos
   e variáveis, no `testes_memoria`. O `bnl_join` roda direto no
   `StorageManager`, com o contador armado só nele, e o plano vem do
   `Monitor` com os mesmos dados. Resultados: o diretório do chunk cabe nas
   suas molduras em todos os casos (por exemplo, m = 32 com registros de
   8 B: 86.016 B em 21 molduras); o resto do heap é controle O(m), até
   4,3 KB com m = 32 (vetor das páginas do chunk, índice da política de
   substituição do subpool, uma entrada por moldura ocupada, e strings de
   chaves VarChar), e não cresce com os dados (4.180 B e 4.268 B com 12.000
   e 24.000 registros). Abaixo do M mínimo, o plano é inviável, como
   esperado. O teste revelou um defeito fora do que o plano previa para este
   commit: o plano estima os registros por página pela média (n / b), e com
   chaves VarChar de 1 a 24 caracteres, em m = 32, uma página acima da média
   enchia o diretório e o BNL parava com `DoesNotFit`, embora o plano
   dissesse que cabia. Correção local no `bnl_join`: quando o diretório
   enche, a rodada fecha ali, e o resto da página e as páginas seguintes,
   ainda fixadas, abrem a próxima. Sem estouro, as rodadas são as mesmas de
   antes (mesma E/S); com estouro, há uma passada a mais na interna, em vez
   de erro. Um erro do próprio teste: a primeira versão comparava 6.000 e
   12.000 registros, e o caso menor não enchia as 32 molduras, o que fazia o
   índice da política parecer crescer com os dados.
   Segundo defeito, achado ao montar a sequência manual da #9: desde o P03, o
   plano cobrava as m − 1 − chunk molduras restantes pelo diretório, mas
   nenhuma era tirada do subpool, que continuava usando-as como cache. Na
   prática, o operador tinha as m molduras para páginas mais o diretório. Agora
   o diretório ocupa ⌈registros do chunk × 24 B / bloco⌉ molduras, retidas
   durante a junção (`StorageManager::hold_frames`), e as demais ficam de cache
   para a interna, como pretendia o chunk de m/2. RED/GREEN em
   `BnlRetemAsMoldurasDoDiretorio` (M = 8, chunk 2, interna de 6 páginas, MRU):
   10 leituras sem retenção, 11 com ela, que é a conta à mão (4 + 6 + 1). Com
   LRU o caso não distingue: as páginas do novo chunk expulsam a interna, que
   inunda o cache nos dois casos.
5. #7 (CBD-005), memória auxiliar do sort-merge e do partition-hash. Testes
   no `testes_memoria` (sort externo com muitas runs, hash recursivo, skew,
   grupo de duplicatas maior que `group_pages`) mostraram três fontes de heap
   fora das molduras, todas corrigidas:
   - cada run ou partição guardava uma cópia própria do esquema (165): agora
     o operador faz uma cópia por entrada e a compartilha (`create_temp` com
     `shared_ptr`);
   - a camada 4 contava E/S por arquivo também para os temporários (dois
     mapas, cerca de 340 B por arquivo), contagem que nada lê: os temporários
     passam a ser contados só no total e por tipo, e a E/S não muda;
   - a tabela de arquivos guardava, para sempre, a entrada de cada
     temporário apagado, com o caminho completo: a memória crescia com o
     total de runs e partições criadas no processo. Agora o caminho não é
     guardado (sai de `dir / nome`), a entrada apagada libera nome e bitmap,
     e as entradas de temporários apagados são reaproveitadas. O detector de
     leitura sequencial esquece o arquivo apagado (o acesso seguinte, a
     outro arquivo, já não era sequencial). Com M = 4, o hash recursivo caiu
     de 304 KB para 11 KB de pico; 500 temporários criados e apagados deixam
     16 B.
   Decisão do usuário: a lista de runs do sort-merge é cobrada em M. O plano
   conta as runs das duas entradas e a saída de um passe, reserva
   `desc_frames = ⌈runs × sizeof(TempFile) / bloco⌉` molduras, tiradas do
   rascunho, do fan-in e da intercalação final, e repete até o número parar
   de subir. Essas molduras ficam retidas durante a junção (`hold_frames`,
   como as do diretório do BNL), e `make_runs` reserva o vetor com o número
   planejado e lança `DoesNotFit` se passar dele. Com 60.000 registros (cerca de 900 páginas):
   M = 32 dá 55 runs e 2 molduras para a lista; M = 16, 151 runs e 4
   molduras; M = 8 fica inviável (a lista não cabe). O mínimo do sort-merge
   com grupo de duplicatas passou de 4 para 5 molduras, e o teste
   `EsDosTemporariosFicaDentroDaMedicao` passou a usar M = 5. Segunda decisão
   do usuário, depois da medição: os descritores das partições do hash ficam
   fora de M, declarados. São até 2(m − 1) por nível e até 6 níveis, um
   limite que não depende de b. No pior caso da bancada (Inscrição como build,
   M = 32, dois níveis) o pico do heap foi de 44,7 KB, cerca de 20 KB deles
   em descritores de partição (perto de 5 molduras). No hash, as partições do
   primeiro nível passam a ser apagadas logo depois de usadas, como a recursão
   já fazia.
   Correção depois do 1º CI no Linux (commit seguinte, já com o 5 enviado):
   `TabelaDeArquivosReaproveitaTemporariosApagados` falhou no GCC/libstdc++
   com 104 B vivos (limite 64) e 1.243 B de pico (limite 1.024). Não era
   vazamento: a libstdc++ aloca o vetor de buckets da tabela de páginas do
   buffer pool na 1ª inserção, que caía dentro do escopo medido, e monta
   cada caminho com uma lista de componentes. O teste tinha limites
   absolutos que dependiam da biblioteca padrão; agora o aquecimento passa
   pelo `append`, e ele compara 50 e 500 temporários criados e apagados (o
   que fica vivo e o pico não podem crescer com o número deles). No macOS:
   0 B vivos e 440 B de pico nos dois casos.
6. #4 (CBD-002), contrato de M. Só documentação. A decisão 173 do handoff
   foi reescrita com o critério (§7.1 do enunciado): o que cresce com a
   entrada fica dentro de M, em molduras retidas da reserva (diretórios do
   ScratchSpace e do BNL, lista de runs); o que é limitado por uma página ou
   por m fica fora, com o tamanho medido pelo `testes_memoria` (descritores
   das partições do hash, `sorted_slots`, chaves VarChar temporárias, tabela
   de arquivos, controle O(m)). Também foram atualizados a §7 (orçamento do
   sort-merge com `desc_frames`), a §9 (a frase "menos de 1 moldura" não
   valia), os blocos copiados dos headers (`JoinPlan`, `StorageManager`,
   `FileManager`, `Counters`), o README (memória de trabalho), o
   `METODOS_DE_INTERFACE` (`create_temp`, `hold_frames`, `plan_join`) e o
   `docs/IO.md` (alertas por commit: 8 no CBD-004, todos de BNL com MRU, de
   +0,9% a +8,3%; 0 nos demais). A entrada anterior dava a #4 como atendida;
   isso valia para o código, não para o texto da 173.
7. #9 (CBD-010), fronteiras da medição. `Monitor::measured` passa a gravar,
   antes do 1º snapshot, o que o preparo deixou sujo, e guarda o número em
   `OpResult::prep_writes`, separado do delta (operação) e do `flush_writes`
   (verificação de fim). A bancada já grava e esvazia o pool antes de cada
   repetição, então `prep_writes` é 0 nela, e o CSV e a E/S não mudam. A
   bancada não confere o campo: a comparação de E/S do CI compila o driver
   do candidato contra a biblioteca da referência, que não o tem. Testes:
   `SequenciaManualDeEs`, a frio, com a E/S contada à mão (BNL de 3 × 2
   páginas com M = 4: chunk 2, 2 rodadas, 7 leituras e 7 faltas; sort-merge
   com M = 5 e interna UNIQUE: leituras e gravações de cada fase pela
   capacidade das runs e 19 registros por página), e
   `PreparoFlushEOperacaoSeparados`. RED: contra `12f861b`, com a carga
   suja, a operação contava 4 gravações (expulsões sujas das páginas da
   carga) e o flush de fim gravava 5; GREEN: tudo em `prep_writes`, delta e
   flush de fim zerados.
8. #16 (BENCH-007), convergência testável. O critério de parada saiu do
   `bench.cpp` para `src/layer1/convergencia.hpp` (`Convergence`,
   `repeat_until_stable`), sem mudar o comportamento: a bancada só adapta
   cada repetição para uma amostra (executou, blocos, tempo). A bancada inclui
   o header pelo próprio diretório (`"convergencia.hpp"`), porque a
   comparação de E/S do CI compila o driver do candidato com os headers da
   referência, que ainda não têm esse arquivo. Testes C++
   com séries determinísticas (`tests/test_convergencia.cpp`): série
   estável converge em 3; tempo alternando 10 e 20 para no teto de 10 com
   dispersão 0,6 e `parada = teto`; E/S variável também para no teto;
   aquecimento converge na 4ª; falha na 1ª dá `nao_executada`; teto igual ao
   mínimo não converge sem cumprir o critério. Teste Python
   (`test_tabelas_convergencia.py`): a tabela 15 lista só os grupos que
   pararam no teto e conta os que pararam só pelo tempo.
9. #17 (BENCH-008), parâmetros das estimativas. A bancada grava
   `parametros.csv`, uma linha por combinação, com os parâmetros que entram
   na fórmula: nas junções, os do mesmo plano que gera o `caminhos.csv` (b e
   d de cada lado, m, chunk e dir_frames; runs, fan-in, passes, group_pages
   e desc_frames; partições, níveis e build); nas seleções, n, b, bfr, lote,
   linhas esperadas, altura, distintos e cadeias. A tabela "Custo estimado ×
   medido" mostra os parâmetros da pior combinação de cada família. Testes
   (`tests/test_estimativas.cpp`): BNL, sort-merge com um passe e entrada
   ordenada (e com blocagem média), hash sem partição, com um e dois níveis e
   pré-particionado, B+ pontual h + 1 e faixa pela fração das folhas,
   clustering sobre hash, hashes, B+ secundária e varredura, cada um com o
   valor calculado no comentário. Os dois desvios grandes que restam (hash
   com Inscrição como build, +29%, pelo termo 4·n_h; clustering sobre hash
   com 50%, −44%, por colisões) continuam explicados na §4.5 do README.
10. #14 (BENCH-005), verificador da matriz obrigatória. O inventário e o
    estado das células saíram do `tabelas.py` para `tools/matriz.py`, usado
    pela tabela de cobertura (que saiu idêntica) e pelo novo
    `tools/verificar_matriz.py`. Ele termina com código 1 se faltar uma das
    60 células obrigatórias em LRU ou MRU; se uma junção obrigatória vier
    não aplicável, ou uma seleção obrigatória fora do caso 3; se o porte de
    uma junção executada não conferir com os blocos reais
    (`estatisticas.csv`) e o m da operação; ou se a rodada for reduzida
    (escala menor que 1, menos de 3 repetições, sem uma das políticas).
    Células inviáveis aparecem como aviso. Na rodada de exemplo: aprovada,
    com 108 células executadas, 12 não aplicáveis (caso 3) e porte conferido
    em 347 execuções; uma rodada `--rapido` é reprovada pela escala e pelas
    repetições. Dez testes Python, oito deles negativos
    (`test_verificar_matriz.py`).
11. #15 (BENCH-006), conferência com o SQLite sem ausências silenciosas. O
    `compare.py` passa a dizer quantas linhas e combinações conferiu e
    quantas estão ausentes, e reprova se houver alguma: operação declarada
    (`operacoes.csv` ou `referencia.csv`) sem linha no `resultados.csv`;
    linha medida sem referência; com `--tuplas`, linha medida sem o dump do
    motor (o nome segue o `shape_for` da bancada) ou dump sem a referência,
    caso em que antes o script parava com `FileNotFoundError`. A conta é
    relativa ao que a rodada declarou, e por isso vale para o smoke do CI; a
    completude da matriz fica com o verificador da #14. O cabeçalho do
    script e o README registram que o motor e o SQL mantêm as duas colunas
    da junção (`SELECT x.*, y.*`). Rodada de exemplo: 534 linhas em 168
    combinações, 0 ausentes, 0 divergências; smoke com `--tuplas`: 168 em
    168. Seis testes Python (`test_compare.py`).
12. #18 (BENCH-009), rodada definitiva. Feita com o código do commit
    `49be2d4`, numa cópia limpa (`git archive`), compilada com `make` e rodada
    sozinha na máquina: escala 1, LRU e MRU, 3 a 10 repetições, M = 32 e
    M = 2, semente 20261122; o manifesto grava o SHA completo, o compilador e
    as flags. Resultado: 563 linhas medidas em 168 combinações, 0 ausentes e
    0 divergências no SQLite; o `verificar_matriz.py` aprova (60 células por
    política, 54 executadas e 6 não aplicáveis pelo caso 3, porte conferido
    em 370 execuções). Reprodução num clone limpo do repositório, no mesmo
    commit: as 176 linhas da 1ª repetição bateram uma a uma (blocos, linhas,
    checksum); só o total de repetições mudou (560), porque o critério de
    parada inclui o tempo. Diferenças em relação à rodada de 13/11: 34 das 168
    combinações; o sort-merge de Aluno ⋈ Inscrição com heaps sobe de 10.483
    para 11.723 blocos (+11,8%), porque a lista de runs ocupa 3 molduras e a
    intercalação final, que aceita 29 runs, recebe 30 (Aluno passa a precisar
    de um passe); o mesmo com hash primário, de 11.662 para 12.902; no BNL
    com MRU, a economia sobre a LRU cai de 0,6–2,3% para 0,5–1,7% (molduras
    do diretório retidas); o resto varia menos de 0,3%. `resultados_exemplo/`
    foi regenerado (com `parametros.csv` e `verificacao_matriz.txt`), e os
    READMEs trazem os números novos e os comandos de reprodução. O projeto
    gera tabelas, não gráficos.

### Validação executada (16 e 17/11)

- Cada commit desta entrada foi verificado numa cópia limpa, como o CI faz:
  build Release e `ctest` (suíte principal e `testes_memoria`), build com
  ASan/UBSan e a suíte inteira, testes Python, smoke `--rapido --tuplas`
  conferido com o SQLite e com o verificador de cobertura do smoke, e a
  comparação de E/S contra o commit anterior. Alertas: 8 no CBD-004 (BNL
  com MRU, +0,9% a +8,3%), nenhum nos demais.
- RED/GREEN registrados nos commits 1, 2, 3, 4, 7.
- No push automático, o CI do commit 5 (Linux, GCC 13) reprovou um teste de
  memória com limite absoluto que dependia da biblioteca padrão. Como o 5
  já estava no remoto, a correção entrou num commit novo logo depois dele,
  e os commits 6 a 12, ainda locais, foram reaplicados por cima: o código
  deles é o que foi verificado, mais o teste corrigido. A rodada definitiva
  foi refeita com o SHA novo do código (`49be2d4`), e as contagens de blocos
  saíram iguais às da rodada anterior; só o tempo, as repetições e as
  medianas das seleções pontuais (cujo lote muda a cada repetição) mudaram.

### Não afirmado

- GCC 13 e GoogleTest real não foram executados localmente; ficam para o CI
  de cada push.
- O LeakSanitizer não foi validado (`detect_leaks=0`).
- O build CMake mostra um aviso pré-existente (`-Wmissing-field-initializers`
  em `test_layer3.cpp:201`), que o `make` suprime nos testes.
- No PR para a main (#19), a etapa "Medir e comparar I/O" falhou: num
  `pull_request`, a referência é a base do PR (`4091762`), e o driver do
  candidato é compilado contra a biblioteca dela, que não tem
  `JoinPlan::dir_frames`, `JoinPlan::desc_frames` nem `TableStats::bfr_medio`
  (usados pelo `parametros.csv` da #17). Nos pushes, a referência era o
  commit anterior, que já os tinha. Correção: as duas funções que gravam os
  parâmetros viraram templates e só usam esses campos se existirem
  (`if constexpr (requires ...)`). Localmente, a comparação contra a main
  passou: 168 cenários e 29 alertas (os já descritos no `docs/IO.md`).

## 2026-11-22 — Regressões locais de SlottedPage e prefetch do PR #19

- Trabalho delimitado aos dois casos solicitados pelo usuário, na branch
  `fix/pr19-slot-prefetch`, criada da base `f8177711ac3f6816f321948dff8bef798018b934`
  em worktree separado. A revisão ampla do PR continua pendente.
- Adicionadas regressões antes da implementação. O teste unitário da
  SlottedPage deriva do layout declarado em `page.hpp`, com região livre
  entre `free_lo` e `free_hi`. O teste de integração BufferPool/FileManager
  deriva do ciclo de alocação e cobre blocos nunca alocados e liberados,
  continuidade do prefetch após lacunas, cache e leituras esperadas.
- Na base com os novos testes, ambos falharam. O slot na região livre era
  aceito e operações posteriores alteravam a página. O prefetch lia oito
  blocos em cenários com um ou dois alocados e impedia a alocação seguinte.
- Correção mínima em `page.cpp` e `buffer_pool.cpp`: usar `free_hi` como
  limite inferior do registro e pular blocos livres no bitmap antes do
  prefetch. Preservadas as interfaces públicas.
- Depois da correção, as duas regressões e os 100 testes C++ da suíte
  Release passaram (89 principais e 11 de memória).
- Os mesmos 100 testes passaram em Debug com ASan/UBSan (199,80 s), com
  LeakSanitizer desativado. Os avisos preexistentes de compilação dos testes
  constam dos logs. O usuário autorizou em seguida o stageamento e o commit
  local das duas correções com testes e evidências.
- Logs, patch dos testes e comandos de reprodução estão em
  `evidencias/PR-19/2026-11-22-slot-prefetch/`.

## 2026-11-22 — Continuação da revisão do PR #19, sort-merge variável

- Após o commit local `2f0148c`, o usuário autorizou seguir ao próximo
  item da revisão. Reproduzida a regressão de viabilidade do sort-merge
  contra a base `4091762`, com programa limitado a esse cenário.
- Duas entradas de 1.000 chaves únicas e VarChar com conteúdo `x` têm
  registros efetivos de 7 bytes. A base produz 1.000 pares com M=5 ou 16.
  No candidato, máximo declarado de 1.000 bytes causa recusa com M=5;
  máximo de 4.050 causa recusa com M=5 e excesso de runs previstas com M=16.
- Um plano testemunha executado com a biblioteca atual, reserva M=5 e
  descritores cobrados nessa reserva produz os 1.000 pares em todos os
  três máximos testados (24, 1.000 e 4.050), com checksum igual ao da base.
  Isso demonstra a viabilidade dentro da reserva de páginas. Pico total de
  heap não foi medido nesta reprodução.
- Causa localizada no planejamento de `monitor.cpp`: capacidade baseada
  no máximo do esquema, realimentando a reserva de descritores. Quando a
  capacidade fica zero, `ceil_div` devolve zero e o plano assume uma run,
  cujo limite é imposto por `make_runs` na execução.
- Evidências em `evidencias/PR-19/2026-11-22-sort-merge-variavel/`, com
  fonte do teste de integração, logs e comandos. A correção desse item
  permanece pendente; o código de produção não foi modificado nesta etapa.

### Correção local após autorização para continuar

- Adicionadas três regressões permanentes de integração, com resultado
  derivado das chaves de entrada. Todas falharam antes da alteração.
  Cobrem VarChar curto, manutenção com tamanhos mistos e capacidade zero.
- A camada 3 passa a manter o maior tamanho codificado observado por tabela,
  coletado nos fluxos já existentes. Insert atualiza esse limite e ambas
  as reorganizações o recalculam; remoções mantêm um limite superior seguro.
  O campo novo de TableStats e os consumidores foram revisados. A API
  anterior de capacity_for permanece disponível.
- O sort-merge usa o limite observado para a capacidade das runs e rejeita
  capacidade zero explicitamente. Continua cobrando a lista de descritores
  em M. O planejamento permanece conservador diante de tamanhos mistos.
- Os três testes passaram depois. Suíte de 103 testes C++ aprovada em
  Release e ASan/UBSan (-O1, 57,73 s, LeakSanitizer desativado). O reproducer
  original retornou os 1.000 pares corretos nos seis casos.
- Perfil de I/O existente: 168 cenários, ambos os lados conferidos com
  SQLite e tuplas, zero alterações de leituras, gravações ou flush frente
  a `2f0148c`. A primeira comparação foi recusada por caminhos diferentes
  de compilador nos metadados; repetida com `/usr/bin/c++` nos dois lados.
- A correção ainda é local e sem commit. As duas medições identificam o
  mesmo SHA base, com hashes distintos das árvores em seus metadados e o
  diff preservado em correcao.patch. Logs e comandos estão no diretório
  de evidências deste item. Revisão adicional do diff sem defeitos apontados.

## 2026-11-22 — Sort-merge com cursores clustering/hash dentro de M

- O usuário autorizou continuar. O próximo caso revisado mantém duas
  páginas por fonte clustering sobre hash (índice e dados), enquanto o
  plano contava uma. Reproduzida exceção de molduras fixadas com duas fontes
  UNIQUE e M=3, e com duplicatas, grupo de uma página e M=4.
- Acrescentados dois testes de integração. A versão completa dos testes
  falha na biblioteca anterior e passa após a alteração. Cobre mínimos de
  quatro/cinco molduras, clustering sobre B+, fontes mistas e passe extra
  para materialização de heap. Os resultados esperados vêm das chaves.
- JoinPlan passa a expor ordered_extra_frames, debitado apenas na fase
  final, inclusive em frames_used, verificação de viabilidade e fan-in.
  O driver registra esse parâmetro e final_fan_in, usando requires para
  compilar também contra os headers anteriores. Consumidores e duração
  dos cursores foram revistos no escopo desta mudança.
- Release: 105 testes C++ aprovados. Perfil de I/O: 168 cenários, zero
  alterações em leituras, gravações ou flush; SQLite com tuplas sem
  divergências nos dois lados. A referência inclui a correção anterior de
  VarChar, permitindo atribuir a comparação somente a este ajuste.
- Evidências em `evidencias/PR-19/2026-11-22-sort-merge-clustering/`.
  Os 105 testes passaram também com ASan/UBSan (-O1, 52,79 s,
  LeakSanitizer desativado). Revisão adicional do ajuste sem defeitos apontados.
  Os patches continuam locais e sem novo commit. As medições guardam o SHA
  base e hashes das fontes para identificar as duas árvores modificadas.

## 2026-11-22 — BNL com página densa e limite exato do diretório

- Continuação autorizada pelo usuário. Reproduzidos o plano inviável com
  400 Int32 e M=3 e a varredura interna vazia após 170 registros externos
  preencherem exatamente o diretório. Os novos testes falharam antes.
- O plano passa a limitar o diretório às molduras disponíveis ao chegar a
  chunk=1. O executor já retém a página parcial entre rodadas. Agora pula
  rodadas de diretório vazio. Conservados os limites de molduras e vetores.
- Testes de integração em LRU/MRU e limites 169/170/171, com oráculo de
  chaves e requisições calculadas à mão. Na fronteira de 170, leituras
  caíram de cinco para três; 171 antes era inviável e agora produz 171 pares
  com cinco leituras. O teste de alocações de memória, atualizado para M=3,
  passou para registros densos e variáveis dentro do diretório e controle
  declarado. As molduras continuam retidas.
- A revisão identificou que a fórmula ainda ignorava o limite do diretório.
  Novo RED comprovou a previsão de três contra cinco no caso de 171.
  Corrigida a premissa usando também ceil(n/dir_entries), com teste unitário
  calculado à mão. O modelo permanece aproximado para arredondamentos por
  página/chunk e cache, com exemplo e limites documentados na evidência.
- Release: 107 testes C++ aprovados. Perfil de I/O existente: 168 cenários
  sem alterações em leituras, gravações e flush; SQLite com tuplas sem
  divergências dos dois lados. Comparação contra a árvore com as duas
  correções anteriores, isolando o efeito deste ajuste.
- Fontes anteriores, diff, logs e comandos estão em
  `evidencias/PR-19/2026-11-22-bnl/`. Alterações locais, sem novo commit.
- ASan/UBSan: os mesmos 107 testes aprovados após o ajuste da fórmula,
  em 54,14 s (-O1, LeakSanitizer desativado). Revisão adicional confirmou
  o ajuste e explicitou a limitação de arredondamentos do modelo.

### Consolidação local das três correções

- O usuário autorizou consolidar VarChar, clustering/hash e BNL em commit
  local antes de continuar a matriz. Conferidos diff, escopo e hashes da
  última evidência. Reexecutados 107 testes C++ e 48 Python, todos aprovados.

## 2026-11-22 — Matriz física e conferência do PR #19

- As três correções anteriores foram consolidadas no commit local
  `b1c0de5`, autorizado pelo usuário. A continuação tratou da matriz e dos
  validadores, seguindo os §§5.1 e 5.2 do PDF revisado.
- Configurações sem índice usavam Scan em heaps com índices no campo do
  predicado. Criadas quatro variantes heap sem índices. A bancada publica
  metodos.csv a partir de access_paths e configuracoes.csv com a ordem real
  dos operandos e campos. O verificador cruza os metadados; não infere
  ausência de índice a partir da escolha de Scan.
- Novos testes negativos falharam no validador anterior. Agora são
  conferidas repetições efetivas e configuração física, além de motivos de
  inviabilidade. Uma rodada vazia de sucessos não é aprovada como matriz.
- O perfil de I/O foi versionado como v2, com 168 identidades renomeadas
  para os heaps sem índices. O v1 foi preservado e comparações cruzadas são
  recusadas. Os métodos/configurações também entram nos hashes do perfil.
- O conferidor passa a reprovar saída vazia, ausência total de medições e
  combinações declaradas ausentes. Falhas em tuplas retiram a combinação da
  contagem aprovada. O filtro --so é aplicado antes dos SQLs/lotes; filtro
  sem correspondência retorna 2. Testes unitários e de integração tiveram
  falhas antes e sucesso depois.
- Validação final: 107 testes C++ Release e ASan/UBSan (64,39 s), 63 Python
  com BUILD_DIR habilitando integração real da CLI. LeakSanitizer desligado.
  Revisões adicionais dos dois diffs sem bloqueadores. Resultados antigos
  sem inventário continuam conferíveis; sua completude é limitada ao que
  os arquivos históricos declaravam.
- Rodada integral: 1.159 linhas, 168 combinações, zero ausências e
  divergências SQLite. Matriz: 108 células executadas e 12 não aplicáveis,
  60 por política, portes e configuração física aprovados. A rodada
  reduzida foi conferida com tuplas e corretamente recusada como prova
  integral por escala/repetições. Convergência temporal no teto é declarada.
- A rodada integral antecedeu apenas o ajuste de --so, sem impacto na
  execução padrão; seu fonte está preservado junto dos CSVs. O manifesto
  informa o SHA base porque o driver foi modificado localmente.
- I/O v2: 168 cenários com driver comum e zero mudanças de contadores. As
  configurações históricas v1 e as novas v2 não são intercambiáveis.
- Evidências em `2026-11-22-matriz-fisica/` e `2026-11-22-conferencia/`,
  sob evidencias/PR-19. Parecer local em docs/REVISAO_PR_19.md. Gráficos e
  tamanho de bloco no manifesto da #18 permanecem pendentes. A main e o
  remoto continuam sem as alterações desta revisão; os ajustes novos
  permanecem sem commit.

### Consolidação e publicação autorizadas

- O usuário autorizou o commit dos ajustes de matriz/validadores, envio da
  branch e acompanhamento do Actions para incorporação ao PR #19.
- Conferidos novamente diff e estado do worktree; reexecutados 107 testes
  C++ e 63 Python, todos aprovados. Preservadas as evidências anteriores
  de ASan/UBSan, SQLite integral e comparação de I/O.
- A tentativa de consultar o PR pela API falhou por conectividade;
  git ls-remote falhou com `Could not resolve hostname github.com`.
  O commit local pode ser consolidado. Envio, Actions e atualização da
  branch remota do PR continuam pendentes de uma conexão funcional.

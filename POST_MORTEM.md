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

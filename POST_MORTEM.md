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

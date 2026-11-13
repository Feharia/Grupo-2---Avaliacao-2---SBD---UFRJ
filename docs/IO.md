# Suíte pequena para regressão de I/O

O perfil `io-rapido-v1` compara **108 execuções de consultas**, incluindo
seleções em heap/B+/hash e índices secundários, BNL, hash join, sort-merge,
LRU e MRU. Reutiliza a escala reduzida da bancada existente. Não substitui
a matriz experimental integral exigida no trabalho.

## Protocolo

- Escala 0,1; semente 20261122; uma repetição; lote de 100 chaves;
  M=32 nos joins e M=2 nas seleções; políticas LRU e MRU.
- Mesmo fonte de `src/layer1/bench.cpp` do candidato compilado contra cada
  `libcbd_motor.a`, usando os **headers da revisão medida**. Assim fill=0,7,
  escolha de entradas, preparo dos dados e invalidação do cache vêm do mesmo
  driver. Não se usa o driver histórico da referência. Uma mudança futura
  nesse protocolo exige revisão explícita e, se necessário, nova versão.
- Defaults de `Config` medidos por `tools/config_io.cpp`, compilado contra
  cada revisão: blocos de 4096 B, extent de 8 blocos, áreas 32/16/4/32,
  prefetch e toss-immediate desativados no perfil inicial. Diferenças invalidam
  a comparação, mesmo que os parâmetros de linha de comando sejam iguais.
- Builds Release com o mesmo compilador. As flags reais da biblioteca são
  extraídas de `compile_commands.json`, normalizando apenas caminhos de
  fontes e saída. As flags e a versão do compilador precisam coincidir.
- SHA256 de dados, esquema, consultas, constantes e lotes, além dos hashes
  de driver/ferramentas, precisam coincidir. O inventário
  `tools/io_cenarios_v1.json` fixa as 108 identidades, incluindo variantes;
  remover uma delas dos dois lados também falha.
- Ambos os lados passam pela referência SQLite do candidato, comparação de
  contagens/checksums e multiconjuntos de tuplas e verificação de cobertura.
  Só depois é criado `medicao.json`. O CSV medido também tem SHA256.

O inventário inicial foi escolhido a partir da rodada reduzida existente e
conferida com SQLite. Ele define **quais casos medir**; não fornece respostas
esperadas para o motor. As respostas continuam sendo obtidas independentemente
por SQL. Os testes unitários do comparador usam números e cenários sintéticos.
As execuções reais da bancada são testes de integração das quatro camadas
com oráculo SQLite. Há também testes de integração da CLI para rejeição de
saída existente e geração de relatório com alerta.

Os contadores representam transferências de blocos arquivo↔buffer, não acesso
físico ao SSD. A preparação faz flush e invalida o buffer antes de cada
consulta. `OpResult.delta` termina antes do flush final: o relatório separa
leituras, escritas, total, `flush_writes` e total incluindo esse flush.
**Carga e manutenção não são medidas por esta primeira versão.** Alterações
nas fronteiras do Monitor ou na semântica de Counters requerem revisão;
assinaturas não provam que sua interpretação permaneceu correta.

## Resultados e política de alertas

O comparador produz `comparacao.csv`, `comparacao.json` e `resumo.md`, com
referência/candidato identificados por SHA, parâmetros, assinaturas e deltas
absolutos/percentuais por cenário. Contadores e checksums usam inteiros exatos,
inclusive acima de 2^53. Percentual com base zero é indefinido quando há
incremento; não se inventa um percentual finito.

Qualquer aumento de leituras, escritas ou flush gera um **alerta**, mesmo se
outra métrica cair. O workflow mostra o resumo na página da execução e uma
anotação de warning. Aumento não bloqueia o merge por si só. Incompatibilidade
de protocolo, cenário ausente, mudança de status, resultado errado ou falha
de ferramenta interrompe o job: não se publica um falso resultado favorável.

Se uma revisão mudar dados, consultas ou inventário intencionalmente, ajustar
o perfil e executar novamente os dois motores sob o mesmo contrato. Se a
interface pública antiga não compilar com o driver comum, a comparação falha;
será necessário um adaptador explícito ou uma nova referência aprovada.

## Execução local

A partir de `PrototipoIA/Implementacao/`, com fonte de referência disponível em
outro checkout e SHA completo de cada revisão:

```bash
io_local=$(mktemp -d /tmp/cbd-io-local-XXXXXX)
python3 -I tools/medir_io.py --source /caminho/referencia/PrototipoIA/Implementacao \
  --commit SHA_COMPLETO_REFERENCIA --saida "$io_local/referencia"
python3 -I tools/medir_io.py --source "$PWD" \
  --commit SHA_COMPLETO_CANDIDATO --saida "$io_local/candidato"
python3 -I tools/comparar_io.py "$io_local/referencia" "$io_local/candidato" \
  "$io_local/comparacao"
```

Os placeholders SHA devem ser substituídos por hashes completos. Para
reaproveitar uma biblioteca já compilada da **mesma revisão**, acrescentar
`--build-dir /caminho/build-release` à medição correspondente. É necessário
`libcbd_motor.a` e `compile_commands.json`; o runner compila o driver comum.
Saídas existentes são rejeitadas para preservar evidências. Alterações locais
ainda sem commit são identificadas também pelos hashes de fontes/ferramentas
em `medicao.json`; o SHA informado sozinho não descreve uma árvore suja.

Repetir uma medição em outra saída permite conferir determinismo dos
contadores. Este perfil mede contagens, não distribuições de latência.

## Integração no GitHub

No job `verificar (normal)`, a biblioteca candidata já compilada é reutilizada;
só a biblioteca de referência exige outro build. O probe e o driver comum são
compilados para ambos. Não há outro job de VM nem execução duplicada no ASan.

| Evento | Referência | Candidato |
|---|---|---|
| pull request | SHA base do evento | SHA do merge testado pelo Actions |
| push | `before` | SHA enviado |
| criação de branch | pai do candidato | SHA enviado |
| manual | `baseline_ref`, se informado; senão pai | SHA escolhido ao disparar |

No primeiro commit, sem pai, a referência é o próprio candidato e há um aviso
de comparação de controle. O SHA resolvido do checkout de referência é gravado;
um nome de branch não substitui a identificação da versão medida.

Os artifacts mantêm CSVs, metadados, manifestos, logs e relatórios por 14 dias.
Os dumps adicionais da suíte e builds não são enviados; a verificação de
correção anterior do workflow mantém seus dumps usuais. Evidências locais
selecionadas desta entrega ficam em `evidencias/IO/2026-11-13/`.

O workflow continua sendo CI de build/testes/medições; não há etapa de deploy.

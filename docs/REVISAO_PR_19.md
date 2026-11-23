# Parecer local sobre o PR #19

PR examinado no SHA `f8177711ac3f6816f321948dff8bef798018b934`, base
`4091762d65a52495c02c9cf63da5bc630998ff4d`. Foram lidos os cinco prints,
o PDF e as duas exportações de chat de wpp-msgs. Seus relatos serviram como
guia para conferir fontes, requisitos e testes.

As correções foram desenvolvidas em `fix/pr19-slot-prefetch`, no worktree
`pr19-slot-prefetch`, e publicadas na origem do PR em `a6b3d48`. O usuário
forneceu a confirmação de três execuções Actions aprovadas para esse HEAD.
A main permanece em `4091762`. As numerações CBD variam entre pacotes de
contexto; os números de issue abaixo são os do GitHub.

## Resultado da revisão e ajustes

| Área | Resultado e evidência |
|---|---|
| SlottedPage e prefetch (#2/#3) | Dois casos residuais reproduzidos, regressões antes/depois e correções no commit local `2f0148c`. |
| Sort-merge variável (#7) | Lista de runs dimensionada pelo máximo declarado causava inviabilidade indevida. Corrigido com maior registro observado, preservando cobrança em M. |
| Fontes clustering/hash (#4/#7) | Cursor retém índice e dados; conta final corrigida, inclusive com duplicatas e passes adicionais. |
| BNL (#6) | Página densa com M=3 agora funciona; rodada vazia eliminada; leituras dirigidas de 5 para 3. Estimativa passou a considerar limite do diretório. |
| Matriz física (#11–#14) | Heaps sem índices separados dos indexados. Validador cruza campos e métodos reais, verifica repetições e recusa sucesso vazio. Rodada integral aprovada. |
| Conferidor (#15) | Rejeita ausência total de medições e combinações declaradas ausentes. Contagem de tuplas corrigida; --so conferido com SQLite. |
| Estimativas e convergência (#16/#17) | Parâmetros e convergência ficam registrados. Aproximações de cache, distribuição e fronteiras de páginas permanecem explicitadas. |

As três correções de operadores foram consolidadas em `b1c0de5`, depois
de 107 testes C++ e 48 Python aprovados. A matriz e os verificadores foram
corrigidos na continuação da revisão, com as evidências descritas abaixo.

## Validação final disponível

- 107 testes C++ aprovados em Release e ASan/UBSan.
- 63 testes Python aprovados, com integração da CLI e SQLite habilitada.
- Rodada integral na escala 1, LRU/MRU e 3 a 10 repetições: 1.159 linhas em
  168 combinações conferidas com SQLite, sem divergências ou ausências.
- Matriz com 60 células por política, 108 executadas e 12 não aplicáveis
  pelo caso 3, sem inviáveis ou ausentes. Portes e configuração física
  conferidos. Rodada reduzida também conferida com tuplas completas.
- Perfil de I/O v2: 168 cenários pareados sob o driver comum e zero
  mudanças de leituras, gravações ou flush entre os motores comparados.

## Limites e recomendação

O PR incorporou os ajustes funcionais e de validação em `a6b3d48`, com
sucesso no CI informado pelo usuário. Os registros dessa publicação e
da comparação de I/O contra a main estão em
`evidencias/PR-19/2026-11-22-io-main/`. A consolidação posterior dos registros
ainda precisa ser enviada e conferida no CI. Este parecer não declara
todas as issues encerradas por um único resultado positivo da suíte.

O contrato de memória usa as exclusões documentadas na decisão 173. Os
testes de alocações cobrem seus casos declarados; não medem todo o RSS do
processo. LeakSanitizer permaneceu desativado. As estimativas continuam
modelos aproximados e casos de não convergência temporal aparecem no CSV.

O material do WhatsApp deixava a #18 fora do fechamento. O commit
`dc1b1ea` contém `Closes #18`, mas gráficos e tamanho do bloco no manifesto
continuam pendentes do escopo relatado pelo colega. A rodada corrigida
preserva CSVs e tabelas, e essa pendência deve permanecer visível no
acompanhamento da issue.

A preparação posterior da integração está em `docs/INTEGRACAO_PR_19.md`.
A instrução de fechamento está em uma mensagem histórica de commit;
preservando esse histórico, o merge previsto deve ser por squash com
mensagem final revisada para manter a #18 pendente. Editar apenas o texto
do PR não neutraliza a mensagem do commit histórico.

Os resultados de exemplo de 17/11 foram preservados como histórico. As
evidências atualizadas, fontes, patches e comandos estão nas pastas
`evidencias/PR-19/2026-11-22-*`, com registros no POST_MORTEM.md.

# Estado final do congelamento do programa

Versão publicada: **`entrega-programa-2026-11-27`**, commit
**`d95acc612b7885e6d35839f15c227c1cd527fca0`**.
A [confirmação da tag](2026-11-27-entrega/publicacao/cbd-tag-entrega-Qc3oaS/publicacao.json)
registra a publicação e o SHA verificado no remoto. O programa está congelado;
a documentação desta branch preserva as evidências sem mover a tag.

| Evidência | Arquivo |
|---|---|
| Aceitação no SHA final | [aceitacao.json](2026-11-27-entrega/aceitacao.json) |
| Parâmetros efetivos e SHA | [manifesto.txt](2026-11-27-entrega/full/manifesto.txt) |
| 697 medições em 168 combinações | [resultados.csv](2026-11-27-entrega/full/resultados.csv) |
| Referência SQLite | [referencia.csv](2026-11-27-entrega/full/referencia.csv) |
| Conferência: zero ausências/divergências | [compare.log](2026-11-27-entrega/compare.log) |
| Matriz: 108 executadas, 12 N/A, nenhuma ausente/inviável | [matrix.log](2026-11-27-entrega/matrix.log) |
| Convergência, incluindo grupos não executados | [convergencia.csv](2026-11-27-entrega/full/convergencia.csv) |
| Tabelas da rodada | [tabelas.md](2026-11-27-entrega/full/tabelas.md) |
| Dados e lotes realmente usados | [entradas-oficiais.tar.gz](2026-11-27-entrega/entradas-oficiais.tar.gz) |
| Integridade dos artefatos de aceitação | [ENTREGA.sha256](2026-11-27-entrega/ENTREGA.sha256) |
| CI da main no SHA final | [integracao-ci-main.json](2026-11-27-entrega/integracao-ci-main.json) |
| Revisão local do PR #21 | [README da revisão](../PR-21/2026-11-27-revisao/README.md) |

A E/S convergiu nos 168 grupos executados; sete sem convergência temporal.
As outras oito linhas de convergência têm `parada=nao_executada`. O README
[datado da execução](2026-11-27-entrega/README.md) registra os comandos,
limites e a correção do verificador auxiliar, sem alteração no programa.
Sua indicação de tag pendente corresponde ao momento anterior à publicação;
a confirmação final está vinculada acima. Os artefatos datados e seus hashes
foram copiados sem modificações.

## Histórico preservado

- [Aceitação anterior de 23/11](2026-11-23-aceitacao/README.md): execução
  anterior ao manifesto físico, com suas próprias contagens e SHA.
- [Publicação de 23/11](2026-11-23-publicacao/publicacao.json): integração
  do PR #20, anterior ao PR #21 e à tag final.
- [Programa de entrega](../../docs/ENTREGA_PROGRAMA.md): versão, checkout e
  limites da aceitação. [Post-mortem](../../POST_MORTEM.md): histórico das etapas.

Caminhos absolutos e diretórios `/tmp` nos logs representam a execução
original. Os dados/lotes oficiais foram arquivados nesta branch; os binários
e arquivos do motor não integram o Git. Nenhuma referência histórica foi
relabelada como execução do SHA final.

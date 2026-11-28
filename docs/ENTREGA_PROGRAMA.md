# Versão de entrega do programa

A versão congelada é a tag anotada **`entrega-programa-2026-11-27`**, publicada
no commit **`d95acc612b7885e6d35839f15c227c1cd527fca0`**. O objeto da tag é
`51ce612a7bbfe53e7c94e9852476c6b0470a8ba9`. O PR #21 foi integrado por squash,
e o [CI da main](https://github.com/Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ/actions/runs/46085054328)
aprovou a versão integrada, incluindo testes normais e ASan/UBSan.

## Código e execução

O código continua em `PrototipoIA/Implementacao/`, com as quatro camadas,
C++20 e ambiente oficial Linux/WSL2. Para obter um checkout separado da versão:

```bash
git fetch origin tag entrega-programa-2026-11-27
git worktree add --detach ../entrega-programa-2026-11-27 entrega-programa-2026-11-27
```

Use um destino novo ao executar esse comando. Nesta consolidação já foi
criado o worktree `entrega-programa-2026-11-27` ao lado dos checkouts de
desenvolvimento. Os worktrees anteriores e suas alterações locais foram
preservados. A tag identifica o código mesmo que a main receba documentos
ou outras mudanças posteriores; os comandos de build estão no
[README da implementação](../PrototipoIA/Implementacao/README.md).

## Aceitação e resultados para os relatórios

A [rodada oficial](../evidencias/CONGELAMENTO/README.md) foi gerada em cópia
limpa do SHA congelado, com build Release, escala 1, LRU/MRU, M=32/2,
lote 1000, semente 20261122, 3..10 repetições e tolerância 0,05.
O manifesto identifica o commit e os parâmetros físicos: bloco 4096 B,
extent 8, áreas 32/16/4/32, prefetch e toss-immediate desligados.

- 697 linhas medidas em 168 combinações; zero ausências e divergências
  de contagem/checksum contra SQLite.
- Matriz obrigatória: 108 células executadas, 12 não aplicáveis, nenhuma
  inviável ou ausente; portes conferidos em 421 execuções de junção.
- E/S convergiu em todos os grupos medidos; sete não convergiram em tempo,
  métrica complementar. Nenhum flush final não zero ou erro de manutenção.
- O perfil reduzido, com tuplas completas, foi validado na revisão e no CI.
  A revisão local teve 114 testes C++, 69 Python e 168 cenários de I/O sem
  deltas contra a versão anterior ao PR #21.

Use os CSVs dessa rodada para os relatórios e gráficos. `resultados_exemplo/`
e as análises de 17/11 permanecem como histórico; não foram sobrescritos.
LeakSanitizer segue desativado, conforme limitação registrada. A aceitação
não constitui prova de ausência de todo defeito possível.

## Registros posteriores ao congelamento

Esta consolidação publica documentação e evidências. A árvore de
`PrototipoIA/Implementacao/` é idêntica à da tag; os commits dos registros
podem ter outro SHA sem mudar o programa aceito ou exigir nova tag.
Os READMEs e logs datados nas evidências preservam o estado da etapa em que
foram produzidos, inclusive indicações de publicação então pendente. O
[índice atual](../evidencias/CONGELAMENTO/README.md) aponta a confirmação final.

Relatórios, gráficos e atualização administrativa de issues são atividades
posteriores. A #18 estava aberta ao concluir a integração do PR #21; os
gráficos ficaram fora dessa alteração. Esta consolidação não fecha issues.

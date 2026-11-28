# Rodada oficial para entrega — 27/11/2026

SHA integrado: `d95acc612b7885e6d35839f15c227c1cd527fca0`.
PR #21, squash; árvore idêntica ao head revisado a502a5f.
CI da main aprovado no run 46085054328 (normal e ASan/UBSan), conforme
`integracao-ci-main.json`. O CI usa LeakSanitizer desativado, limite já
registrado. Não repetimos aqui as suítes que acabaram de passar no CI.

Build Release do zero em worktree Git separado e limpo, com GCC 13.3,
GoogleTest obrigatório encontrado, sem avisos de compilação do alvo bancada.
`CBD_COMMIT` identifica o SHA integrado, sem relabelar resultados antigos.

## Aceitação observada

- Escala 1, LRU/MRU, seed 20261122, M=32/2, lote 1000, 3..10 repetições,
  tolerância 0,05; manifesto registra bloco 4096 B, extent 8, áreas
  32/16/4/32 e prefetch/toss-immediate desligados.
- 697 linhas medidas em 168 combinações. SQLite (contagem/checksum):
  zero ausências e divergências. Dumps de tuplas completos não foram
  gerados na integral; o CI e a revisão anterior conferiram as tuplas
  completas no perfil reduzido, também de 168 combinações.
- Matriz obrigatória aprovada: 108 células executadas e 12 não aplicáveis,
  zero inviáveis ou ausentes. Portes conferidos em 421 execuções de junção.
- E/S convergiu nos 168 grupos medidos; sete sem convergência temporal.
  Nenhum flush final não zero ou falha de verificação pós-manutenção.
- Duração informativa da bancada: 91,2 s. Não é comparação controlada de
  tempo com rodadas anteriores. Mudanças no número de repetições adaptativas
  explicam contagens de linhas diferentes entre rodadas.
- 16 tabelas geradas com o novo manifesto. Código do checkout permaneceu
  limpo. Algoritmos e implementação não foram modificados nesta execução.

## Ocorrência no verificador auxiliar

A primeira execução do script chegou ao fim de build/bancada/SQLite/matriz/
tabelas com sucesso, mas uma asserção auxiliar assumia que convergencia.csv
teria somente 168 linhas. Há também oito grupos legitimamente não executados
(`parada=nao_executada`), totalizando 176 linhas. Corrigimos exclusivamente a
seleção dos grupos de convergência pelas chaves realmente medidas, exigindo
168 grupos correspondentes e classificando as oito linhas adicionais.
O script anterior e o traceback foram preservados. `finalize.py` conferiu
os produtos existentes, sem repetir benchmark ou alterar o programa.
Essa ocorrência não foi falha funcional da bancada.

## Reproduzir e preservar

`run_official.py` contém os comandos exatos de build e execução e a versão
corrigida da validação. Usa diretórios novos; sua raiz deriva da localização
do arquivo. Para repetir, preparar outra raiz com checkout limpo `source/`
no SHA acima e ajustar somente o prefixo local do GoogleTest se necessário.
Os logs preservam os comandos realmente executados nesta rodada.

`full/` contém CSVs, manifesto, conferência e tabelas. O arquivo
`entradas-oficiais.tar.gz` preserva os dados e lotes realmente usados.
Arquivos do motor e executável permanecem no diretório de `workdir.txt`.
`aceitacao.json` registra hashes das fontes e do binário. `ENTREGA.sha256`
permite conferir os arquivos desta aceitação antes de publicar a tag.

A tag prevista é `entrega-programa-2026-11-27`, no SHA integrado. O script
`publicar-tag-entrega.sh` confere aceitação, hashes, main remota e CI antes
de criar uma tag anotada e enviar exclusivamente essa referência. Não cria
commits de registros nem envia branches. Repetição não sobrescreve tags.
Sintaxe e publicação/repetição em remoto Git local foram validadas; CI
simulado reprovado foi recusado. Esses testes não publicaram tag no projeto.
A publicação real da tag continua pendente do terminal do usuário, pois
este ambiente não acessa o GitHub. Os registros locais não foram commitados.

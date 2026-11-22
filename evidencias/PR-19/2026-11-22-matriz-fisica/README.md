# Revisão da matriz física dos experimentos

Base local `b1c0de590748ba995de38199e788bde0229e0116`, após consolidar as
correções de operadores. As medições identificam a árvore local pelo patch
e pelos hashes das fontes.
O plano está em `docs/superpowers/plans/2026-11-22-matriz-fisica.md`.

## Contrato e alteração

PDF revisado de 04/11/2026, §§5.1 e 5.2. Configuração a exige ausência de
métodos primários/secundários nos campos do predicado. Na b de junção, um
campo dita a primária e o outro não tem índice. Escolher Scan em um heap
indexado não comprovava essas condições.

Foram acrescentadas quatro variantes heap sem índices, de Curso,
Disciplina, Aluno e Inscrição. As variantes indexadas existentes continuam
nos casos que exigem índices. As consultas e os três algoritmos foram
preservados. `metodos.csv` registra os métodos obtidos de
`Monitor::access_paths` por campo; `configuracoes.csv` registra operandos e
campos na ordem efetiva da execução. O validador cruza os dois arquivos.
Métodos de outro campo não afetam a classificação. A presença de índices
continua distinta do uso efetivo pelo operador, registrado em caminhos.csv.

O validador também verifica as repetições realmente executadas, motivos
de inviabilidade e a existência de células obrigatórias bem-sucedidas.
Uma rodada só com linhas inviáveis sem motivo e uma rodada com uma única
repetição deixam de ser aprovadas por um manifesto que anuncia três.

## Testes e resultados

`red.log` contém 18 testes da matriz com o validador anterior em uma cópia
de `b1c0de5`, incluindo as falhas dos novos casos. Os fixtures fornecem
configurações físicas independentes, com três repetições. Depois da
alteração, os 18 testes passaram (`green-python.log`). A validação global
final inclui também os testes do conferidor e está no diretório irmão
`2026-11-22-conferencia/`.

Rodada reduzida com tuplas: 168 combinações sem divergências SQLite.
`smoke-matrix.log` registra a reprovação esperada como evidência da matriz
integral, pelos parâmetros escala=0,1 e uma repetição.

Rodada integral: escala=1, semente 20261122, LRU/MRU, M=32/2, lote=1.000 e
3 a 10 repetições. Foram conferidas 1.159 linhas em 168 combinações, com
zero ausências e divergências SQLite. O validador aprovou 60 células por
política, 108 executadas e 12 não aplicáveis (igualdade UNIQUE no caso 3),
sem inviáveis ou ausentes. Portes conferidos em 782 execuções de junção.

`full/` preserva os CSVs, manifesto e tabelas. A conferência dessa pasta
pode ser repetida sem reconstruir os arquivos binários. Os dados SQL,
binários do motor e dumps da rodada reduzida ficam no diretório temporário
identificado em `workdir.txt`; podem ser regenerados com a semente e fontes.

O manifesto contém o SHA base, pois o driver foi modificado localmente.
`bench-full-source.cpp` preserva o fonte usado na rodada integral, anterior
somente ao ajuste posterior de `--so`, que não altera a execução sem filtro.
Os resultados antigos em `resultados_exemplo/` foram preservados e os
READMEs indicam seu caráter histórico. A variação das repetições decorre
também do tempo; `convergencia.csv` registra os casos encerrados no teto.

## Perfil de I/O

O perfil `io-rapido-v2` renomeia os scans afetados para
`*_heap_sem_indices` e mantém 168 identidades. `io_cenarios_v1.json` foi
preservado. O comparador lê os dois protocolos e recusa cruzá-los.
Ambos os motores são medidos com o driver v2, incluindo os mesmos métodos
e configurações nos hashes da assinatura.

Na comparação válida em `comparacao.*`, não houve alteração de leituras,
escritas ou flush. É um controle entre motores sob o novo perfil. A matriz
antiga e a nova têm configurações distintas; números históricos não devem
ser misturados como se o protocolo fosse o mesmo.

Revisão adicional do diff sem bloqueadores; também reexecutou 18 testes
da matriz e 19 do comparador de I/O na versão então disponível.

## Comandos

Executados da raiz do worktree, com saídas próprias em `/tmp`.

```bash
cmake --build /tmp/cbd-pr19-slot-prefetch-release --parallel 3
CBD_COMMIT=b1c0de590748ba995de38199e788bde0229e0116 \
  /tmp/cbd-pr19-slot-prefetch-release/bancada --saida DIRETORIO_NOVO
python3 -I PrototipoIA/Implementacao/tools/reference.py DIRETORIO_NOVO
python3 -I PrototipoIA/Implementacao/tools/compare.py DIRETORIO_NOVO
python3 -I PrototipoIA/Implementacao/tools/verificar_matriz.py DIRETORIO_NOVO
python3 -I PrototipoIA/Implementacao/tools/tabelas.py DIRETORIO_NOVO
```

Os caminhos efetivos e parâmetros de cada medição de I/O estão em
`medicao-*.json` e `io-final-*.log`. Os fontes completos são identificados
pelo commit base e pelo patch local preservado nesta evidência.

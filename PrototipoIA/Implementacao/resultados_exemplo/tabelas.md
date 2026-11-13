# Tabelas da rodada (LRU)

Geradas por `tools/tabelas.py` a partir de `resultados_exemplo/`. A métrica é o número de blocos transferidos (leituras + gravações); o tempo é só complementar.

Índice: [Resumo da rodada](#resumo-da-rodada) · [Cobertura da matriz do enunciado](#cobertura-da-matriz-do-enunciado) · [Seleção por igualdade em campo UNIQUE (lote de chaves)](#seleção-por-igualdade-em-campo-unique-lote-de-chaves) · [Seleção por faixa](#seleção-por-faixa) · [Mesma seleção, organizações e caminhos diferentes](#mesma-seleção-organizações-e-caminhos-diferentes) · [Junções: custo de cada algoritmo](#junções-custo-de-cada-algoritmo) · [Melhor algoritmo por situação](#melhor-algoritmo-por-situação) · [Escolha da externa e da interna](#escolha-da-externa-e-da-interna) · [Política de substituição: LRU × MRU](#política-de-substituição-lru--mru) · [Custo estimado × medido](#custo-estimado--medido) · [Fases do sort-merge e do hash join](#fases-do-sort-merge-e-do-hash-join) · [Estruturas construídas](#estruturas-construídas) · [Ocupação antes e depois da carga de manutenção](#ocupação-antes-e-depois-da-carga-de-manutenção) · [Células que não rodam](#células-que-não-rodam) · [Acesso executado nas junções](#acesso-executado-nas-junções) · [Convergência das repetições](#convergência-das-repetições)

## Resumo da rodada

| Item | Valor |
|---|---|
| Escala | 1 |
| Políticas comparadas | LRU, MRU |
| Repetições por combinação | 3..10 (tolerância 0.05) |
| M das junções / das seleções | 32 / 2 molduras |
| Chaves por lote de seleção pontual | 1000 |
| Combinações medidas (por política) | 84 |
| Linhas medidas no CSV (todas as repetições e políticas) | 555 |
| Células `nao_aplicavel` (caso 1) por política | 54 |
| Células `nao_aplicavel` (caso 2) por política | 2 |
| Células `nao_aplicavel` (caso 3) por política | 6 |
| Células `nao_aplicavel` (caso 4) por política | 4 |
| Conferência com o SQLite | conferidas: 555 linhas medidas; sem execução: nao_aplicavel: 132; divergências: 0 |
| Duração da rodada | 37.9 s |

## Cobertura da matriz do enunciado

Cada célula do enunciado v2 (04/11/2026), lida do resultados.csv (LRU). obrigatórias: 60 células, 54 executadas, 6 não aplicáveis, 0 inviáveis e 0 ausentes; opcionais: 96 células, 10 executadas, 56 não aplicáveis, 0 inviáveis e 30 não implementadas. Nas junções, as colunas são os algoritmos (BNL, sort-merge, hash); nas seleções, as seletividades (pontual, 1%, 50%; a faixa não tem nível pontual). Igualdade em UNIQUE com 1% ou 50% é o caso 3: a consulta devolve 1 linha e é medida no nível pontual. “AUSENTE” em negrito seria uma célula obrigatória sem linha no CSV; “não implementada” é uma célula opcional que a bancada não monta.

| Grupo | Célula | 1ª coluna | 2ª coluna | 3ª coluna |
|---|---|---|---|---|
| natural join (obrigatória) | menor ⋈ menor, config. a | ok | ok | ok |
| natural join (obrigatória) | menor ⋈ menor, config. b | ok | ok | ok |
| natural join (obrigatória) | menor ⋈ menor, config. c | ok (2 variantes) | ok (2 variantes) | ok (2 variantes) |
| natural join (obrigatória) | menor ⋈ menor, config. d | ok | ok | ok |
| natural join (obrigatória) | menor ⋈ menor, config. e | ok | ok | ok |
| natural join (obrigatória) | menor ⋈ maior, config. a | ok | ok | ok |
| natural join (obrigatória) | menor ⋈ maior, config. b | ok | ok | ok |
| natural join (obrigatória) | menor ⋈ maior, config. c | ok (2 variantes) | ok (2 variantes) | ok (2 variantes) |
| natural join (obrigatória) | menor ⋈ maior, config. d | ok | ok | ok |
| natural join (obrigatória) | menor ⋈ maior, config. e | ok | ok | ok |
| natural join (obrigatória) | maior ⋈ maior, config. a | ok | ok | ok |
| natural join (obrigatória) | maior ⋈ maior, config. b | ok | ok | ok |
| natural join (obrigatória) | maior ⋈ maior, config. c | ok (2 variantes) | ok (2 variantes) | ok (2 variantes) |
| natural join (obrigatória) | maior ⋈ maior, config. d | ok | ok | ok |
| natural join (obrigatória) | maior ⋈ maior, config. e | ok | ok | ok |
| equijoin UNIQUE (opcional) | menor ⋈ menor, config. a | não implementada | não implementada | não implementada |
| equijoin UNIQUE (opcional) | menor ⋈ menor, config. b | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | menor ⋈ menor, config. c | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | menor ⋈ menor, config. d | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | menor ⋈ menor, config. e | não implementada | não implementada | não implementada |
| equijoin UNIQUE (opcional) | menor ⋈ maior, config. a | não implementada | não implementada | não implementada |
| equijoin UNIQUE (opcional) | menor ⋈ maior, config. b | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | menor ⋈ maior, config. c | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | menor ⋈ maior, config. d | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | menor ⋈ maior, config. e | não implementada | não implementada | não implementada |
| equijoin UNIQUE (opcional) | maior ⋈ maior, config. a | ok | ok | ok |
| equijoin UNIQUE (opcional) | maior ⋈ maior, config. b | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | maior ⋈ maior, config. c | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | maior ⋈ maior, config. d | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin UNIQUE (opcional) | maior ⋈ maior, config. e | não implementada | não implementada | não implementada |
| equijoin NOT UNIQUE (opcional) | menor ⋈ menor, config. a | não implementada | não implementada | não implementada |
| equijoin NOT UNIQUE (opcional) | menor ⋈ menor, config. b | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | menor ⋈ menor, config. c | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | menor ⋈ menor, config. d | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | menor ⋈ menor, config. e | não implementada | não implementada | não implementada |
| equijoin NOT UNIQUE (opcional) | menor ⋈ maior, config. a | não implementada | não implementada | não implementada |
| equijoin NOT UNIQUE (opcional) | menor ⋈ maior, config. b | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | menor ⋈ maior, config. c | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | menor ⋈ maior, config. d | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | menor ⋈ maior, config. e | não implementada | não implementada | não implementada |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. a | não implementada | não implementada | não implementada |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. b | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. c | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. d | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. e | ok | ok | ok |
| igualdade em UNIQUE (obrigatória) | config. a | ok | n/a (caso 3) | n/a (caso 3) |
| igualdade em UNIQUE (obrigatória) | config. b | ok (2 variantes) | n/a (caso 3) | n/a (caso 3) |
| igualdade em UNIQUE (obrigatória) | config. c | ok | n/a (caso 3) | n/a (caso 3) |
| faixa em NOT UNIQUE (obrigatória) | config. a | — | ok | ok |
| faixa em NOT UNIQUE (obrigatória) | config. b | — | ok | ok |
| faixa em NOT UNIQUE (obrigatória) | config. c | — | ok (3 variantes) | ok (3 variantes) |
| faixa em UNIQUE (opcional) | config. a | — | ok | ok |
| faixa em UNIQUE (opcional) | config. b | — | ok | ok |
| faixa em UNIQUE (opcional) | config. c | — | n/a (caso 2) | n/a (caso 2) |

## Seleção por igualdade em campo UNIQUE (lote de chaves)

Cada execução busca um lote de 1.000 chaves sorteadas. O lote muda a cada repetição, então as contagens variam um pouco: a tabela mostra a mediana e, em “Var. reps”, a amplitude entre repetições. “Leituras/chave” divide as leituras pelo lote. “× melhor” compara com o caminho de menor custo na mesma consulta.

| Consulta | Config. | Caminho | Linhas | Leituras | Leituras/chave | Aleatórias | Taxa de acerto | Estimado | Erro est. | Tempo (ms) | Var. reps | × melhor |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| igualdade em UNIQUE: cpf | a | heap, varredura | 1.000 | 321.360 | 321,36 | 996 | 0,00 | 323.000 | +0,5% | 296,5 | 5,3% | 169,0× |
| igualdade em UNIQUE: cpf | c | heap + hash secundário (cpf) | 1.000 | 1.901 | 1,90 | 1.901 | 0,05 | 2.000 | +5,2% | 1,7 | 2,1% | melhor |
| igualdade em UNIQUE: dre | a | heap, varredura | 1.000 | 326.680 | 326,68 | 996 | 0,00 | 323.000 | -1,1% | 252,6 | 3,0% | 323,9× |
| igualdade em UNIQUE: dre | b | hash primário (dre) | 1.000 | 1.008 | 1,01 | 1.006 | 0,00 | 1.108 | +9,8% | 0,8 | 0,6% | melhor |
| igualdade em UNIQUE: dre | b | B+ primária (dre) | 1.000 | 1.045 | 1,04 | 1.002 | 0,66 | 3.000 | +187,1% | 3,4 | 1,3% | 1,04× |

## Seleção por faixa

“Selet.” é o alvo; “Efetiva” é a fração de linhas que a faixa realmente devolve, feita de grupos inteiros de valores iguais (decisão 174). “% seq.” é a fração das leituras que foram sequenciais. “× melhor” compara com o caminho de menor custo na mesma consulta e seletividade.

| Consulta | Selet. | Efetiva | Config. | Caminho | Linhas | Leituras | % seq. | Estimado | Erro est. | Tempo (ms) | × melhor |
|---|---:|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| faixa em UNIQUE: dre | 1% | 1,00% | a | heap, varredura | 200 | 646 | 99,8% | 646 | +0,0% | 0,58 | 53,8× |
| faixa em UNIQUE: dre | 1% | 1,00% | b | B+ primária (dre) | 200 | 12 | 75,0% | 12 | +0,0% | 0,06 | melhor |
| faixa em UNIQUE: dre | 50% | 50,00% | a | heap, varredura | 10.000 | 646 | 99,8% | 646 | +0,0% | 2,91 | 1,35× |
| faixa em UNIQUE: dre | 50% | 50,00% | b | B+ primária (dre) | 10.000 | 479 | 99,4% | 478 | -0,2% | 2,84 | melhor |
| faixa em NOT UNIQUE: cr | 1% | 1,00% | a | heap, varredura | 199 | 646 | 99,8% | 646 | +0,0% | 0,51 | 3,21× |
| faixa em NOT UNIQUE: cr | 1% | 1,00% | c | heap + B+ secundária (cr) | 199 | 201 | 0,5% | 201 | +0,0% | 0,14 | melhor |
| faixa em NOT UNIQUE: cr | 50% | 49,88% | a | heap, varredura | 9.976 | 646 | 99,8% | 646 | +0,0% | 2,94 | melhor |
| faixa em NOT UNIQUE: cr | 50% | 49,88% | c | heap + B+ secundária (cr) | 9.976 | 9.646 | 6,9% | 9.978 | +3,4% | 6,59 | 14,9× |
| faixa em NOT UNIQUE: curso | 1% | 1,03% | a | heap, varredura | 207 | 646 | 99,8% | 646 | +0,0% | 0,58 | 64,6× |
| faixa em NOT UNIQUE: curso | 1% | 1,03% | b | B+ primária (curso) | 207 | 13 | 76,9% | 12 | -7,7% | 0,07 | 1,30× |
| faixa em NOT UNIQUE: curso | 1% | 1,03% | c | hash primário (curso) + B+ clustering (curso) | 207 | 10 | 0,0% | 12 | +22,0% | 0,06 | melhor |
| faixa em NOT UNIQUE: curso | 1% | 1,03% | c | B+ primária (curso) + B+ clustering (curso) | 207 | 13 | 76,9% | 11 | -15,4% | 0,07 | 1,30× |
| faixa em NOT UNIQUE: curso | 50% | 49,84% | a | heap, varredura | 9.967 | 646 | 99,8% | 646 | +0,0% | 2,94 | 1,35× |
| faixa em NOT UNIQUE: curso | 50% | 49,84% | b | B+ primária (curso) | 9.967 | 477 | 99,4% | 477 | +0,0% | 2,86 | melhor |
| faixa em NOT UNIQUE: curso | 50% | 49,84% | c | B+ primária (curso) + B+ clustering (curso) | 9.967 | 477 | 99,4% | 476 | -0,2% | 2,83 | melhor |
| faixa em NOT UNIQUE: curso | 50% | 49,84% | c | hash primário (curso) + B+ clustering (curso) | 9.967 | 668 | 0,0% | 376 | -43,7% | 2,84 | 1,40× |

## Mesma seleção, organizações e caminhos diferentes

Blocos transferidos (LRU). Em negrito, o menor custo de cada linha. “n/a” é uma célula que não se aplica (decisão 170); vazio é uma combinação que a bancada não monta para essa consulta.

| Consulta | Selet. | Heap (varredura) | B+ primária | Hash primário | Heap + hash sec. | Heap + B+ sec. | B+ prim. + clustering | Hash prim. + clustering |
|---|---|---:|---:|---|---:|---:|---:|---:|
| igualdade em UNIQUE: cpf | pontual | 321.360 |  |  | **1.901** |  |  |  |
| igualdade em UNIQUE: dre | pontual | 326.680 | 1.045 | **1.008** |  |  |  |  |
| faixa em UNIQUE: dre | 1% | 646 | **12** | n/a (caso 4) |  |  |  |  |
| faixa em UNIQUE: dre | 50% | 646 | **479** | n/a (caso 4) |  |  |  |  |
| faixa em NOT UNIQUE: cr | 1% | 646 |  |  |  | **201** |  |  |
| faixa em NOT UNIQUE: cr | 50% | **646** |  |  |  | 9.646 |  |  |
| faixa em NOT UNIQUE: curso | 1% | 646 | 13 | n/a (caso 4) |  |  | 13 | **10** |
| faixa em NOT UNIQUE: curso | 50% | 646 | **477** | n/a (caso 4) |  |  | **477** | 668 |

## Junções: custo de cada algoritmo

M = 32 molduras. A primeira tabela de “Externa → interna” é a externa no BNL, a de build no hash e a primeira entrada do merge. “× melhor” compara com a opção mais barata para as mesmas entradas (todos os algoritmos e as duas ordens).

| Junção | Tamanho | Config. | Externa → interna | Algoritmo | Leituras | Gravações | Total | Estimado | Erro est. | Tempo (ms) | × melhor |
|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | BNL | 70.141 | 0 | 70.141 | 70.141 | +0,0% | 365,9 | 9,91× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | BNL | 70.171 | 0 | 70.171 | 70.171 | +0,0% | 276,3 | 9,92× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | sort-merge | 6.412 | 4.071 | 10.483 | 10.413 | -0,7% | 105,3 | 1,48× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | sort-merge | 6.412 | 4.071 | 10.483 | 10.413 | -0,7% | 106,1 | 1,48× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | hash | 4.708 | 2.367 | 7.075 | 7.135 | +0,8% | 56,2 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | hash | 7.202 | 4.833 | 12.035 | 15.549 | +29,2% | 83,3 | 1,70× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | BNL | 102.652 | 0 | 102.652 | 102.652 | +0,0% | 544,9 | 13,9× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Inscrição: heap, varredura → Aluno: B+ primária (dre) | sort-merge | 6.062 | 3.413 | 9.475 | 9.427 | -0,5% | 92,9 | 1,28× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | hash | 5.014 | 2.367 | 7.381 | 7.441 | +0,8% | 56,7 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | BNL | 147.292 | 0 | 147.292 | 147.292 | +0,0% | 559,9 | 43,4× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Inscrição: B+ primária (dre) → Aluno: B+ primária (dre) | sort-merge | 3.395 | 0 | 3.395 | 3.391 | -0,1% | 40,6 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | hash | 5.758 | 2.367 | 8.125 | 8.185 | +0,7% | 56,1 | 2,39× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | BNL | 147.840 | 0 | 147.840 | 147.840 | +0,0% | 553,7 | 30,5× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Inscrição: hash primário (dre) → Aluno: hash primário (dre) | sort-merge | 7.591 | 4.071 | 11.662 | 11.592 | -0,6% | 104,2 | 2,41× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | hash | 4.180 | 660 | 4.840 | 4.924 | +1,7% | 45,8 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | BNL | 102.652 | 0 | 102.652 | 102.652 | +0,0% | 538,8 | 13,9× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Inscrição: heap + B+ secundária (dre) → Aluno: B+ primária (dre) | sort-merge | 6.062 | 3.413 | 9.475 | 9.427 | -0,5% | 93,4 | 1,28× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | hash | 5.014 | 2.367 | 7.381 | 7.441 | +0,8% | 57,9 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | e | Aluno: heap + hash secundário (dre) → Inscrição: heap + B+ secundária (dre) | BNL | 70.141 | 0 | 70.141 | 70.141 | +0,0% | 368,1 | 9,91× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | e | Inscrição: heap + B+ secundária (dre) → Aluno: heap + hash secundário (dre) | sort-merge | 6.412 | 4.071 | 10.483 | 10.413 | -0,7% | 103,2 | 1,48× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | e | Aluno: heap + hash secundário (dre) → Inscrição: heap + B+ secundária (dre) | hash | 4.708 | 2.367 | 7.075 | 7.135 | +0,8% | 56,9 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Curso: heap, varredura → Disciplina: heap, varredura | BNL | 19 | 0 | 19 | 19 | +0,0% | 0,4 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Disciplina: heap, varredura → Curso: heap, varredura | sort-merge | 38 | 19 | 57 | 57 | +0,0% | 1,1 | 3,00× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Curso: heap, varredura → Disciplina: heap, varredura | hash | 19 | 0 | 19 | 19 | +0,0% | 0,5 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | b | Curso: B+ primária (codigo) → Disciplina: heap, varredura | BNL | 21 | 0 | 21 | 21 | +0,0% | 0,4 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | b | Disciplina: heap, varredura → Curso: B+ primária (codigo) | sort-merge | 34 | 12 | 46 | 45 | -2,2% | 0,8 | 2,19× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | b | Curso: B+ primária (codigo) → Disciplina: heap, varredura | hash | 21 | 0 | 21 | 21 | +0,0% | 0,5 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Curso: B+ primária (codigo) → Disciplina: B+ primária (curso) | BNL | 25 | 0 | 25 | 25 | +0,0% | 0,4 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Disciplina: B+ primária (curso) → Curso: B+ primária (codigo) | sort-merge | 27 | 0 | 27 | 25 | -7,4% | 0,5 | 1,08× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Curso: B+ primária (codigo) → Disciplina: B+ primária (curso) | hash | 25 | 0 | 25 | 25 | +0,0% | 0,5 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Curso: hash primário (codigo) → Disciplina: hash primário (curso) | BNL | 28 | 0 | 28 | 28 | +0,0% | 0,5 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Disciplina: hash primário (curso) → Curso: hash primário (codigo) | sort-merge | 47 | 19 | 66 | 66 | +0,0% | 1,1 | 2,36× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Curso: hash primário (codigo) → Disciplina: hash primário (curso) | hash | 28 | 0 | 28 | 28 | +0,0% | 0,5 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | d | Curso: B+ primária (codigo) → Disciplina: heap + B+ secundária (curso) | BNL | 21 | 0 | 21 | 21 | +0,0% | 0,4 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | d | Disciplina: heap + B+ secundária (curso) → Curso: B+ primária (codigo) | sort-merge | 34 | 12 | 46 | 45 | -2,2% | 0,9 | 2,19× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | d | Curso: B+ primária (codigo) → Disciplina: heap + B+ secundária (curso) | hash | 21 | 0 | 21 | 21 | +0,0% | 0,5 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | e | Curso: heap + hash secundário (codigo) → Disciplina: heap + B+ secundária (curso) | BNL | 19 | 0 | 19 | 19 | +0,0% | 0,4 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | e | Disciplina: heap + B+ secundária (curso) → Curso: heap + hash secundário (codigo) | sort-merge | 38 | 19 | 57 | 57 | +0,0% | 1,0 | 3,00× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | e | Curso: heap + hash secundário (codigo) → Disciplina: heap + B+ secundária (curso) | hash | 19 | 0 | 19 | 19 | +0,0% | 0,5 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Curso: heap, varredura → Aluno: heap, varredura | BNL | 653 | 0 | 653 | 653 | +0,0% | 9,6 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Aluno: heap, varredura → Curso: heap, varredura | sort-merge | 1.318 | 665 | 1.983 | 1.959 | -1,2% | 18,4 | 3,04× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Curso: heap, varredura → Aluno: heap, varredura | hash | 653 | 0 | 653 | 653 | +0,0% | 9,1 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Curso: heap, varredura → Aluno: B+ primária (curso) | BNL | 959 | 0 | 959 | 959 | +0,0% | 9,6 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Aluno: B+ primária (curso) → Curso: heap, varredura | sort-merge | 968 | 7 | 975 | 973 | -0,2% | 9,4 | 1,02× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Curso: heap, varredura → Aluno: B+ primária (curso) | hash | 959 | 0 | 959 | 959 | +0,0% | 9,0 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Curso: B+ primária (codigo) → Aluno: B+ primária (curso) | BNL | 961 | 0 | 961 | 961 | +0,0% | 9,7 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Aluno: B+ primária (curso) → Curso: B+ primária (codigo) | sort-merge | 964 | 0 | 964 | 961 | -0,3% | 9,1 | 1,00× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Curso: B+ primária (codigo) → Aluno: B+ primária (curso) | hash | 961 | 0 | 961 | 961 | +0,0% | 9,0 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Curso: hash primário (codigo) → Aluno: hash primário (curso) | BNL | 802 | 0 | 802 | 802 | +0,0% | 9,8 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Aluno: hash primário (curso) → Curso: hash primário (codigo) | sort-merge | 1.467 | 665 | 2.132 | 2.108 | -1,1% | 18,1 | 2,66× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Curso: hash primário (codigo) → Aluno: hash primário (curso) | hash | 802 | 0 | 802 | 802 | +0,0% | 9,1 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | d | Curso: B+ primária (codigo) → Aluno: heap + B+ secundária (curso) | BNL | 655 | 0 | 655 | 655 | +0,0% | 9,7 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | d | Aluno: heap + B+ secundária (curso) → Curso: B+ primária (codigo) | sort-merge | 1.314 | 658 | 1.972 | 1.947 | -1,3% | 18,4 | 3,01× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | d | Curso: B+ primária (codigo) → Aluno: heap + B+ secundária (curso) | hash | 655 | 0 | 655 | 655 | +0,0% | 9,1 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | e | Curso: heap + hash secundário (codigo) → Aluno: heap + B+ secundária (curso) | BNL | 653 | 0 | 653 | 653 | +0,0% | 9,7 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | e | Aluno: heap + B+ secundária (curso) → Curso: heap + hash secundário (codigo) | sort-merge | 1.318 | 665 | 1.983 | 1.959 | -1,2% | 18,6 | 3,04× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | e | Curso: heap + hash secundário (codigo) → Aluno: heap + B+ secundária (curso) | hash | 653 | 0 | 653 | 653 | +0,0% | 9,1 | melhor |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | BNL | 22.506 | 0 | 22.506 | 22.506 | +0,0% | 120,6 | 6,21× |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | sort-merge | 3.040 | 1.852 | 4.892 | 4.644 | -5,1% | 40,1 | 1,35× |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | hash | 2.406 | 1.218 | 3.624 | 3.686 | +1,7% | 21,8 | melhor |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | maior ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | BNL | 1.980 | 0 | 1.980 | 1.980 | +0,0% | 79,6 | 1,02× |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | maior ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | sort-merge | 1.322 | 634 | 1.956 | 2.064 | +5,5% | 84,4 | 1,01× |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | maior ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | hash | 1.314 | 626 | 1.940 | 2.076 | +7,0% | 78,2 | melhor |

## Melhor algoritmo por situação

Para cada célula (tipo, tamanho, configuração), o total de blocos de cada algoritmo na melhor ordem externa/interna. “Vantagem” é quantas vezes o próximo algoritmo mais barato custa em relação ao vencedor (empates contam juntos).

| Junção | Tipo | Tamanho | Config. | Entradas | BNL | Sort-merge | Hash | Vencedor | Vantagem |
|---|---|---|---|---|---:|---:|---:|---|---:|
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | a | Aluno: heap, varredura ⋈ Inscrição: heap, varredura | 70.141 | 10.483 | **7.075** | hash | 1,48× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) ⋈ Inscrição: heap, varredura | 102.652 | 9.475 | **7.381** | hash | 1,28× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) ⋈ Inscrição: B+ primária (dre) | 147.292 | **3.395** | 8.125 | sort-merge | 2,39× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | c | Aluno: hash primário (dre) ⋈ Inscrição: hash primário (dre) | 147.840 | 11.662 | **4.840** | hash | 2,41× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) ⋈ Inscrição: heap + B+ secundária (dre) | 102.652 | 9.475 | **7.381** | hash | 1,28× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | e | Aluno: heap + hash secundário (dre) ⋈ Inscrição: heap + B+ secundária (dre) | 70.141 | 10.483 | **7.075** | hash | 1,48× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | a | Curso: heap, varredura ⋈ Disciplina: heap, varredura | **19** | 57 | **19** | BNL = hash (empate) | 3,00× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | b | Curso: B+ primária (codigo) ⋈ Disciplina: heap, varredura | **21** | 46 | **21** | BNL = hash (empate) | 2,19× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | c | Curso: B+ primária (codigo) ⋈ Disciplina: B+ primária (curso) | **25** | 27 | **25** | BNL = hash (empate) | 1,08× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | c | Curso: hash primário (codigo) ⋈ Disciplina: hash primário (curso) | **28** | 66 | **28** | BNL = hash (empate) | 2,36× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | d | Curso: B+ primária (codigo) ⋈ Disciplina: heap + B+ secundária (curso) | **21** | 46 | **21** | BNL = hash (empate) | 2,19× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | e | Curso: heap + hash secundário (codigo) ⋈ Disciplina: heap + B+ secundária (curso) | **19** | 57 | **19** | BNL = hash (empate) | 3,00× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | a | Curso: heap, varredura ⋈ Aluno: heap, varredura | **653** | 1.983 | **653** | BNL = hash (empate) | 3,04× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | b | Curso: heap, varredura ⋈ Aluno: B+ primária (curso) | **959** | 975 | **959** | BNL = hash (empate) | 1,02× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | c | Curso: B+ primária (codigo) ⋈ Aluno: B+ primária (curso) | **961** | 964 | **961** | BNL = hash (empate) | 1,00× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | c | Curso: hash primário (codigo) ⋈ Aluno: hash primário (curso) | **802** | 2.132 | **802** | BNL = hash (empate) | 2,66× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | d | Curso: B+ primária (codigo) ⋈ Aluno: heap + B+ secundária (curso) | **655** | 1.972 | **655** | BNL = hash (empate) | 3,01× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | e | Curso: heap + hash secundário (codigo) ⋈ Aluno: heap + B+ secundária (curso) | **653** | 1.983 | **653** | BNL = hash (empate) | 3,04× |
| Aluno ⋈ Detalhes (cpf) | equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura ⋈ Aluno: heap, varredura | 22.506 | 4.892 | **3.624** | hash | 1,35× |
| Aluno ⋈ Turma (professor_orientador = professor) | equijoin NOT UNIQUE | maior ⋈ maior | e | Turma: heap + B+ secundária (professor) ⋈ Aluno: heap + B+ secundária (professor_orientador) | 1.980 | 1.956 | **1.940** | hash | 1,01× |

## Escolha da externa e da interna

A ordem padrão segue a decisão 122: no BNL e no hash, a menor por fora (build); no sort-merge, a UNIQUE por dentro. “Δ” é o custo da ordem trocada em relação à padrão.

| Junção | Config. | Algoritmo | Ordem padrão | Total | Ordem trocada | Total | Δ |
|---|---|---|---|---:|---|---:|---:|
| Aluno ⋈ Inscrição (dre) | a | BNL | Aluno: heap, varredura → Inscrição: heap, varredura | 70.141 | Inscrição: heap, varredura → Aluno: heap, varredura | 70.171 | +0,0% |
| Aluno ⋈ Inscrição (dre) | a | sort-merge | Inscrição: heap, varredura → Aluno: heap, varredura | 10.483 | Aluno: heap, varredura → Inscrição: heap, varredura | 10.483 | +0,0% |
| Aluno ⋈ Inscrição (dre) | a | hash | Aluno: heap, varredura → Inscrição: heap, varredura | 7.075 | Inscrição: heap, varredura → Aluno: heap, varredura | 12.035 | +70,1% |

## Política de substituição: LRU × MRU

Só as combinações em que o total de blocos muda. Nas outras 68, as duas políticas transferem o mesmo número de blocos.

| Consulta | Selet./tamanho | Onde | LRU | MRU | Δ | Acerto LRU | Acerto MRU |
|---|---|---|---:|---:|---:|---:|---:|
| Aluno ⋈ Detalhes (cpf) | maior ⋈ maior | BNL, config. a: Detalhes: heap, varredura → Aluno: heap, varredura | 22.506 | 21.995 | -2,3% | 0,00 | 0,02 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. a: Inscrição: heap, varredura → Aluno: heap, varredura | 70.171 | 68.580 | -2,3% | 0,00 | 0,02 |
| Aluno ⋈ Turma (professor_orientador = professor) | maior ⋈ maior | BNL, config. e: Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | 1.980 | 1.948 | -1,6% | 0,00 | 0,02 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. a: Aluno: heap, varredura → Inscrição: heap, varredura | 70.141 | 69.525 | -0,9% | 0,00 | 0,01 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. e: Aluno: heap + hash secundário (dre) → Inscrição: heap + B+ secundária (dre) | 70.141 | 69.525 | -0,9% | 0,00 | 0,01 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. b: Aluno: B+ primária (dre) → Inscrição: heap, varredura | 102.652 | 101.751 | -0,9% | 0,00 | 0,01 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. d: Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | 102.652 | 101.751 | -0,9% | 0,00 | 0,01 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. c: Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | 147.292 | 146.391 | -0,6% | 0,00 | 0,01 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. c: Aluno: hash primário (dre) → Inscrição: hash primário (dre) | 147.840 | 147.000 | -0,6% | 0,00 | 0,01 |
| igualdade em UNIQUE: cpf | pontual | config. c: heap + hash secundário (cpf) | 1.901 | 1.892 | -0,5% | 0,05 | 0,05 |
| igualdade em UNIQUE: dre | pontual | config. a: heap, varredura | 326.680 | 325.976 | -0,2% | 0,00 | 0,00 |
| igualdade em UNIQUE: cpf | pontual | config. a: heap, varredura | 321.360 | 320.821 | -0,2% | 0,00 | 0,00 |
| faixa em NOT UNIQUE: curso | 50% | config. c: hash primário (curso) + B+ clustering (curso) | 668 | 667 | -0,1% | 0,00 | 0,00 |
| igualdade em UNIQUE: dre | pontual | config. b: hash primário (dre) | 1.008 | 1.010 | +0,1% | 0,00 | 0,00 |
| faixa em NOT UNIQUE: cr | 50% | config. c: heap + B+ secundária (cr) | 9.646 | 9.633 | -0,1% | 0,04 | 0,04 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | hash, config. a: Inscrição: heap, varredura → Aluno: heap, varredura | 12.035 | 12.034 | -0,0% | 0,00 | 0,00 |

## Custo estimado × medido

Erro = (estimado − medido) / medido, em blocos transferidos. As fórmulas são as dos livros (Elmasri e Silberschatz), com os parâmetros medidos pela própria bancada.

| Família | Combinações | Mediana do erro absoluto | Erro absoluto ≤ 10% | Maior erro | Onde |
|---|---:|---:|---|---:|---|
| junção: BNL | 21 | 0,0% | 21 de 21 | +0,0% | Aluno ⋈ Inscrição (dre) (config. a: Aluno: heap, varredura → Inscrição: heap, varredura) |
| junção: hash | 21 | 0,0% | 20 de 21 | +29,2% | Aluno ⋈ Inscrição (dre) (config. a: Inscrição: heap, varredura → Aluno: heap, varredura) |
| junção: sort-merge | 21 | 0,7% | 21 de 21 | -7,4% | Curso ⋈ Disciplina (codigo = curso) (config. c: Disciplina: B+ primária (curso) → Curso: B+ primária (codigo)) |
| seleção: faixa | 16 | 0,0% | 13 de 16 | -43,7% | faixa em NOT UNIQUE: curso (config. c: hash primário (curso) + B+ clustering (curso)) |
| seleção: igualdade (lote) | 5 | 5,2% | 4 de 5 | +187,1% | igualdade em UNIQUE: dre (config. b: B+ primária (dre)) |

## Fases do sort-merge e do hash join

Leituras/gravações de cada fase (1ª repetição). No hash join, o resumo traz os passes de particionamento recursivo e os bailouts (decisão 171).

| Junção | Config. | Externa → interna | Algoritmo | Fases (leituras/gravações) | Recursão |
|---|---|---|---|---|---|
| Aluno ⋈ Inscrição (dre) | a | Inscrição: heap, varredura → Aluno: heap, varredura | sort-merge | runs externa 1.695/1.718 · runs interna 646/658 · passes 1.718/1.695 · intercalação final 2.353/0 | — |
| Aluno ⋈ Inscrição (dre) | a | Aluno: heap, varredura → Inscrição: heap, varredura | sort-merge | runs externa 646/658 · runs interna 1.695/1.718 · passes 1.718/1.695 · intercalação final 2.353/0 | — |
| Aluno ⋈ Inscrição (dre) | a | Aluno: heap, varredura → Inscrição: heap, varredura | hash | particiona externa 646/657 · particiona interna 1.695/1.710 · build probe 2.367/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | a | Inscrição: heap, varredura → Aluno: heap, varredura | hash | particiona externa 1.695/1.707 · particiona interna 646/660 · build probe 4.861/2.466 | passes recursivos=32 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | b | Inscrição: heap, varredura → Aluno: B+ primária (dre) | sort-merge | runs externa 1.695/1.718 · passes 1.718/1.695 · intercalação final 2.649/0 | — |
| Aluno ⋈ Inscrição (dre) | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | hash | particiona externa 952/657 · particiona interna 1.695/1.710 · build probe 2.367/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | c | Inscrição: B+ primária (dre) → Aluno: B+ primária (dre) | sort-merge | passes 0/0 · intercalação final 3.395/0 | — |
| Aluno ⋈ Inscrição (dre) | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | hash | particiona externa 952/657 · particiona interna 2.439/1.710 · build probe 2.367/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | c | Inscrição: hash primário (dre) → Aluno: hash primário (dre) | sort-merge | runs externa 2.624/1.718 · runs interna 896/658 · passes 1.718/1.695 · intercalação final 2.353/0 | — |
| Aluno ⋈ Inscrição (dre) | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | hash | particiona externa 896/660 · build probe 3.284/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | d | Inscrição: heap + B+ secundária (dre) → Aluno: B+ primária (dre) | sort-merge | runs externa 1.695/1.718 · passes 1.718/1.695 · intercalação final 2.649/0 | — |
| Aluno ⋈ Inscrição (dre) | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | hash | particiona externa 952/657 · particiona interna 1.695/1.710 · build probe 2.367/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | e | Inscrição: heap + B+ secundária (dre) → Aluno: heap + hash secundário (dre) | sort-merge | runs externa 1.695/1.718 · runs interna 646/658 · passes 1.718/1.695 · intercalação final 2.353/0 | — |
| Aluno ⋈ Inscrição (dre) | e | Aluno: heap + hash secundário (dre) → Inscrição: heap + B+ secundária (dre) | hash | particiona externa 646/657 · particiona interna 1.695/1.710 · build probe 2.367/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Disciplina (codigo = curso) | a | Disciplina: heap, varredura → Curso: heap, varredura | sort-merge | runs externa 12/12 · runs interna 7/7 · passes 0/0 · intercalação final 19/0 | — |
| Curso ⋈ Disciplina (codigo = curso) | a | Curso: heap, varredura → Disciplina: heap, varredura | hash | build probe 19/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Disciplina (codigo = curso) | b | Disciplina: heap, varredura → Curso: B+ primária (codigo) | sort-merge | runs externa 12/12 · passes 0/0 · intercalação final 22/0 | — |
| Curso ⋈ Disciplina (codigo = curso) | b | Curso: B+ primária (codigo) → Disciplina: heap, varredura | hash | build probe 21/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Disciplina (codigo = curso) | c | Disciplina: B+ primária (curso) → Curso: B+ primária (codigo) | sort-merge | passes 0/0 · intercalação final 27/0 | — |
| Curso ⋈ Disciplina (codigo = curso) | c | Curso: B+ primária (codigo) → Disciplina: B+ primária (curso) | hash | build probe 25/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Disciplina (codigo = curso) | c | Disciplina: hash primário (curso) → Curso: hash primário (codigo) | sort-merge | runs externa 17/12 · runs interna 11/7 · passes 0/0 · intercalação final 19/0 | — |
| Curso ⋈ Disciplina (codigo = curso) | c | Curso: hash primário (codigo) → Disciplina: hash primário (curso) | hash | build probe 28/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Disciplina (codigo = curso) | d | Disciplina: heap + B+ secundária (curso) → Curso: B+ primária (codigo) | sort-merge | runs externa 12/12 · passes 0/0 · intercalação final 22/0 | — |
| Curso ⋈ Disciplina (codigo = curso) | d | Curso: B+ primária (codigo) → Disciplina: heap + B+ secundária (curso) | hash | build probe 21/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Disciplina (codigo = curso) | e | Disciplina: heap + B+ secundária (curso) → Curso: heap + hash secundário (codigo) | sort-merge | runs externa 12/12 · runs interna 7/7 · passes 0/0 · intercalação final 19/0 | — |
| Curso ⋈ Disciplina (codigo = curso) | e | Curso: heap + hash secundário (codigo) → Disciplina: heap + B+ secundária (curso) | hash | build probe 19/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | a | Aluno: heap, varredura → Curso: heap, varredura | sort-merge | runs externa 646/658 · runs interna 7/7 · passes 0/0 · intercalação final 665/0 | — |
| Curso ⋈ Aluno (codigo = curso) | a | Curso: heap, varredura → Aluno: heap, varredura | hash | build probe 653/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | b | Aluno: B+ primária (curso) → Curso: heap, varredura | sort-merge | runs interna 7/7 · passes 0/0 · intercalação final 961/0 | — |
| Curso ⋈ Aluno (codigo = curso) | b | Curso: heap, varredura → Aluno: B+ primária (curso) | hash | build probe 959/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | c | Aluno: B+ primária (curso) → Curso: B+ primária (codigo) | sort-merge | passes 0/0 · intercalação final 964/0 | — |
| Curso ⋈ Aluno (codigo = curso) | c | Curso: B+ primária (codigo) → Aluno: B+ primária (curso) | hash | build probe 961/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | c | Aluno: hash primário (curso) → Curso: hash primário (codigo) | sort-merge | runs externa 791/658 · runs interna 11/7 · passes 0/0 · intercalação final 665/0 | — |
| Curso ⋈ Aluno (codigo = curso) | c | Curso: hash primário (codigo) → Aluno: hash primário (curso) | hash | build probe 802/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | d | Aluno: heap + B+ secundária (curso) → Curso: B+ primária (codigo) | sort-merge | runs externa 646/658 · passes 0/0 · intercalação final 668/0 | — |
| Curso ⋈ Aluno (codigo = curso) | d | Curso: B+ primária (codigo) → Aluno: heap + B+ secundária (curso) | hash | build probe 655/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | e | Aluno: heap + B+ secundária (curso) → Curso: heap + hash secundário (codigo) | sort-merge | runs externa 646/658 · runs interna 7/7 · passes 0/0 · intercalação final 665/0 | — |
| Curso ⋈ Aluno (codigo = curso) | e | Curso: heap + hash secundário (codigo) → Aluno: heap + B+ secundária (curso) | hash | build probe 653/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Detalhes (cpf) | a | Detalhes: heap, varredura → Aluno: heap, varredura | sort-merge | runs externa 542/548 · runs interna 646/658 · passes 658/646 · intercalação final 1.194/0 | — |
| Aluno ⋈ Detalhes (cpf) | a | Detalhes: heap, varredura → Aluno: heap, varredura | hash | particiona externa 542/558 · particiona interna 646/660 · build probe 1.218/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Turma (professor_orientador = professor) | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | sort-merge | runs externa 42/43 · runs interna 646/591 · passes 0/0 · intercalação final 634/0 | — |
| Aluno ⋈ Turma (professor_orientador = professor) | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | hash | particiona externa 42/44 · particiona interna 646/582 · build probe 626/0 | passes recursivos=0 bailouts=0 |

## Estruturas construídas

Medidas pela própria bancada depois da carga. n = registros, R = bytes por registro, bfr = registros por bloco. Nos índices, “Blocos” conta só o índice.

| Estrutura | n | R | bfr | Blocos | Altura | Folhas | Distintos | Buckets | Overflow | Fator de carga | Cadeia média | Cadeia máx. |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| aluno_bt_curso:btree | 20.000 | 127 | 31 | 952 | 3 | 952 | 200 | — | — | — | — | — |
| aluno_bt_curso:sec_btree_clustering(curso) | 20.000 | 127 | 31 | 3 | 2 | 2 | 200 | — | 0 | — | — | — |
| aluno_bt_dre:btree | 20.000 | 127 | 31 | 952 | 3 | 952 | 20.000 | — | — | — | — | — |
| aluno_h_curso:hash | 20.000 | 127 | 31 | 791 | — | — | — | 211 | 580 | 3,06 | 3,75 | 14 |
| aluno_h_curso:sec_btree_clustering(curso) | 20.000 | 127 | 31 | 3 | 2 | 2 | 200 | — | 0 | — | — | — |
| aluno_h_dre:hash | 20.000 | 127 | 31 | 896 | — | — | — | 809 | 87 | 0,80 | 1,11 | 2 |
| aluno_heap:heap | 20.000 | 127 | 31 | 646 | — | — | — | — | — | — | — | — |
| aluno_heap:sec_hash(cpf) | 20.000 | 127 | 31 | 149 | — | — | — | 149 | 0 | 0,76 | 1,00 | 1 |
| aluno_heap:sec_btree(professor_orientador) | 20.000 | 127 | 31 | 63 | 2 | 62 | 500 | — | 0 | — | — | — |
| aluno_heap:sec_btree(cr) | 20.000 | 127 | 31 | 155 | 2 | 61 | 571 | — | 93 | — | — | — |
| aluno_heap:sec_hash(dre) | 20.000 | 127 | 31 | 101 | — | — | — | 101 | 0 | 0,78 | 1,00 | 1 |
| aluno_heap:sec_btree(curso) | 20.000 | 127 | 31 | 237 | 2 | 36 | 200 | — | 200 | — | — | — |
| curso_bt_codigo:btree | 200 | 122 | 32 | 9 | 2 | 9 | 200 | — | — | — | — | — |
| curso_h_codigo:hash | 200 | 122 | 32 | 11 | — | — | — | 11 | 0 | 0,57 | 1,00 | 1 |
| curso_heap:heap | 200 | 122 | 32 | 7 | — | — | — | — | — | — | — | — |
| curso_heap:sec_hash(codigo) | 200 | 122 | 32 | 2 | — | — | — | 2 | 0 | 0,39 | 1,00 | 1 |
| detalhes_heap:heap | 20.000 | 256 | 15 | 542 | — | — | — | — | — | — | — | — |
| disciplina_bt_curso:btree | 800 | 52 | 72 | 16 | 2 | 16 | 200 | — | — | — | — | — |
| disciplina_h_curso:hash | 800 | 52 | 72 | 17 | — | — | — | 17 | 0 | 0,65 | 1,00 | 1 |
| disciplina_heap:heap | 800 | 52 | 72 | 12 | — | — | — | — | — | — | — | — |
| disciplina_heap:sec_btree(curso) | 800 | 52 | 72 | 5 | 2 | 4 | 200 | — | 0 | — | — | — |
| inscricao_bt_dre:btree | 100.000 | 64 | 59 | 2.439 | 3 | 2.439 | 20.000 | — | — | — | — | — |
| inscricao_h_dre:hash | 100.000 | 64 | 59 | 2.624 | — | — | — | 2.129 | 495 | 0,80 | 1,23 | 2 |
| inscricao_heap:heap | 100.000 | 64 | 59 | 1.695 | — | — | — | — | — | — | — | — |
| inscricao_heap:sec_btree(dre) | 100.000 | 64 | 59 | 441 | 2 | 440 | 20.000 | — | 0 | — | — | — |
| turma_heap:heap | 6.000 | 24 | 145 | 42 | — | — | — | — | — | — | — | — |
| turma_heap:sec_btree(professor) | 6.000 | 24 | 145 | 19 | 2 | 18 | 500 | — | 0 | — | — | — |

## Ocupação antes e depois da carga de manutenção

Blocos por estrutura. Só as estruturas que mudaram; as outras 20 ficaram iguais (tabelas fora da carga de manutenção).

| Estrutura | Dados antes | Dados depois | Overflow antes | Overflow depois | Total antes | Total depois | Δ |
|---|---:|---:|---:|---:|---:|---:|---:|
| aluno_bt_curso:btree | 952 | 955 | 0 | 0 | 958 | 961 | +0,3% |
| aluno_bt_dre:btree | 952 | 1.015 | 0 | 0 | 958 | 1.021 | +6,6% |
| aluno_h_curso:hash | 211 | 211 | 580 | 616 | 792 | 828 | +4,5% |
| aluno_h_dre:hash | 809 | 809 | 87 | 125 | 897 | 935 | +4,2% |
| aluno_heap:heap | 646 | 678 | 0 | 0 | 647 | 679 | +4,9% |
| aluno_heap:sec_btree(professor_orientador) | 62 | 63 | 0 | 0 | 64 | 65 | +1,6% |
| aluno_heap:sec_btree(cr) | 61 | 62 | 93 | 117 | 156 | 181 | +16,0% |

## Células que não rodam

Células da matriz do enunciado que aparecem no resultados.csv sem execução, com o motivo (decisão 170).

| Status | Caso | Linhas por política | Operações | Motivo |
|---|---|---:|---|---|
| nao_aplicavel | caso 1 | 54 | j_unique, j_not_unique | o tipo exige campos que não ditam a organização primária, e a configuração exige que um ou os dois campos a ditem (§5.1; 170) |
| nao_aplicavel | caso 2 | 2 | sel_rg_dre_baixa, sel_rg_dre_alta | o §4.2 só admite hash como secundário de campo UNIQUE (heap) e só clustering NOT UNIQUE nas outras organizações; hash não responde a faixa (170) |
| nao_aplicavel | caso 3 | 6 | sel_igualdade_unique | igualdade em UNIQUE devolve 1 registro (s = 1/N); medida no nível pontual (170) |
| nao_aplicavel | caso 4 | 4 | sel_rg_dre_baixa, sel_rg_curso_baixa, sel_rg_dre_alta, sel_rg_curso_alta | faixa no hash primário, que não preserva a ordem das chaves (128, 170) |

## Acesso executado nas junções

O que cada algoritmo lê de fato em cada entrada (externa · interna), tirado do plano. BNL, sort-merge e partition-hash leem a organização primária: um índice secundário não clustering nunca é usado, então as configurações que diferem só por ele medem o mesmo acesso (P09).

| Junção | Config. | Entradas | BNL | Sort-merge | Hash |
|---|---|---|---|---|---|
| Aluno ⋈ Inscrição (dre) | a | aluno_heap:scan × inscricao_heap:scan | varredura do heap · varredura do heap | varredura do heap + ordenação em runs · varredura do heap + ordenação em runs | varredura do heap + particionamento · varredura do heap + particionamento |
| Aluno ⋈ Inscrição (dre) | b | aluno_bt_dre:primary_btree × inscricao_heap:scan | folhas da B+ · varredura do heap | varredura do heap + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+ + particionamento · varredura do heap + particionamento |
| Aluno ⋈ Inscrição (dre) | c | aluno_bt_dre:primary_btree × inscricao_bt_dre:primary_btree | folhas da B+ · folhas da B+ | folhas da B+ lidas em ordem (sem ordenar) · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+ + particionamento · folhas da B+ + particionamento |
| Aluno ⋈ Inscrição (dre) | c | aluno_h_dre:primary_hash × inscricao_h_dre:primary_hash | buckets do hash · buckets do hash | buckets do hash + ordenação em runs · buckets do hash + ordenação em runs | buckets do hash + particionamento · buckets do hash usados como partições (sem particionar) |
| Aluno ⋈ Inscrição (dre) | d | aluno_bt_dre:primary_btree × inscricao_heap:secondary_btree | folhas da B+ · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+ + particionamento · varredura do heap (índice secundário não usado) + particionamento |
| Aluno ⋈ Inscrição (dre) | e | aluno_heap:secondary_hash × inscricao_heap:secondary_btree | varredura do heap (índice secundário não usado) · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · varredura do heap (índice secundário não usado) + ordenação em runs | varredura do heap (índice secundário não usado) + particionamento · varredura do heap (índice secundário não usado) + particionamento |
| Curso ⋈ Disciplina (codigo = curso) | a | curso_heap:scan × disciplina_heap:scan | varredura do heap · varredura do heap | varredura do heap + ordenação em runs · varredura do heap + ordenação em runs | varredura do heap, sem particionar (cabe em memória) · varredura do heap, sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | b | curso_bt_codigo:primary_btree × disciplina_heap:scan | folhas da B+ · varredura do heap | varredura do heap + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · varredura do heap, sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | c | curso_bt_codigo:primary_btree × disciplina_bt_curso:primary_btree | folhas da B+ · folhas da B+ | folhas da B+ lidas em ordem (sem ordenar) · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · folhas da B+, sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | c | curso_h_codigo:primary_hash × disciplina_h_curso:primary_hash | buckets do hash · buckets do hash | buckets do hash + ordenação em runs · buckets do hash + ordenação em runs | buckets do hash, sem particionar (cabe em memória) · buckets do hash, sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | d | curso_bt_codigo:primary_btree × disciplina_heap:secondary_btree | folhas da B+ · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | e | curso_heap:secondary_hash × disciplina_heap:secondary_btree | varredura do heap (índice secundário não usado) · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · varredura do heap (índice secundário não usado) + ordenação em runs | varredura do heap (índice secundário não usado), sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | a | curso_heap:scan × aluno_heap:scan | varredura do heap · varredura do heap | varredura do heap + ordenação em runs · varredura do heap + ordenação em runs | varredura do heap, sem particionar (cabe em memória) · varredura do heap, sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | b | curso_heap:scan × aluno_bt_curso:primary_btree | varredura do heap · folhas da B+ | folhas da B+ lidas em ordem (sem ordenar) · varredura do heap + ordenação em runs | varredura do heap, sem particionar (cabe em memória) · folhas da B+, sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | c | curso_bt_codigo:primary_btree × aluno_bt_curso:primary_btree | folhas da B+ · folhas da B+ | folhas da B+ lidas em ordem (sem ordenar) · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · folhas da B+, sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | c | curso_h_codigo:primary_hash × aluno_h_curso:primary_hash | buckets do hash · buckets do hash | buckets do hash + ordenação em runs · buckets do hash + ordenação em runs | buckets do hash, sem particionar (cabe em memória) · buckets do hash, sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | d | curso_bt_codigo:primary_btree × aluno_heap:secondary_btree | folhas da B+ · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | e | curso_heap:secondary_hash × aluno_heap:secondary_btree | varredura do heap (índice secundário não usado) · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · varredura do heap (índice secundário não usado) + ordenação em runs | varredura do heap (índice secundário não usado), sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |
| Aluno ⋈ Detalhes (cpf) | a | aluno_heap:scan × detalhes_heap:scan | varredura do heap · varredura do heap | varredura do heap + ordenação em runs · varredura do heap + ordenação em runs | varredura do heap + particionamento · varredura do heap + particionamento |
| Aluno ⋈ Turma (professor_orientador = professor) | e | aluno_heap:secondary_btree × turma_heap:secondary_btree | varredura do heap (índice secundário não usado) · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · varredura do heap (índice secundário não usado) + ordenação em runs | varredura do heap (índice secundário não usado) + particionamento · varredura do heap (índice secundário não usado) + particionamento |

## Convergência das repetições

168 grupos executados: 166 convergiram (E/S e tempo abaixo de 0.05) e 2 pararam no teto de repetições sem convergir (2 só no tempo, com a E/S estável). A dispersão é (máx − mín)/média das últimas repetições. Na seleção pontual, o lote muda a cada repetição, então a E/S também varia. Só os grupos que não convergiram aparecem abaixo.

| Política | Consulta | Variante | Algoritmo | Repetições | Dispersão E/S | Dispersão tempo | Convergiu E/S | Convergiu tempo |
|---|---|---|---|---:|---:|---:|---|---|
| LRU | Aluno ⋈ Detalhes (cpf) | aluno_heap:scan x detalhes_heap:scan | hash | 10 | 0,0% | 10,0% | sim | nao |
| MRU | Aluno ⋈ Detalhes (cpf) | aluno_heap:scan x detalhes_heap:scan | hash | 10 | 0,0% | 5,4% | sim | nao |


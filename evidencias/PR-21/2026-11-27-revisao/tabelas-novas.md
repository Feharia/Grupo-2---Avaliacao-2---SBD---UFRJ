# Tabelas da rodada (LRU)

Geradas por `tools/tabelas.py` a partir de `smoke/`. A métrica é o número de blocos transferidos (leituras + gravações); o tempo é só complementar.

Índice: [Resumo da rodada](#resumo-da-rodada) · [Cobertura da matriz do enunciado](#cobertura-da-matriz-do-enunciado) · [Seleção por igualdade em campo UNIQUE (lote de chaves)](#seleção-por-igualdade-em-campo-unique-lote-de-chaves) · [Seleção por faixa](#seleção-por-faixa) · [Mesma seleção, organizações e caminhos diferentes](#mesma-seleção-organizações-e-caminhos-diferentes) · [Junções: custo de cada algoritmo](#junções-custo-de-cada-algoritmo) · [Melhor algoritmo por situação](#melhor-algoritmo-por-situação) · [Escolha da externa e da interna](#escolha-da-externa-e-da-interna) · [Política de substituição: LRU × MRU](#política-de-substituição-lru--mru) · [Custo estimado × medido](#custo-estimado--medido) · [Fases do sort-merge e do hash join](#fases-do-sort-merge-e-do-hash-join) · [Estruturas construídas](#estruturas-construídas) · [Ocupação antes e depois da carga de manutenção](#ocupação-antes-e-depois-da-carga-de-manutenção) · [Células que não rodam](#células-que-não-rodam) · [Acesso executado nas junções](#acesso-executado-nas-junções) · [Convergência das repetições](#convergência-das-repetições)

## Resumo da rodada

| Item | Valor |
|---|---|
| Escala | 0.1 |
| Bloco | 4096 B (extent de 8 blocos) |
| Políticas comparadas | LRU, MRU |
| Repetições por combinação | 1..1 (tolerância 0.05) |
| M das junções / das seleções | 32 / 2 molduras |
| Chaves por lote de seleção pontual | 100 |
| Combinações medidas (por política) | 84 |
| Linhas medidas no CSV (todas as repetições e políticas) | 168 |
| Células `nao_aplicavel` (caso 1) por política | 54 |
| Células `nao_aplicavel` (caso 2) por política | 2 |
| Células `nao_aplicavel` (caso 3) por política | 6 |
| Células `nao_aplicavel` (caso 4) por política | 4 |
| Duração da rodada | 5.0 s |

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
| equijoin NOT UNIQUE (opcional) | menor ⋈ maior, config. e | ok | ok | ok |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. a | não implementada | não implementada | não implementada |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. b | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. c | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. d | n/a (caso 1) | n/a (caso 1) | n/a (caso 1) |
| equijoin NOT UNIQUE (opcional) | maior ⋈ maior, config. e | não implementada | não implementada | não implementada |
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

Cada execução busca um lote de 100 chaves sorteadas. O lote muda a cada repetição, então as contagens variam um pouco: a tabela mostra a mediana e, em “Var. reps”, a amplitude entre repetições. “Leituras/chave” divide as leituras pelo lote. “× melhor” compara com o caminho de menor custo na mesma consulta.

| Consulta | Config. | Caminho | Linhas | Leituras | Leituras/chave | Aleatórias | Taxa de acerto | Estimado | Erro est. | Tempo (ms) | Var. reps | × melhor |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| igualdade em UNIQUE: cpf | a | heap, varredura | 100 | 3.037 | 30,37 | 94 | 0,00 | 3.250 | +7,0% | 7,6 | 0,0% | 15,6× |
| igualdade em UNIQUE: cpf | c | heap + hash secundário (cpf) | 100 | 195 | 1,95 | 195 | 0,03 | 200 | +2,6% | 0,9 | 0,0% | melhor |
| igualdade em UNIQUE: dre | a | heap, varredura | 100 | 3.344 | 33,44 | 98 | 0,00 | 3.250 | -2,8% | 7,2 | 0,0% | 33,8× |
| igualdade em UNIQUE: dre | b | hash primário (dre) | 100 | 99 | 0,99 | 98 | 0,01 | 107 | +8,3% | 0,6 | 0,0% | melhor |
| igualdade em UNIQUE: dre | b | B+ primária (dre) | 100 | 108 | 1,08 | 100 | 0,47 | 200 | +85,2% | 0,9 | 0,0% | 1,09× |

## Seleção por faixa

“Selet.” é o alvo; “Efetiva” é a fração de linhas que a faixa realmente devolve, feita de grupos inteiros de valores iguais (decisão 174). “% seq.” é a fração das leituras que foram sequenciais. “× melhor” compara com o caminho de menor custo na mesma consulta e seletividade.

| Consulta | Selet. | Efetiva | Config. | Caminho | Linhas | Leituras | % seq. | Estimado | Erro est. | Tempo (ms) | × melhor |
|---|---:|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| faixa em UNIQUE: dre | 1% | 1,00% | a | heap, varredura | 20 | 65 | 98,5% | 65 | +0,0% | 0,32 | 21,7× |
| faixa em UNIQUE: dre | 1% | 1,00% | b | B+ primária (dre) | 20 | 3 | 33,3% | 2 | -33,3% | 0,12 | melhor |
| faixa em UNIQUE: dre | 50% | 50,00% | a | heap, varredura | 1.000 | 65 | 98,5% | 65 | +0,0% | 4,93 | 1,30× |
| faixa em UNIQUE: dre | 50% | 50,00% | b | B+ primária (dre) | 1.000 | 50 | 96,0% | 49 | -2,0% | 4,85 | melhor |
| faixa em NOT UNIQUE: cr | 1% | 1,00% | a | heap, varredura | 20 | 65 | 98,5% | 65 | +0,0% | 0,23 | 2,95× |
| faixa em NOT UNIQUE: cr | 1% | 1,00% | c | heap + B+ secundária (cr) | 20 | 22 | 4,5% | 22 | +0,0% | 0,14 | melhor |
| faixa em NOT UNIQUE: cr | 50% | 50,20% | a | heap, varredura | 1.004 | 65 | 98,5% | 65 | +0,0% | 5,48 | melhor |
| faixa em NOT UNIQUE: cr | 50% | 50,20% | c | heap + B+ secundária (cr) | 1.004 | 964 | 9,8% | 1.006 | +4,4% | 5,96 | 14,8× |
| faixa em NOT UNIQUE: curso | 1% | 0,75% | a | heap, varredura | 15 | 65 | 98,5% | 65 | +0,0% | 0,21 | 21,7× |
| faixa em NOT UNIQUE: curso | 1% | 0,75% | b | B+ primária (curso) | 15 | 3 | 33,3% | 2 | -33,3% | 0,09 | melhor |
| faixa em NOT UNIQUE: curso | 1% | 0,75% | c | B+ primária (curso) + B+ clustering (curso) | 15 | 4 | 25,0% | 2 | -50,0% | 0,09 | 1,33× |
| faixa em NOT UNIQUE: curso | 1% | 0,75% | c | hash primário (curso) + B+ clustering (curso) | 15 | 4 | 25,0% | 4 | -7,5% | 0,10 | 1,33× |
| faixa em NOT UNIQUE: curso | 50% | 49,80% | a | heap, varredura | 996 | 65 | 98,5% | 65 | +0,0% | 4,84 | 1,33× |
| faixa em NOT UNIQUE: curso | 50% | 49,80% | b | B+ primária (curso) | 996 | 49 | 95,9% | 49 | +0,0% | 5,67 | melhor |
| faixa em NOT UNIQUE: curso | 50% | 49,80% | c | B+ primária (curso) + B+ clustering (curso) | 996 | 50 | 94,0% | 49 | -2,0% | 5,20 | 1,02× |
| faixa em NOT UNIQUE: curso | 50% | 49,80% | c | hash primário (curso) + B+ clustering (curso) | 996 | 158 | 0,6% | 136 | -14,0% | 4,95 | 3,22× |

## Mesma seleção, organizações e caminhos diferentes

Blocos transferidos (LRU). Em negrito, o menor custo de cada linha. “n/a” é uma célula que não se aplica (decisão 170); vazio é uma combinação que a bancada não monta para essa consulta.

| Consulta | Selet. | Heap (varredura) | B+ primária | Hash primário | Heap + hash sec. | Heap + B+ sec. | B+ prim. + clustering | Hash prim. + clustering |
|---|---|---:|---:|---|---:|---:|---:|---:|
| igualdade em UNIQUE: cpf | pontual | 3.037 |  |  | **195** |  |  |  |
| igualdade em UNIQUE: dre | pontual | 3.344 | 108 | **99** |  |  |  |  |
| faixa em UNIQUE: dre | 1% | 65 | **3** | n/a (caso 4) |  |  |  |  |
| faixa em UNIQUE: dre | 50% | 65 | **50** | n/a (caso 4) |  |  |  |  |
| faixa em NOT UNIQUE: cr | 1% | 65 |  |  |  | **22** |  |  |
| faixa em NOT UNIQUE: cr | 50% | **65** |  |  |  | 964 |  |  |
| faixa em NOT UNIQUE: curso | 1% | 65 | **3** | n/a (caso 4) |  |  | 4 | 4 |
| faixa em NOT UNIQUE: curso | 50% | 65 | **49** | n/a (caso 4) |  |  | 50 | 158 |

## Junções: custo de cada algoritmo

M = 32 molduras. A primeira tabela de “Externa → interna” é a externa no BNL, a de build no hash e a primeira entrada do merge. “× melhor” compara com a opção mais barata para as mesmas entradas (todos os algoritmos e as duas ordens).

| Junção | Tamanho | Config. | Externa → interna | Algoritmo | Leituras | Gravações | Total | Estimado | Erro est. | Tempo (ms) | × melhor |
|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | BNL | 915 | 0 | 915 | 915 | +0,0% | 81,6 | 1,29× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | BNL | 885 | 0 | 885 | 885 | +0,0% | 79,8 | 1,25× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | sort-merge | 472 | 237 | 709 | 705 | -0,6% | 85,0 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | sort-merge | 472 | 237 | 709 | 705 | -0,6% | 85,3 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | hash | 472 | 237 | 709 | 717 | +1,1% | 80,5 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | hash | 479 | 244 | 723 | 737 | +1,9% | 80,0 | 1,02× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | BNL | 1.115 | 0 | 1.115 | 1.115 | +0,0% | 87,4 | 1,83× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Inscrição: heap, varredura → Aluno: B+ primária (dre) | sort-merge | 437 | 171 | 608 | 605 | -0,5% | 82,5 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | hash | 502 | 237 | 739 | 747 | +1,1% | 72,3 | 1,22× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | BNL | 1.559 | 0 | 1.559 | 1.559 | +0,0% | 81,3 | 4,57× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Inscrição: B+ primária (dre) → Aluno: B+ primária (dre) | sort-merge | 341 | 0 | 341 | 339 | -0,6% | 73,6 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | hash | 576 | 237 | 813 | 821 | +1,0% | 73,2 | 2,38× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | BNL | 1.691 | 0 | 1.691 | 1.691 | +0,0% | 82,0 | 3,47× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Inscrição: hash primário (dre) → Aluno: hash primário (dre) | sort-merge | 593 | 237 | 830 | 826 | -0,5% | 79,8 | 1,70× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | hash | 422 | 66 | 488 | 498 | +2,0% | 75,7 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | BNL | 1.115 | 0 | 1.115 | 1.115 | +0,0% | 84,5 | 1,83× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Inscrição: heap + B+ secundária (dre) → Aluno: B+ primária (dre) | sort-merge | 437 | 171 | 608 | 605 | -0,5% | 83,2 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | hash | 502 | 237 | 739 | 747 | +1,1% | 79,0 | 1,22× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | e | Aluno: heap + hash secundário (dre) → Inscrição: heap + B+ secundária (dre) | BNL | 915 | 0 | 915 | 915 | +0,0% | 87,5 | 1,29× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | e | Inscrição: heap + B+ secundária (dre) → Aluno: heap + hash secundário (dre) | sort-merge | 472 | 237 | 709 | 705 | -0,6% | 83,5 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | e | Aluno: heap + hash secundário (dre) → Inscrição: heap + B+ secundária (dre) | hash | 472 | 237 | 709 | 717 | +1,1% | 78,9 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Curso: heap, varredura → Disciplina: heap, varredura | BNL | 19 | 0 | 19 | 19 | +0,0% | 3,8 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Disciplina: heap, varredura → Curso: heap, varredura | sort-merge | 38 | 19 | 57 | 57 | +0,0% | 5,9 | 3,00× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Curso: heap, varredura → Disciplina: heap, varredura | hash | 19 | 0 | 19 | 19 | +0,0% | 3,7 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | b | Curso: B+ primária (codigo) → Disciplina: heap, varredura | BNL | 21 | 0 | 21 | 21 | +0,0% | 3,8 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | b | Disciplina: heap, varredura → Curso: B+ primária (codigo) | sort-merge | 34 | 12 | 46 | 45 | -2,2% | 5,0 | 2,19× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | b | Curso: B+ primária (codigo) → Disciplina: heap, varredura | hash | 21 | 0 | 21 | 21 | +0,0% | 4,5 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Curso: B+ primária (codigo) → Disciplina: B+ primária (curso) | BNL | 25 | 0 | 25 | 25 | +0,0% | 3,7 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Disciplina: B+ primária (curso) → Curso: B+ primária (codigo) | sort-merge | 27 | 0 | 27 | 25 | -7,4% | 3,7 | 1,08× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Curso: B+ primária (codigo) → Disciplina: B+ primária (curso) | hash | 25 | 0 | 25 | 25 | +0,0% | 4,1 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Curso: hash primário (codigo) → Disciplina: hash primário (curso) | BNL | 28 | 0 | 28 | 28 | +0,0% | 4,8 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Disciplina: hash primário (curso) → Curso: hash primário (codigo) | sort-merge | 47 | 19 | 66 | 66 | +0,0% | 4,3 | 2,36× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | c | Curso: hash primário (codigo) → Disciplina: hash primário (curso) | hash | 28 | 0 | 28 | 28 | +0,0% | 3,7 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | d | Curso: B+ primária (codigo) → Disciplina: heap + B+ secundária (curso) | BNL | 21 | 0 | 21 | 21 | +0,0% | 4,5 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | d | Disciplina: heap + B+ secundária (curso) → Curso: B+ primária (codigo) | sort-merge | 34 | 12 | 46 | 45 | -2,2% | 4,8 | 2,19× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | d | Curso: B+ primária (codigo) → Disciplina: heap + B+ secundária (curso) | hash | 21 | 0 | 21 | 21 | +0,0% | 3,7 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | e | Curso: heap + hash secundário (codigo) → Disciplina: heap + B+ secundária (curso) | BNL | 19 | 0 | 19 | 19 | +0,0% | 3,8 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | e | Disciplina: heap + B+ secundária (curso) → Curso: heap + hash secundário (codigo) | sort-merge | 38 | 19 | 57 | 57 | +0,0% | 5,8 | 3,00× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | e | Curso: heap + hash secundário (codigo) → Disciplina: heap + B+ secundária (curso) | hash | 19 | 0 | 19 | 19 | +0,0% | 3,7 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Curso: heap, varredura → Aluno: heap, varredura | BNL | 72 | 0 | 72 | 72 | +0,0% | 16,4 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Aluno: heap, varredura → Curso: heap, varredura | sort-merge | 145 | 73 | 218 | 216 | -0,9% | 17,6 | 3,03× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Curso: heap, varredura → Aluno: heap, varredura | hash | 72 | 0 | 72 | 72 | +0,0% | 16,2 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Curso: heap, varredura → Aluno: B+ primária (curso) | BNL | 102 | 0 | 102 | 102 | +0,0% | 16,3 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Aluno: B+ primária (curso) → Curso: heap, varredura | sort-merge | 110 | 7 | 117 | 116 | -0,9% | 16,4 | 1,15× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Curso: heap, varredura → Aluno: B+ primária (curso) | hash | 102 | 0 | 102 | 102 | +0,0% | 16,3 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Curso: B+ primária (codigo) → Aluno: B+ primária (curso) | BNL | 104 | 0 | 104 | 104 | +0,0% | 16,4 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Aluno: B+ primária (curso) → Curso: B+ primária (codigo) | sort-merge | 106 | 0 | 106 | 104 | -1,9% | 16,2 | 1,02× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Curso: B+ primária (codigo) → Aluno: B+ primária (curso) | hash | 104 | 0 | 104 | 104 | +0,0% | 16,2 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Curso: hash primário (codigo) → Aluno: hash primário (curso) | BNL | 123 | 0 | 123 | 123 | +0,0% | 16,4 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Aluno: hash primário (curso) → Curso: hash primário (codigo) | sort-merge | 196 | 73 | 269 | 267 | -0,7% | 17,7 | 2,19× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | c | Curso: hash primário (codigo) → Aluno: hash primário (curso) | hash | 123 | 0 | 123 | 123 | +0,0% | 16,3 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | d | Curso: B+ primária (codigo) → Aluno: heap + B+ secundária (curso) | BNL | 74 | 0 | 74 | 74 | +0,0% | 16,4 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | d | Aluno: heap + B+ secundária (curso) → Curso: B+ primária (codigo) | sort-merge | 141 | 66 | 207 | 204 | -1,4% | 19,0 | 2,80× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | d | Curso: B+ primária (codigo) → Aluno: heap + B+ secundária (curso) | hash | 74 | 0 | 74 | 74 | +0,0% | 16,2 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | e | Curso: heap + hash secundário (codigo) → Aluno: heap + B+ secundária (curso) | BNL | 72 | 0 | 72 | 72 | +0,0% | 16,4 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | e | Aluno: heap + B+ secundária (curso) → Curso: heap + hash secundário (codigo) | sort-merge | 145 | 73 | 218 | 216 | -0,9% | 17,5 | 3,03× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | e | Curso: heap + hash secundário (codigo) → Aluno: heap + B+ secundária (curso) | hash | 72 | 0 | 72 | 72 | +0,0% | 16,4 | melhor |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | BNL | 314 | 0 | 314 | 314 | +0,0% | 21,4 | melhor |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | sort-merge | 240 | 121 | 361 | 359 | -0,6% | 22,8 | 1,15× |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | hash | 243 | 124 | 367 | 383 | +4,4% | 19,4 | 1,17× |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | menor ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | BNL | 70 | 0 | 70 | 70 | +0,0% | 16,3 | melhor |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | menor ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | sort-merge | 135 | 65 | 200 | 210 | +5,0% | 17,8 | 2,86× |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | menor ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | hash | 70 | 0 | 70 | 70 | +0,0% | 16,0 | melhor |

## Melhor algoritmo por situação

Para cada célula (tipo, tamanho, configuração), o total de blocos de cada algoritmo na melhor ordem externa/interna. “Vantagem” é quantas vezes o próximo algoritmo mais barato custa em relação ao vencedor (empates contam juntos).

| Junção | Tipo | Tamanho | Config. | Entradas | BNL | Sort-merge | Hash | Vencedor | Vantagem |
|---|---|---|---|---|---:|---:|---:|---|---:|
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | a | aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan | 885 | **709** | **709** | sort-merge = hash (empate) | 1,25× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) ⋈ Inscrição: heap, varredura | 1.115 | **608** | 739 | sort-merge | 1,22× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) ⋈ Inscrição: B+ primária (dre) | 1.559 | **341** | 813 | sort-merge | 2,38× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | c | Aluno: hash primário (dre) ⋈ Inscrição: hash primário (dre) | 1.691 | 830 | **488** | hash | 1,70× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) ⋈ Inscrição: heap + B+ secundária (dre) | 1.115 | **608** | 739 | sort-merge | 1,22× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | e | Aluno: heap + hash secundário (dre) ⋈ Inscrição: heap + B+ secundária (dre) | 915 | **709** | **709** | sort-merge = hash (empate) | 1,29× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | a | Curso: heap, varredura ⋈ Disciplina: heap, varredura | **19** | 57 | **19** | BNL = hash (empate) | 3,00× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | b | Curso: B+ primária (codigo) ⋈ Disciplina: heap, varredura | **21** | 46 | **21** | BNL = hash (empate) | 2,19× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | c | Curso: B+ primária (codigo) ⋈ Disciplina: B+ primária (curso) | **25** | 27 | **25** | BNL = hash (empate) | 1,08× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | c | Curso: hash primário (codigo) ⋈ Disciplina: hash primário (curso) | **28** | 66 | **28** | BNL = hash (empate) | 2,36× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | d | Curso: B+ primária (codigo) ⋈ Disciplina: heap + B+ secundária (curso) | **21** | 46 | **21** | BNL = hash (empate) | 2,19× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | e | Curso: heap + hash secundário (codigo) ⋈ Disciplina: heap + B+ secundária (curso) | **19** | 57 | **19** | BNL = hash (empate) | 3,00× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | a | Curso: heap, varredura ⋈ Aluno: heap, varredura | **72** | 218 | **72** | BNL = hash (empate) | 3,03× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | b | Curso: heap, varredura ⋈ Aluno: B+ primária (curso) | **102** | 117 | **102** | BNL = hash (empate) | 1,15× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | c | Curso: B+ primária (codigo) ⋈ Aluno: B+ primária (curso) | **104** | 106 | **104** | BNL = hash (empate) | 1,02× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | c | Curso: hash primário (codigo) ⋈ Aluno: hash primário (curso) | **123** | 269 | **123** | BNL = hash (empate) | 2,19× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | d | Curso: B+ primária (codigo) ⋈ Aluno: heap + B+ secundária (curso) | **74** | 207 | **74** | BNL = hash (empate) | 2,80× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | e | Curso: heap + hash secundário (codigo) ⋈ Aluno: heap + B+ secundária (curso) | **72** | 218 | **72** | BNL = hash (empate) | 3,03× |
| Aluno ⋈ Detalhes (cpf) | equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura ⋈ Aluno: heap, varredura | **314** | 361 | 367 | BNL | 1,15× |
| Aluno ⋈ Turma (professor_orientador = professor) | equijoin NOT UNIQUE | menor ⋈ maior | e | Turma: heap + B+ secundária (professor) ⋈ Aluno: heap + B+ secundária (professor_orientador) | **70** | 200 | **70** | BNL = hash (empate) | 2,86× |

## Escolha da externa e da interna

A ordem padrão segue a decisão 122: no BNL e no hash, a menor por fora (build); no sort-merge, a UNIQUE por dentro. “Δ” é o custo da ordem trocada em relação à padrão.

| Junção | Config. | Algoritmo | Ordem padrão | Total | Ordem trocada | Total | Δ |
|---|---|---|---|---:|---|---:|---:|
| Aluno ⋈ Inscrição (dre) | a | BNL | Aluno: heap, varredura → Inscrição: heap, varredura | 915 | Inscrição: heap, varredura → Aluno: heap, varredura | 885 | -3,3% |
| Aluno ⋈ Inscrição (dre) | a | sort-merge | Inscrição: heap, varredura → Aluno: heap, varredura | 709 | Aluno: heap, varredura → Inscrição: heap, varredura | 709 | +0,0% |
| Aluno ⋈ Inscrição (dre) | a | hash | Aluno: heap, varredura → Inscrição: heap, varredura | 709 | Inscrição: heap, varredura → Aluno: heap, varredura | 723 | +2,0% |

## Política de substituição: LRU × MRU

Só as combinações em que o total de blocos muda. Nas outras 73, as duas políticas transferem o mesmo número de blocos.

| Consulta | Selet./tamanho | Onde | LRU | MRU | Δ | Acerto LRU | Acerto MRU |
|---|---|---|---:|---:|---:|---:|---:|
| igualdade em UNIQUE: dre | pontual | config. b: B+ primária (dre) | 108 | 202 | +87,0% | 0,47 | 0,01 |
| Aluno ⋈ Detalhes (cpf) | maior ⋈ maior | BNL, config. a: Detalhes: heap, varredura → Aluno: heap, varredura | 314 | 278 | -11,5% | 0,00 | 0,11 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. a: Inscrição: heap, varredura → Aluno: heap, varredura | 885 | 785 | -11,3% | 0,00 | 0,11 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. b: Aluno: B+ primária (dre) → Inscrição: heap, varredura | 1.115 | 1.050 | -5,8% | 0,00 | 0,06 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. d: Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | 1.115 | 1.050 | -5,8% | 0,00 | 0,06 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. a: Aluno: heap, varredura → Inscrição: heap, varredura | 915 | 863 | -5,7% | 0,00 | 0,06 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. e: Aluno: heap + hash secundário (dre) → Inscrição: heap + B+ secundária (dre) | 915 | 863 | -5,7% | 0,00 | 0,06 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. c: Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | 1.559 | 1.494 | -4,2% | 0,00 | 0,04 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. c: Aluno: hash primário (dre) → Inscrição: hash primário (dre) | 1.691 | 1.626 | -3,8% | 0,00 | 0,04 |
| igualdade em UNIQUE: dre | pontual | config. a: heap, varredura | 3.344 | 3.244 | -3,0% | 0,00 | 0,03 |
| igualdade em UNIQUE: cpf | pontual | config. a: heap, varredura | 3.037 | 2.982 | -1,8% | 0,00 | 0,02 |

## Custo estimado × medido

Erro = (estimado − medido) / medido, em blocos transferidos. As fórmulas são as dos livros (Elmasri e Silberschatz), com os parâmetros medidos pela própria bancada. A última coluna traz os parâmetros que entraram na fórmula da pior combinação; os de todas estão em parametros.csv.

| Família | Combinações | Mediana do erro absoluto | Erro absoluto ≤ 10% | Maior erro | Onde | Parâmetros da pior |
|---|---:|---:|---|---:|---|---|
| junção: BNL | 21 | 0,0% | 21 de 21 | +0,0% | Aluno ⋈ Inscrição (dre) (config. a: Aluno: heap, varredura → Inscrição: heap, varredura) | m=32; b_ext=65; b_int=170; d_ext=65; d_int=170; chunk=16; dir_frames=3 |
| junção: hash | 21 | 0,0% | 21 de 21 | +4,4% | Aluno ⋈ Detalhes (cpf) (config. a: Detalhes: heap, varredura → Aluno: heap, varredura) | m=32; b_ext=54; b_int=65; d_ext=55; d_int=65; particoes=6; niveis=1; build_pages=31; pre_ext=nao; pre_int=nao |
| junção: sort-merge | 21 | 0,6% | 21 de 21 | -7,4% | Curso ⋈ Disciplina (codigo = curso) (config. c: Disciplina: B+ primária (curso) → Curso: B+ primária (codigo)) | m=32; b_ext=16; b_int=9; d_ext=12; d_int=7; ordena_ext=nao; ordena_int=nao; run_pages=30; runs_ext=1; runs_int=1; fan_in=31; passes_ext=0; passes_int=0; group_pages=0; desc_frames=0; final_fan_in=32; ordered_extra_frames=0 |
| seleção: faixa | 16 | 1,0% | 12 de 16 | -50,0% | faixa em NOT UNIQUE: curso (config. c: B+ primária (curso) + B+ clustering (curso)) | n=2000; b=95; bfr=31; bfr_medio=31; lote=1; linhas_esperadas=15; h=2; distintos=200 |
| seleção: igualdade (lote) | 5 | 7,0% | 4 de 5 | +85,2% | igualdade em UNIQUE: dre (config. b: B+ primária (dre)) | n=2000; b=95; bfr=31; bfr_medio=31; lote=100; linhas_esperadas=100; h=2; distintos=2000 |

## Fases do sort-merge e do hash join

Leituras/gravações de cada fase (1ª repetição). No hash join, o resumo traz os passes de particionamento recursivo e os bailouts (decisão 171).

| Junção | Config. | Externa → interna | Algoritmo | Fases (leituras/gravações) | Recursão |
|---|---|---|---|---|---|
| Aluno ⋈ Inscrição (dre) | a | Inscrição: heap, varredura → Aluno: heap, varredura | sort-merge | runs externa 170/171 · runs interna 65/66 · passes 0/0 · intercalação final 237/0 | — |
| Aluno ⋈ Inscrição (dre) | a | Aluno: heap, varredura → Inscrição: heap, varredura | sort-merge | runs externa 65/66 · runs interna 170/171 · passes 0/0 · intercalação final 237/0 | — |
| Aluno ⋈ Inscrição (dre) | a | Aluno: heap, varredura → Inscrição: heap, varredura | hash | particiona externa 65/66 · particiona interna 170/171 · build probe 237/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | a | Inscrição: heap, varredura → Aluno: heap, varredura | hash | particiona externa 170/174 · particiona interna 65/70 · build probe 244/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | b | Inscrição: heap, varredura → Aluno: B+ primária (dre) | sort-merge | runs externa 170/171 · passes 0/0 · intercalação final 267/0 | — |
| Aluno ⋈ Inscrição (dre) | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | hash | particiona externa 95/66 · particiona interna 170/171 · build probe 237/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | c | Inscrição: B+ primária (dre) → Aluno: B+ primária (dre) | sort-merge | passes 0/0 · intercalação final 341/0 | — |
| Aluno ⋈ Inscrição (dre) | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | hash | particiona externa 95/66 · particiona interna 244/171 · build probe 237/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | c | Inscrição: hash primário (dre) → Aluno: hash primário (dre) | sort-merge | runs externa 267/171 · runs interna 89/66 · passes 0/0 · intercalação final 237/0 | — |
| Aluno ⋈ Inscrição (dre) | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | hash | particiona externa 89/66 · build probe 333/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | d | Inscrição: heap + B+ secundária (dre) → Aluno: B+ primária (dre) | sort-merge | runs externa 170/171 · passes 0/0 · intercalação final 267/0 | — |
| Aluno ⋈ Inscrição (dre) | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | hash | particiona externa 95/66 · particiona interna 170/171 · build probe 237/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | e | Inscrição: heap + B+ secundária (dre) → Aluno: heap + hash secundário (dre) | sort-merge | runs externa 170/171 · runs interna 65/66 · passes 0/0 · intercalação final 237/0 | — |
| Aluno ⋈ Inscrição (dre) | e | Aluno: heap + hash secundário (dre) → Inscrição: heap + B+ secundária (dre) | hash | particiona externa 65/66 · particiona interna 170/171 · build probe 237/0 | passes recursivos=0 bailouts=0 |
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
| Curso ⋈ Aluno (codigo = curso) | a | Aluno: heap, varredura → Curso: heap, varredura | sort-merge | runs externa 65/66 · runs interna 7/7 · passes 0/0 · intercalação final 73/0 | — |
| Curso ⋈ Aluno (codigo = curso) | a | Curso: heap, varredura → Aluno: heap, varredura | hash | build probe 72/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | b | Aluno: B+ primária (curso) → Curso: heap, varredura | sort-merge | runs interna 7/7 · passes 0/0 · intercalação final 103/0 | — |
| Curso ⋈ Aluno (codigo = curso) | b | Curso: heap, varredura → Aluno: B+ primária (curso) | hash | build probe 102/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | c | Aluno: B+ primária (curso) → Curso: B+ primária (codigo) | sort-merge | passes 0/0 · intercalação final 106/0 | — |
| Curso ⋈ Aluno (codigo = curso) | c | Curso: B+ primária (codigo) → Aluno: B+ primária (curso) | hash | build probe 104/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | c | Aluno: hash primário (curso) → Curso: hash primário (codigo) | sort-merge | runs externa 112/66 · runs interna 11/7 · passes 0/0 · intercalação final 73/0 | — |
| Curso ⋈ Aluno (codigo = curso) | c | Curso: hash primário (codigo) → Aluno: hash primário (curso) | hash | build probe 123/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | d | Aluno: heap + B+ secundária (curso) → Curso: B+ primária (codigo) | sort-merge | runs externa 65/66 · passes 0/0 · intercalação final 76/0 | — |
| Curso ⋈ Aluno (codigo = curso) | d | Curso: B+ primária (codigo) → Aluno: heap + B+ secundária (curso) | hash | build probe 74/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | e | Aluno: heap + B+ secundária (curso) → Curso: heap + hash secundário (codigo) | sort-merge | runs externa 65/66 · runs interna 7/7 · passes 0/0 · intercalação final 73/0 | — |
| Curso ⋈ Aluno (codigo = curso) | e | Curso: heap + hash secundário (codigo) → Aluno: heap + B+ secundária (curso) | hash | build probe 72/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Detalhes (cpf) | a | Detalhes: heap, varredura → Aluno: heap, varredura | sort-merge | runs externa 54/55 · runs interna 65/66 · passes 0/0 · intercalação final 121/0 | — |
| Aluno ⋈ Detalhes (cpf) | a | Detalhes: heap, varredura → Aluno: heap, varredura | hash | particiona externa 54/57 · particiona interna 65/67 · build probe 124/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Turma (professor_orientador = professor) | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | sort-merge | runs externa 5/5 · runs interna 65/60 · passes 0/0 · intercalação final 65/0 | — |
| Aluno ⋈ Turma (professor_orientador = professor) | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | hash | build probe 70/0 | passes recursivos=0 bailouts=0 |

## Estruturas construídas

Medidas pela própria bancada depois da carga. n = registros, R = bytes por registro, bfr = registros por bloco. Nos índices, “Blocos” conta só o índice.

| Estrutura | n | R | bfr | Blocos | Altura | Folhas | Distintos | Buckets | Overflow | Fator de carga | Cadeia média | Cadeia máx. |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| aluno_bt_curso:btree | 2.000 | 127 | 31 | 95 | 2 | 95 | 200 | — | — | — | — | — |
| aluno_bt_curso:sec_btree_clustering(curso) | 2.000 | 127 | 31 | 3 | 2 | 2 | 200 | — | 0 | — | — | — |
| aluno_bt_dre:btree | 2.000 | 127 | 31 | 95 | 2 | 95 | 2.000 | — | — | — | — | — |
| aluno_h_curso:hash | 2.000 | 127 | 31 | 112 | — | — | — | 83 | 29 | 0,78 | 1,35 | 3 |
| aluno_h_curso:sec_btree_clustering(curso) | 2.000 | 127 | 31 | 3 | 2 | 2 | 200 | — | 0 | — | — | — |
| aluno_h_dre:hash | 2.000 | 127 | 31 | 89 | — | — | — | 83 | 6 | 0,78 | 1,07 | 2 |
| aluno_heap:heap | 2.000 | 127 | 31 | 65 | — | — | — | — | — | — | — | — |
| aluno_heap:sec_hash(cpf) | 2.000 | 127 | 31 | 17 | — | — | — | 17 | 0 | 0,67 | 1,00 | 1 |
| aluno_heap:sec_btree(professor_orientador) | 2.000 | 127 | 31 | 9 | 2 | 8 | 490 | — | 0 | — | — | — |
| aluno_heap:sec_btree(cr) | 2.000 | 127 | 31 | 9 | 2 | 8 | 461 | — | 0 | — | — | — |
| aluno_heap:sec_hash(dre) | 2.000 | 127 | 31 | 11 | — | — | — | 11 | 0 | 0,72 | 1,00 | 1 |
| aluno_heap:sec_btree(curso) | 2.000 | 127 | 31 | 9 | 2 | 8 | 200 | — | 0 | — | — | — |
| aluno_heap_sem_indices:heap | 2.000 | 127 | 31 | 65 | — | — | — | — | — | — | — | — |
| curso_bt_codigo:btree | 200 | 122 | 32 | 9 | 2 | 9 | 200 | — | — | — | — | — |
| curso_h_codigo:hash | 200 | 122 | 32 | 11 | — | — | — | 11 | 0 | 0,57 | 1,00 | 1 |
| curso_heap:heap | 200 | 122 | 32 | 7 | — | — | — | — | — | — | — | — |
| curso_heap:sec_hash(codigo) | 200 | 122 | 32 | 2 | — | — | — | 2 | 0 | 0,39 | 1,00 | 1 |
| curso_heap_sem_indices:heap | 200 | 122 | 32 | 7 | — | — | — | — | — | — | — | — |
| detalhes_heap:heap | 2.000 | 256 | 15 | 54 | — | — | — | — | — | — | — | — |
| disciplina_bt_curso:btree | 800 | 52 | 72 | 16 | 2 | 16 | 200 | — | — | — | — | — |
| disciplina_h_curso:hash | 800 | 52 | 72 | 17 | — | — | — | 17 | 0 | 0,65 | 1,00 | 1 |
| disciplina_heap:heap | 800 | 52 | 72 | 12 | — | — | — | — | — | — | — | — |
| disciplina_heap:sec_btree(curso) | 800 | 52 | 72 | 5 | 2 | 4 | 200 | — | 0 | — | — | — |
| disciplina_heap_sem_indices:heap | 800 | 52 | 72 | 12 | — | — | — | — | — | — | — | — |
| inscricao_bt_dre:btree | 10.000 | 64 | 59 | 244 | 2 | 244 | 2.000 | — | — | — | — | — |
| inscricao_h_dre:hash | 10.000 | 64 | 59 | 267 | — | — | — | 223 | 44 | 0,76 | 1,20 | 2 |
| inscricao_heap:heap | 10.000 | 64 | 59 | 170 | — | — | — | — | — | — | — | — |
| inscricao_heap:sec_btree(dre) | 10.000 | 64 | 59 | 40 | 2 | 39 | 2.000 | — | 0 | — | — | — |
| inscricao_heap_sem_indices:heap | 10.000 | 64 | 59 | 170 | — | — | — | — | — | — | — | — |
| turma_heap:heap | 600 | 24 | 145 | 5 | — | — | — | — | — | — | — | — |
| turma_heap:sec_btree(professor) | 600 | 24 | 145 | 5 | 2 | 4 | 354 | — | 0 | — | — | — |

## Ocupação antes e depois da carga de manutenção

Blocos por estrutura. Total do arquivo inclui a folga reservada quando disponível; saídas históricas usam o total de blocos usados. Só estruturas que mudaram; as outras 26 ficaram iguais.

| Estrutura | Dados antes | Dados depois | Overflow antes | Overflow depois | Total antes | Total depois | Δ |
|---|---:|---:|---:|---:|---:|---:|---:|
| aluno_bt_dre:btree | 95 | 101 | 0 | 0 | 106 | 114 | +7,5% |
| aluno_h_curso:hash | 83 | 83 | 29 | 30 | 113 | 121 | +7,1% |
| aluno_h_dre:hash | 83 | 83 | 6 | 11 | 97 | 97 | +0,0% |
| aluno_heap:heap | 65 | 68 | 0 | 0 | 73 | 73 | +0,0% |
| aluno_heap_sem_indices:heap | 65 | 68 | 0 | 0 | 73 | 73 | +0,0% |

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
| Aluno ⋈ Inscrição (dre) | a | aluno_heap_sem_indices:scan × inscricao_heap_sem_indices:scan | varredura do heap · varredura do heap | varredura do heap + ordenação em runs · varredura do heap + ordenação em runs | varredura do heap + particionamento · varredura do heap + particionamento |
| Aluno ⋈ Inscrição (dre) | b | aluno_bt_dre:primary_btree × inscricao_heap_sem_indices:scan | folhas da B+ · varredura do heap | varredura do heap + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+ + particionamento · varredura do heap + particionamento |
| Aluno ⋈ Inscrição (dre) | c | aluno_bt_dre:primary_btree × inscricao_bt_dre:primary_btree | folhas da B+ · folhas da B+ | folhas da B+ lidas em ordem (sem ordenar) · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+ + particionamento · folhas da B+ + particionamento |
| Aluno ⋈ Inscrição (dre) | c | aluno_h_dre:primary_hash × inscricao_h_dre:primary_hash | buckets do hash · buckets do hash | buckets do hash + ordenação em runs · buckets do hash + ordenação em runs | buckets do hash + particionamento · buckets do hash usados como partições (sem particionar) |
| Aluno ⋈ Inscrição (dre) | d | aluno_bt_dre:primary_btree × inscricao_heap:secondary_btree | folhas da B+ · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+ + particionamento · varredura do heap (índice secundário não usado) + particionamento |
| Aluno ⋈ Inscrição (dre) | e | aluno_heap:secondary_hash × inscricao_heap:secondary_btree | varredura do heap (índice secundário não usado) · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · varredura do heap (índice secundário não usado) + ordenação em runs | varredura do heap (índice secundário não usado) + particionamento · varredura do heap (índice secundário não usado) + particionamento |
| Curso ⋈ Disciplina (codigo = curso) | a | curso_heap_sem_indices:scan × disciplina_heap_sem_indices:scan | varredura do heap · varredura do heap | varredura do heap + ordenação em runs · varredura do heap + ordenação em runs | varredura do heap, sem particionar (cabe em memória) · varredura do heap, sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | b | curso_bt_codigo:primary_btree × disciplina_heap_sem_indices:scan | folhas da B+ · varredura do heap | varredura do heap + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · varredura do heap, sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | c | curso_bt_codigo:primary_btree × disciplina_bt_curso:primary_btree | folhas da B+ · folhas da B+ | folhas da B+ lidas em ordem (sem ordenar) · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · folhas da B+, sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | c | curso_h_codigo:primary_hash × disciplina_h_curso:primary_hash | buckets do hash · buckets do hash | buckets do hash + ordenação em runs · buckets do hash + ordenação em runs | buckets do hash, sem particionar (cabe em memória) · buckets do hash, sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | d | curso_bt_codigo:primary_btree × disciplina_heap:secondary_btree | folhas da B+ · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |
| Curso ⋈ Disciplina (codigo = curso) | e | curso_heap:secondary_hash × disciplina_heap:secondary_btree | varredura do heap (índice secundário não usado) · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · varredura do heap (índice secundário não usado) + ordenação em runs | varredura do heap (índice secundário não usado), sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | a | curso_heap_sem_indices:scan × aluno_heap_sem_indices:scan | varredura do heap · varredura do heap | varredura do heap + ordenação em runs · varredura do heap + ordenação em runs | varredura do heap, sem particionar (cabe em memória) · varredura do heap, sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | b | curso_heap_sem_indices:scan × aluno_bt_curso:primary_btree | varredura do heap · folhas da B+ | folhas da B+ lidas em ordem (sem ordenar) · varredura do heap + ordenação em runs | varredura do heap, sem particionar (cabe em memória) · folhas da B+, sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | c | curso_bt_codigo:primary_btree × aluno_bt_curso:primary_btree | folhas da B+ · folhas da B+ | folhas da B+ lidas em ordem (sem ordenar) · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · folhas da B+, sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | c | curso_h_codigo:primary_hash × aluno_h_curso:primary_hash | buckets do hash · buckets do hash | buckets do hash + ordenação em runs · buckets do hash + ordenação em runs | buckets do hash, sem particionar (cabe em memória) · buckets do hash, sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | d | curso_bt_codigo:primary_btree × aluno_heap:secondary_btree | folhas da B+ · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · folhas da B+ lidas em ordem (sem ordenar) | folhas da B+, sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |
| Curso ⋈ Aluno (codigo = curso) | e | curso_heap:secondary_hash × aluno_heap:secondary_btree | varredura do heap (índice secundário não usado) · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · varredura do heap (índice secundário não usado) + ordenação em runs | varredura do heap (índice secundário não usado), sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |
| Aluno ⋈ Detalhes (cpf) | a | aluno_heap_sem_indices:scan × detalhes_heap:scan | varredura do heap · varredura do heap | varredura do heap + ordenação em runs · varredura do heap + ordenação em runs | varredura do heap + particionamento · varredura do heap + particionamento |
| Aluno ⋈ Turma (professor_orientador = professor) | e | aluno_heap:secondary_btree × turma_heap:secondary_btree | varredura do heap (índice secundário não usado) · varredura do heap (índice secundário não usado) | varredura do heap (índice secundário não usado) + ordenação em runs · varredura do heap (índice secundário não usado) + ordenação em runs | varredura do heap (índice secundário não usado), sem particionar (cabe em memória) · varredura do heap (índice secundário não usado), sem particionar (cabe em memória) |

## Convergência das repetições

168 grupos executados: 168 convergiram (E/S e tempo abaixo de 0.05) e 0 pararam no teto de repetições sem convergir (0 só no tempo, com a E/S estável). A dispersão é (máx − mín)/média das últimas repetições. Na seleção pontual, o lote muda a cada repetição, então a E/S também varia. Só os grupos que não convergiram aparecem abaixo.

_Sem dados nesta saída._


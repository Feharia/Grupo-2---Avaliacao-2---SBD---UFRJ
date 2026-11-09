# Tabelas da rodada (LRU)

Geradas por `tools/tabelas.py` a partir de `resultados_exemplo/`. A métrica é o número de blocos transferidos (leituras + gravações); o tempo é só complementar.

Índice: [Resumo da rodada](#resumo-da-rodada) · [Seleção por igualdade em campo UNIQUE (lote de chaves)](#seleção-por-igualdade-em-campo-unique-lote-de-chaves) · [Seleção por faixa](#seleção-por-faixa) · [Mesma seleção, organizações e caminhos diferentes](#mesma-seleção-organizações-e-caminhos-diferentes) · [Junções: custo de cada algoritmo](#junções-custo-de-cada-algoritmo) · [Melhor algoritmo por situação](#melhor-algoritmo-por-situação) · [Escolha da externa e da interna](#escolha-da-externa-e-da-interna) · [Política de substituição: LRU × MRU](#política-de-substituição-lru--mru) · [Custo estimado × medido](#custo-estimado--medido) · [Fases do sort-merge e do hash join](#fases-do-sort-merge-e-do-hash-join) · [Estruturas construídas](#estruturas-construídas) · [Ocupação antes e depois da carga de manutenção](#ocupação-antes-e-depois-da-carga-de-manutenção) · [Células que não rodam](#células-que-não-rodam)

## Resumo da rodada

| Item | Valor |
|---|---|
| Escala | 1 |
| Políticas comparadas | LRU, MRU |
| Repetições por combinação | 3..10 (tolerância 0.05) |
| M das junções / das seleções | 32 / 2 molduras |
| Chaves por lote de seleção pontual | 1000 |
| Combinações medidas (por política) | 54 |
| Linhas medidas no CSV (todas as repetições e políticas) | 358 |
| Células `nao_aplicavel` (caso 1) por política | 54 |
| Células `nao_aplicavel` (caso 2) por política | 2 |
| Células `nao_aplicavel` (caso 3) por política | 6 |
| Células `nao_aplicavel` (caso 4) por política | 4 |
| Conferência com o SQLite | conferidas: 358 linhas medidas; sem execução: nao_aplicavel: 132; divergências: 0 |
| Duração da rodada | 23.6 s |

## Seleção por igualdade em campo UNIQUE (lote de chaves)

Cada execução busca um lote de 1.000 chaves sorteadas. O lote muda a cada repetição, então as contagens variam um pouco: a tabela mostra a mediana e, em “Var. reps”, a amplitude entre repetições. “Leituras/chave” divide as leituras pelo lote. “× melhor” compara com o caminho de menor custo na mesma consulta.

| Consulta | Config. | Caminho | Linhas | Leituras | Leituras/chave | Aleatórias | Taxa de acerto | Estimado | Erro est. | Tempo (ms) | Var. reps | × melhor |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| igualdade em UNIQUE: cpf | a | heap, varredura | 1.000 | 321.360 | 321,36 | 996 | 0,00 | 323.000 | +0,5% | 317,8 | 5,3% | 169,0× |
| igualdade em UNIQUE: cpf | c | heap + hash secundário (cpf) | 1.000 | 1.901 | 1,90 | 1.901 | 0,05 | 2.000 | +5,2% | 1,7 | 2,1% | melhor |
| igualdade em UNIQUE: dre | a | heap, varredura | 1.000 | 326.680 | 326,68 | 996 | 0,00 | 323.000 | -1,1% | 248,4 | 3,0% | 323,9× |
| igualdade em UNIQUE: dre | b | hash primário (dre) | 1.000 | 1.008 | 1,01 | 1.006 | 0,00 | 1.108 | +9,8% | 0,8 | 0,6% | melhor |
| igualdade em UNIQUE: dre | b | B+ primária (dre) | 1.000 | 1.045 | 1,04 | 1.002 | 0,66 | 3.000 | +187,1% | 3,5 | 1,3% | 1,04× |

## Seleção por faixa

“% seq.” é a fração das leituras que foram sequenciais. “× melhor” compara com o caminho de menor custo na mesma consulta e seletividade.

| Consulta | Selet. | Config. | Caminho | Linhas | Leituras | % seq. | Estimado | Erro est. | Tempo (ms) | × melhor |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| faixa em UNIQUE: dre | 0,1% | a | heap, varredura | 20 | 646 | 99,8% | 646 | +0,0% | 0,53 | 161,5× |
| faixa em UNIQUE: dre | 0,1% | b | B+ primária (dre) | 20 | 4 | 25,0% | 3 | -25,0% | 0,01 | melhor |
| faixa em UNIQUE: dre | 50% | a | heap, varredura | 10.000 | 646 | 99,8% | 646 | +0,0% | 3,00 | 1,35× |
| faixa em UNIQUE: dre | 50% | b | B+ primária (dre) | 10.000 | 479 | 99,4% | 325 | -32,2% | 2,96 | melhor |
| faixa em NOT UNIQUE: cr | 0,1% | a | heap, varredura | 21 | 646 | 99,8% | 646 | +0,0% | 0,44 | 28,1× |
| faixa em NOT UNIQUE: cr | 0,1% | c | heap + B+ secundária (cr) | 21 | 23 | 0,0% | 23 | +0,0% | 0,02 | melhor |
| faixa em NOT UNIQUE: cr | 50% | a | heap, varredura | 10.044 | 646 | 99,8% | 646 | +0,0% | 3,03 | melhor |
| faixa em NOT UNIQUE: cr | 50% | c | heap + B+ secundária (cr) | 10.044 | 9.715 | 6,9% | 10.046 | +3,4% | 6,72 | 15,0× |
| faixa em NOT UNIQUE: curso | 0,1% | a | heap, varredura | 107 | 646 | 99,8% | 646 | +0,0% | 0,53 | 107,7× |
| faixa em NOT UNIQUE: curso | 0,1% | b | B+ primária (curso) | 107 | 8 | 62,5% | 6 | -25,0% | 0,04 | 1,33× |
| faixa em NOT UNIQUE: curso | 0,1% | c | hash primário (curso) + B+ clustering (curso) | 107 | 6 | 0,0% | 5 | -16,7% | 0,03 | melhor |
| faixa em NOT UNIQUE: curso | 0,1% | c | B+ primária (curso) + B+ clustering (curso) | 107 | 8 | 62,5% | 5 | -37,5% | 0,04 | 1,33× |
| faixa em NOT UNIQUE: curso | 50% | a | heap, varredura | 10.074 | 646 | 99,8% | 646 | +0,0% | 3,05 | 1,34× |
| faixa em NOT UNIQUE: curso | 50% | b | B+ primária (curso) | 10.074 | 482 | 99,4% | 327 | -32,2% | 2,96 | melhor |
| faixa em NOT UNIQUE: curso | 50% | c | B+ primária (curso) + B+ clustering (curso) | 10.074 | 482 | 99,4% | 326 | -32,4% | 2,88 | melhor |
| faixa em NOT UNIQUE: curso | 50% | c | hash primário (curso) + B+ clustering (curso) | 10.074 | 672 | 0,0% | 326 | -51,5% | 3,04 | 1,39× |

## Mesma seleção, organizações e caminhos diferentes

Blocos transferidos (LRU). Em negrito, o menor custo de cada linha. “n/a” é uma célula que não se aplica (decisão 170); vazio é uma combinação que a bancada não monta para essa consulta.

| Consulta | Selet. | Heap (varredura) | B+ primária | Hash primário | Heap + hash sec. | Heap + B+ sec. | B+ prim. + clustering | Hash prim. + clustering |
|---|---|---:|---:|---|---:|---:|---:|---:|
| igualdade em UNIQUE: cpf | pontual | 321.360 |  |  | **1.901** |  |  |  |
| igualdade em UNIQUE: dre | pontual | 326.680 | 1.045 | **1.008** |  |  |  |  |
| faixa em UNIQUE: dre | 0,1% | 646 | **4** | n/a (caso 4) |  |  |  |  |
| faixa em UNIQUE: dre | 50% | 646 | **479** | n/a (caso 4) |  |  |  |  |
| faixa em NOT UNIQUE: cr | 0,1% | 646 |  |  |  | **23** |  |  |
| faixa em NOT UNIQUE: cr | 50% | **646** |  |  |  | 9.715 |  |  |
| faixa em NOT UNIQUE: curso | 0,1% | 646 | 8 | n/a (caso 4) |  |  | 8 | **6** |
| faixa em NOT UNIQUE: curso | 50% | 646 | **482** | n/a (caso 4) |  |  | **482** | 672 |

## Junções: custo de cada algoritmo

M = 32 molduras. A primeira tabela de “Externa → interna” é a externa no BNL, a de build no hash e a primeira entrada do merge. “× melhor” compara com a opção mais barata para as mesmas entradas (todos os algoritmos e as duas ordens).

| Junção | Tamanho | Config. | Externa → interna | Algoritmo | Leituras | Gravações | Total | Estimado | Erro est. | Tempo (ms) | × melhor |
|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | BNL | 70.141 | 0 | 70.141 | 70.141 | +0,0% | 166,0 | 9,93× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | BNL | 70.171 | 0 | 70.171 | 70.171 | +0,0% | 132,4 | 9,93× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | sort-merge | 6.404 | 4.063 | 10.467 | 10.413 | -0,5% | 91,5 | 1,48× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | sort-merge | 6.404 | 4.063 | 10.467 | 10.413 | -0,5% | 91,4 | 1,48× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Aluno: heap, varredura → Inscrição: heap, varredura | hash | 4.703 | 2.362 | 7.065 | 7.127 | +0,9% | 58,0 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | a | Inscrição: heap, varredura → Aluno: heap, varredura | hash | 7.134 | 4.793 | 11.927 | 15.549 | +30,4% | 82,2 | 1,69× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | BNL | 102.653 | 0 | 102.653 | 102.653 | +0,0% | 238,6 | 13,9× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Inscrição: heap, varredura → Aluno: B+ primária (dre) | sort-merge | 6.067 | 3.417 | 9.484 | 9.428 | -0,6% | 84,4 | 1,29× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | hash | 5.010 | 2.362 | 7.372 | 7.434 | +0,8% | 58,7 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | BNL | 147.353 | 0 | 147.353 | 147.353 | +0,0% | 213,0 | 43,4× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Inscrição: B+ primária (dre) → Aluno: B+ primária (dre) | sort-merge | 3.397 | 0 | 3.397 | 3.393 | -0,1% | 42,1 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | hash | 5.755 | 2.362 | 8.117 | 8.179 | +0,8% | 58,5 | 2,39× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | BNL | 147.840 | 0 | 147.840 | 147.840 | +0,0% | 271,3 | 30,6× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Inscrição: hash primário (dre) → Aluno: hash primário (dre) | sort-merge | 7.583 | 4.063 | 11.646 | 11.592 | -0,5% | 93,8 | 2,41× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | hash | 4.179 | 659 | 4.838 | 4.916 | +1,6% | 49,3 | melhor |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | BNL | 102.653 | 0 | 102.653 | 102.653 | +0,0% | 239,8 | 13,9× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Inscrição: heap + B+ secundária (dre) → Aluno: B+ primária (dre) | sort-merge | 6.067 | 3.417 | 9.484 | 9.428 | -0,6% | 84,6 | 1,29× |
| Aluno ⋈ Inscrição (dre) — natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | hash | 5.010 | 2.362 | 7.372 | 7.434 | +0,8% | 58,3 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Curso: heap, varredura → Disciplina: heap, varredura | BNL | 19 | 0 | 19 | 19 | +0,0% | 0,3 | melhor |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Disciplina: heap, varredura → Curso: heap, varredura | sort-merge | 38 | 19 | 57 | 57 | +0,0% | 0,8 | 3,00× |
| Curso ⋈ Disciplina (codigo = curso) — natural join | menor ⋈ menor | a | Curso: heap, varredura → Disciplina: heap, varredura | hash | 19 | 0 | 19 | 19 | +0,0% | 0,4 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Curso: heap, varredura → Aluno: heap, varredura | BNL | 653 | 0 | 653 | 653 | +0,0% | 9,1 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Aluno: heap, varredura → Curso: heap, varredura | sort-merge | 1.306 | 653 | 1.959 | 1.959 | +0,0% | 16,7 | 3,00× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | a | Curso: heap, varredura → Aluno: heap, varredura | hash | 653 | 0 | 653 | 653 | +0,0% | 9,3 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Curso: heap, varredura → Aluno: B+ primária (curso) | BNL | 960 | 0 | 960 | 960 | +0,0% | 9,2 | melhor |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Aluno: B+ primária (curso) → Curso: heap, varredura | sort-merge | 969 | 7 | 976 | 974 | -0,2% | 9,7 | 1,02× |
| Curso ⋈ Aluno (codigo = curso) — natural join | menor ⋈ maior | b | Curso: heap, varredura → Aluno: B+ primária (curso) | hash | 960 | 0 | 960 | 960 | +0,0% | 9,4 | melhor |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | BNL | 22.506 | 0 | 22.506 | 22.506 | +0,0% | 58,4 | 6,21× |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | sort-merge | 3.027 | 1.839 | 4.866 | 7.816 | +60,6% | 34,8 | 1,34× |
| Aluno ⋈ Detalhes (cpf) — equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura → Aluno: heap, varredura | hash | 2.406 | 1.218 | 3.624 | 12.952 | +257,4% | 22,0 | melhor |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | maior ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | BNL | 1.980 | 0 | 1.980 | 1.980 | +0,0% | 76,2 | 1,02× |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | maior ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | sort-merge | 1.310 | 622 | 1.932 | 2.064 | +6,8% | 82,0 | melhor |
| Aluno ⋈ Turma (professor_orientador = professor) — equijoin NOT UNIQUE | maior ⋈ maior | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | hash | 1.312 | 624 | 1.936 | 2.072 | +7,0% | 78,8 | 1,00× |

## Melhor algoritmo por situação

Para cada célula (tipo, tamanho, configuração), o total de blocos de cada algoritmo na melhor ordem externa/interna. “Vantagem” é quantas vezes o próximo algoritmo mais barato custa em relação ao vencedor (empates contam juntos).

| Junção | Tipo | Tamanho | Config. | Entradas | BNL | Sort-merge | Hash | Vencedor | Vantagem |
|---|---|---|---|---|---:|---:|---:|---|---:|
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | a | Aluno: heap, varredura ⋈ Inscrição: heap, varredura | 70.141 | 10.467 | **7.065** | hash | 1,48× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | b | Aluno: B+ primária (dre) ⋈ Inscrição: heap, varredura | 102.653 | 9.484 | **7.372** | hash | 1,29× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | c | Aluno: B+ primária (dre) ⋈ Inscrição: B+ primária (dre) | 147.353 | **3.397** | 8.117 | sort-merge | 2,39× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | c | Aluno: hash primário (dre) ⋈ Inscrição: hash primário (dre) | 147.840 | 11.646 | **4.838** | hash | 2,41× |
| Aluno ⋈ Inscrição (dre) | natural join | maior ⋈ maior | d | Aluno: B+ primária (dre) ⋈ Inscrição: heap + B+ secundária (dre) | 102.653 | 9.484 | **7.372** | hash | 1,29× |
| Curso ⋈ Disciplina (codigo = curso) | natural join | menor ⋈ menor | a | Curso: heap, varredura ⋈ Disciplina: heap, varredura | **19** | 57 | **19** | BNL = hash (empate) | 3,00× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | a | Curso: heap, varredura ⋈ Aluno: heap, varredura | **653** | 1.959 | **653** | BNL = hash (empate) | 3,00× |
| Curso ⋈ Aluno (codigo = curso) | natural join | menor ⋈ maior | b | Curso: heap, varredura ⋈ Aluno: B+ primária (curso) | **960** | 976 | **960** | BNL = hash (empate) | 1,02× |
| Aluno ⋈ Detalhes (cpf) | equijoin UNIQUE | maior ⋈ maior | a | Detalhes: heap, varredura ⋈ Aluno: heap, varredura | 22.506 | 4.866 | **3.624** | hash | 1,34× |
| Aluno ⋈ Turma (professor_orientador = professor) | equijoin NOT UNIQUE | maior ⋈ maior | e | Turma: heap + B+ secundária (professor) ⋈ Aluno: heap + B+ secundária (professor_orientador) | 1.980 | **1.932** | 1.936 | sort-merge | 1,00× |

## Escolha da externa e da interna

A ordem padrão segue a decisão 122: no BNL e no hash, a menor por fora (build); no sort-merge, a UNIQUE por dentro. “Δ” é o custo da ordem trocada em relação à padrão.

| Junção | Config. | Algoritmo | Ordem padrão | Total | Ordem trocada | Total | Δ |
|---|---|---|---|---:|---|---:|---:|
| Aluno ⋈ Inscrição (dre) | a | BNL | Aluno: heap, varredura → Inscrição: heap, varredura | 70.141 | Inscrição: heap, varredura → Aluno: heap, varredura | 70.171 | +0,0% |
| Aluno ⋈ Inscrição (dre) | a | sort-merge | Inscrição: heap, varredura → Aluno: heap, varredura | 10.467 | Aluno: heap, varredura → Inscrição: heap, varredura | 10.467 | +0,0% |
| Aluno ⋈ Inscrição (dre) | a | hash | Aluno: heap, varredura → Inscrição: heap, varredura | 7.065 | Inscrição: heap, varredura → Aluno: heap, varredura | 11.927 | +68,8% |

## Política de substituição: LRU × MRU

Só as combinações em que o total de blocos muda. Nas outras 40, as duas políticas transferem o mesmo número de blocos.

| Consulta | Selet./tamanho | Onde | LRU | MRU | Δ | Acerto LRU | Acerto MRU |
|---|---|---|---:|---:|---:|---:|---:|
| Aluno ⋈ Detalhes (cpf) | maior ⋈ maior | BNL, config. a: Detalhes: heap, varredura → Aluno: heap, varredura | 22.506 | 21.995 | -2,3% | 0,00 | 0,02 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. a: Inscrição: heap, varredura → Aluno: heap, varredura | 70.171 | 68.580 | -2,3% | 0,00 | 0,02 |
| Aluno ⋈ Turma (professor_orientador = professor) | maior ⋈ maior | BNL, config. e: Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | 1.980 | 1.948 | -1,6% | 0,00 | 0,02 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. a: Aluno: heap, varredura → Inscrição: heap, varredura | 70.141 | 69.525 | -0,9% | 0,00 | 0,01 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. b: Aluno: B+ primária (dre) → Inscrição: heap, varredura | 102.653 | 101.752 | -0,9% | 0,00 | 0,01 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. d: Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | 102.653 | 101.752 | -0,9% | 0,00 | 0,01 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. c: Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | 147.353 | 146.452 | -0,6% | 0,00 | 0,01 |
| igualdade em UNIQUE: cpf | pontual | config. a: heap, varredura | 321.360 | 319.477 | -0,6% | 0,00 | 0,00 |
| Aluno ⋈ Inscrição (dre) | maior ⋈ maior | BNL, config. c: Aluno: hash primário (dre) → Inscrição: hash primário (dre) | 147.840 | 147.000 | -0,6% | 0,00 | 0,01 |
| igualdade em UNIQUE: cpf | pontual | config. c: heap + hash secundário (cpf) | 1.901 | 1.892 | -0,5% | 0,05 | 0,05 |
| igualdade em UNIQUE: dre | pontual | config. a: heap, varredura | 326.680 | 325.976 | -0,2% | 0,00 | 0,00 |
| faixa em NOT UNIQUE: curso | 50% | config. c: hash primário (curso) + B+ clustering (curso) | 672 | 671 | -0,1% | 0,00 | 0,00 |
| faixa em NOT UNIQUE: cr | 50% | config. c: heap + B+ secundária (cr) | 9.715 | 9.702 | -0,1% | 0,04 | 0,04 |
| igualdade em UNIQUE: dre | pontual | config. b: hash primário (dre) | 1.008 | 1.010 | +0,1% | 0,00 | 0,00 |

## Custo estimado × medido

Erro = (estimado − medido) / medido, em blocos transferidos. As fórmulas são as dos livros (Elmasri e Silberschatz), com os parâmetros medidos pela própria bancada.

| Família | Combinações | Mediana do erro absoluto | Erro absoluto ≤ 10% | Maior erro | Onde |
|---|---:|---:|---|---:|---|
| junção: BNL | 11 | 0,0% | 11 de 11 | +0,0% | Aluno ⋈ Inscrição (dre) (config. a: Aluno: heap, varredura → Inscrição: heap, varredura) |
| junção: hash | 11 | 0,8% | 9 de 11 | +257,4% | Aluno ⋈ Detalhes (cpf) (config. a: Detalhes: heap, varredura → Aluno: heap, varredura) |
| junção: sort-merge | 11 | 0,5% | 10 de 11 | +60,6% | Aluno ⋈ Detalhes (cpf) (config. a: Detalhes: heap, varredura → Aluno: heap, varredura) |
| seleção: faixa | 16 | 10,0% | 8 de 16 | -51,5% | faixa em NOT UNIQUE: curso (config. c: hash primário (curso) + B+ clustering (curso)) |
| seleção: igualdade (lote) | 5 | 5,2% | 4 de 5 | +187,1% | igualdade em UNIQUE: dre (config. b: B+ primária (dre)) |

## Fases do sort-merge e do hash join

Leituras/gravações de cada fase (1ª repetição). No hash join, o resumo traz os passes de particionamento recursivo e os bailouts (decisão 171).

| Junção | Config. | Externa → interna | Algoritmo | Fases (leituras/gravações) | Recursão |
|---|---|---|---|---|---|
| Aluno ⋈ Inscrição (dre) | a | Inscrição: heap, varredura → Aluno: heap, varredura | sort-merge | runs externa 1.695/1.721 · runs interna 646/646 · passes 1.721/1.696 · intercalação final 2.342/0 | — |
| Aluno ⋈ Inscrição (dre) | a | Aluno: heap, varredura → Inscrição: heap, varredura | sort-merge | runs externa 646/646 · runs interna 1.695/1.721 · passes 1.721/1.696 · intercalação final 2.342/0 | — |
| Aluno ⋈ Inscrição (dre) | a | Aluno: heap, varredura → Inscrição: heap, varredura | hash | particiona externa 646/657 · particiona interna 1.695/1.705 · build probe 2.362/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | a | Inscrição: heap, varredura → Aluno: heap, varredura | hash | particiona externa 1.695/1.707 · particiona interna 646/660 · build probe 4.793/2.426 | passes recursivos=31 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | b | Inscrição: heap, varredura → Aluno: B+ primária (dre) | sort-merge | runs externa 1.695/1.721 · passes 1.721/1.696 · intercalação final 2.651/0 | — |
| Aluno ⋈ Inscrição (dre) | b | Aluno: B+ primária (dre) → Inscrição: heap, varredura | hash | particiona externa 953/657 · particiona interna 1.695/1.705 · build probe 2.362/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | c | Inscrição: B+ primária (dre) → Aluno: B+ primária (dre) | sort-merge | passes 0/0 · intercalação final 3.397/0 | — |
| Aluno ⋈ Inscrição (dre) | c | Aluno: B+ primária (dre) → Inscrição: B+ primária (dre) | hash | particiona externa 953/657 · particiona interna 2.440/1.705 · build probe 2.362/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | c | Inscrição: hash primário (dre) → Aluno: hash primário (dre) | sort-merge | runs externa 2.624/1.721 · runs interna 896/646 · passes 1.721/1.696 · intercalação final 2.342/0 | — |
| Aluno ⋈ Inscrição (dre) | c | Aluno: hash primário (dre) → Inscrição: hash primário (dre) | hash | particiona externa 896/659 · build probe 3.283/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Inscrição (dre) | d | Inscrição: heap + B+ secundária (dre) → Aluno: B+ primária (dre) | sort-merge | runs externa 1.695/1.721 · passes 1.721/1.696 · intercalação final 2.651/0 | — |
| Aluno ⋈ Inscrição (dre) | d | Aluno: B+ primária (dre) → Inscrição: heap + B+ secundária (dre) | hash | particiona externa 953/657 · particiona interna 1.695/1.705 · build probe 2.362/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Disciplina (codigo = curso) | a | Disciplina: heap, varredura → Curso: heap, varredura | sort-merge | runs externa 12/12 · runs interna 7/7 · passes 0/0 · intercalação final 19/0 | — |
| Curso ⋈ Disciplina (codigo = curso) | a | Curso: heap, varredura → Disciplina: heap, varredura | hash | build probe 19/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | a | Aluno: heap, varredura → Curso: heap, varredura | sort-merge | runs externa 646/646 · runs interna 7/7 · passes 0/0 · intercalação final 653/0 | — |
| Curso ⋈ Aluno (codigo = curso) | a | Curso: heap, varredura → Aluno: heap, varredura | hash | build probe 653/0 | passes recursivos=0 bailouts=0 |
| Curso ⋈ Aluno (codigo = curso) | b | Aluno: B+ primária (curso) → Curso: heap, varredura | sort-merge | runs interna 7/7 · passes 0/0 · intercalação final 962/0 | — |
| Curso ⋈ Aluno (codigo = curso) | b | Curso: heap, varredura → Aluno: B+ primária (curso) | hash | build probe 960/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Detalhes (cpf) | a | Detalhes: heap, varredura → Aluno: heap, varredura | sort-merge | runs externa 542/547 · runs interna 646/646 · passes 646/646 · intercalação final 1.193/0 | — |
| Aluno ⋈ Detalhes (cpf) | a | Detalhes: heap, varredura → Aluno: heap, varredura | hash | particiona externa 542/558 · particiona interna 646/660 · build probe 1.218/0 | passes recursivos=0 bailouts=0 |
| Aluno ⋈ Turma (professor_orientador = professor) | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | sort-merge | runs externa 42/42 · runs interna 646/580 · passes 0/0 · intercalação final 622/0 | — |
| Aluno ⋈ Turma (professor_orientador = professor) | e | Turma: heap + B+ secundária (professor) → Aluno: heap + B+ secundária (professor_orientador) | hash | particiona externa 42/43 · particiona interna 646/581 · build probe 624/0 | passes recursivos=0 bailouts=0 |

## Estruturas construídas

Medidas pela própria bancada depois da carga. n = registros, R = bytes por registro, bfr = registros por bloco. Nos índices, “Blocos” conta só o índice.

| Estrutura | n | R | bfr | Blocos | Altura | Folhas | Distintos | Buckets | Overflow | Fator de carga | Cadeia média | Cadeia máx. |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| aluno_bt_curso:btree | 20.000 | 127 | 31 | 953 | 3 | 953 | 200 | — | — | — | — | — |
| aluno_bt_curso:sec_btree_clustering(curso) | 20.000 | 127 | 31 | 3 | 2 | 2 | 200 | — | 0 | — | — | — |
| aluno_bt_dre:btree | 20.000 | 127 | 31 | 953 | 3 | 953 | 20.000 | — | — | — | — | — |
| aluno_h_curso:hash | 20.000 | 127 | 31 | 791 | — | — | — | 211 | 580 | 3,06 | 3,75 | 14 |
| aluno_h_curso:sec_btree_clustering(curso) | 20.000 | 127 | 31 | 3 | 2 | 2 | 200 | — | 0 | — | — | — |
| aluno_h_dre:hash | 20.000 | 127 | 31 | 896 | — | — | — | 809 | 87 | 0,80 | 1,11 | 2 |
| aluno_heap:heap | 20.000 | 127 | 31 | 646 | — | — | — | — | — | — | — | — |
| aluno_heap:sec_hash(cpf) | 20.000 | 127 | 31 | 149 | — | — | — | 149 | 0 | 0,76 | 1,00 | 1 |
| aluno_heap:sec_btree(professor_orientador) | 20.000 | 127 | 31 | 63 | 2 | 62 | 500 | — | 0 | — | — | — |
| aluno_heap:sec_btree(cr) | 20.000 | 127 | 31 | 155 | 2 | 61 | 571 | — | 93 | — | — | — |
| curso_heap:heap | 200 | 122 | 32 | 7 | — | — | — | — | — | — | — | — |
| detalhes_heap:heap | 20.000 | 256 | 15 | 542 | — | — | — | — | — | — | — | — |
| disciplina_heap:heap | 800 | 52 | 72 | 12 | — | — | — | — | — | — | — | — |
| inscricao_bt_dre:btree | 100.000 | 64 | 59 | 2.440 | 3 | 2.440 | 20.000 | — | — | — | — | — |
| inscricao_h_dre:hash | 100.000 | 64 | 59 | 2.624 | — | — | — | 2.129 | 495 | 0,80 | 1,23 | 2 |
| inscricao_heap:heap | 100.000 | 64 | 59 | 1.695 | — | — | — | — | — | — | — | — |
| inscricao_heap:sec_btree(dre) | 100.000 | 64 | 59 | 441 | 2 | 440 | 20.000 | — | 0 | — | — | — |
| turma_heap:heap | 6.000 | 24 | 145 | 42 | — | — | — | — | — | — | — | — |
| turma_heap:sec_btree(professor) | 6.000 | 24 | 145 | 19 | 2 | 18 | 500 | — | 0 | — | — | — |

## Ocupação antes e depois da carga de manutenção

Blocos por estrutura. Só as estruturas que mudaram; as outras 12 ficaram iguais (tabelas fora da carga de manutenção).

| Estrutura | Dados antes | Dados depois | Overflow antes | Overflow depois | Total antes | Total depois | Δ |
|---|---:|---:|---:|---:|---:|---:|---:|
| aluno_bt_curso:btree | 953 | 955 | 0 | 0 | 959 | 961 | +0,2% |
| aluno_bt_dre:btree | 953 | 1.015 | 0 | 0 | 959 | 1.021 | +6,5% |
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


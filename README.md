# Grupo-2---Avaliacao-2---SBD---UFRJ

Repositório com o trabalho da Avaliação 2 de Sistemas de Banco de Dados da UFRJ, do professor Milton Cunha: uma bancada de teste que mede quantos blocos cada operação relacional transfere entre o disco e o buffer pool, sob diferentes configurações do gerente de armazenamento.

- [1. Contexto](#1-contexto)
- [2. Objetivo](#2-objetivo)
- [3. Como rodar a bancada](#3-como-rodar-a-bancada)
- [4. Resultados](#4-resultados)
- [5. Limitações conhecidas](#5-limitações-conhecidas)
- [6. Mapa do repositório](#6-mapa-do-repositório)

## 1. Contexto

O enunciado ([PDF](PrototipoIA/Avaliacao_2_Indexacao_e_Processamento_de_Consultas.pdf)) pede que o grupo construa, com auxílio de IA, um mini SGBD em quatro camadas. Cada camada usa só a interface da camada de baixo:

| Camada | Papel | No código |
|---|---|---|
| 1. Bancada de teste | Gera a base, monta cada configuração, executa as operações, repete as medidas e grava os CSVs | `src/layer1/bench.cpp` |
| 2. Mini monitor de execução | Uma primitiva por algoritmo: BNL, sort-merge e hash join; seleção por varredura, por busca na estrutura ou por índice + fetch. O caminho de acesso é escolhido pelo experimento; não há SQL nem otimizador | `src/layer2/` |
| 3. Gerente de armazenamento | Organizações primárias (heap, B+, hash estático), índices secundários (hash, B+ e B+ clustering) e um mini catálogo | `src/layer3/` |
| 4. Gerente de arquivos e buffer pool | Blocos de 4 KB lidos e gravados com `pread`/`pwrite`. Buffer pool com áreas separadas e políticas LRU e MRU. Conta cada leitura e gravação, separando as sequenciais das aleatórias | `src/layer4/` |

![Diagrama das camadas](PrototipoIA/diagrama_camadas_v5.png)

A métrica central é o **número de blocos transferidos**. O tempo de relógio também é registrado, mas só como informação complementar. Cada medida tem ao lado o custo estimado pelas fórmulas dos livros-texto (Silberschatz e afins), calculado com os parâmetros medidos pela própria bancada. Cada resultado é conferido contra o SQLite, em número de linhas e checksum.

O código em `PrototipoIA/Implementacao/` é um **protótipo gerado por IA (Claude)**, ainda sem revisão completa do grupo. O log de prompts está em [`PROMPTS.md`](PrototipoIA/Implementacao/PROMPTS.md). A arquitetura e as 117 decisões que guiaram a geração estão em [`HANDOFF_ARQUITETURA_2026-11-08.md`](PrototipoIA/HANDOFF_ARQUITETURA_2026-11-08.md).

## 2. Objetivo

Responder com medidas: **qual configuração do SGBD processa melhor uma dada carga de trabalho sobre uma dada base?** Para isso, a bancada compara:

- **Organizações primárias:** heap; B+ por campo UNIQUE e por NOT UNIQUE; hash estático por campo UNIQUE e por NOT UNIQUE.
- **Métodos de acesso:**
  - varredura;
  - busca pela organização primária;
  - índice secundário hash (campo UNIQUE) e B+ (campo NOT UNIQUE) sobre o heap;
  - índice B+ clustering sobre B+ e sobre hash.
- **Algoritmos de junção:** nested-loop em blocos (BNL), sort-merge e partition-hash, este com particionamento recursivo.
- **Situações (fatores):**

| Fator | Níveis |
|---|---|
| Tipo de seleção | igualdade em UNIQUE; faixa em UNIQUE; faixa em NOT UNIQUE |
| Seletividade da seleção | pontual (1 registro por chave, em lotes de 1.000 chaves); cerca de 0,1%; cerca de 50% |
| Configuração da seleção | a: sem índice; b: pelo campo da organização primária; c: por índice secundário |
| Tipo de junção | natural join (PK–FK); equijoin em campos UNIQUE; equijoin em campos NOT UNIQUE |
| Tamanho das tabelas da junção | menor ⋈ menor; menor ⋈ maior; maior ⋈ maior. "Menor" é a tabela que cabe na área de trabalho: b ≤ M − 2 = 30 blocos |
| Configuração da junção | a: nenhuma tabela com índice; b: uma organizada pelo campo da junção; c: as duas organizadas pelo campo; d: uma organizada e a outra com índice secundário; e: as duas com índice secundário |
| Ordem das entradas | externa × interna (build × probe no hash) |
| Política de substituição | LRU e MRU |

Células da matriz do enunciado que não fazem sentido, como uma faixa sobre hash, aparecem no resultado como `nao_aplicavel`, junto com o motivo (casos 1 a 4, decisão 170).

## 3. Como rodar a bancada

### Pré-requisitos

- **Sistema POSIX:** macOS, Linux ou WSL2. A E/S usa `pread` e `pwrite`, então não roda no Windows nativo.
- **Compilador C++20:** testado com Apple clang 21. O alvo do grupo é GCC 12 ou mais novo, no Linux, ainda não testado.
- **Build:** `make`, ou CMake 3.20+.
- **Python 3:** só a biblioteca padrão, incluindo o módulo `sqlite3`.
- **Disco:** cerca de 110 MB livres para uma rodada completa. A maior parte é a base gerada e os arquivos do motor.

### Passo a passo

1. Clone o repositório e entre na pasta do código:

   ```bash
   git clone git@github.com:Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ.git
   cd Grupo-2---Avaliacao-2---SBD---UFRJ/PrototipoIA/Implementacao
   ```

2. Compile. Isso gera `build/bancada` e `build/testes`; leva uns 4 s com `-j8`.

   ```bash
   make -j8
   ```

3. Rode os testes: são 40, das camadas 2, 3 e 4. Para rodar os mesmos testes com AddressSanitizer e UBSan, use `make asan`.

   ```bash
   make test
   ```

4. Faça uma rodada rápida (escala 0,1 e 1 repetição) para conferir que tudo funciona. Em poucos segundos ela gera a base, mede, confere com o SQLite e grava as tabelas em `saida_rapida/tabelas.md`. A última linha do `compare.py` deve dizer `divergências: 0`.

   ```bash
   make rapido
   ```

5. Faça a rodada completa (escala 1, de 3 a 10 repetições por combinação, LRU e MRU). Leva uns 25 s.

   ```bash
   ./build/bancada --saida saida
   ```

6. Gere a referência no SQLite (uns 5 s) e confira as linhas e os checksums. O `compare.py` termina com código 1 se alguma linha divergir.

   ```bash
   python3 -I tools/reference.py saida
   python3 -I tools/compare.py saida
   ```

7. Gere as tabelas de análise, em `saida/tabelas.md` e `saida/tabelas/*.csv`:

   ```bash
   python3 -I tools/tabelas.py saida
   ```

   Para gerar as tabelas principais com a outra política, use `python3 -I tools/tabelas.py saida --politica MRU`.

Com CMake, o build fica em outra pasta:

```bash
cmake -S . -B build-cmake && cmake --build build-cmake && ctest --test-dir build-cmake
```

As opções da bancada estão no [README do protótipo](PrototipoIA/Implementacao/README.md#como-rodar):
- `--escala`;
- `--reps-min` e `--reps-max`;
- `--tolerancia`;
- `--politicas`;
- `--m-juncao` e `--m-selecao`;
- `--lote`;
- `--so`, para rodar só algumas operações;
- `--tuplas`, para conferir tupla a tupla.

### O que a bancada grava

| Arquivo em `saida/` | Conteúdo |
|---|---|
| `resultados.csv` | Uma linha por política, operação, variante, algoritmo e repetição. Traz a posição na matriz (tipo, tamanho, configuração, seletividade), o status e o motivo, as leituras e gravações (sequenciais e aleatórias), as requisições e os acertos, o custo estimado e o tempo |
| `fases.csv` | E/S de cada fase: runs, passes, particionamento, build/probe |
| `estatisticas.csv`, `ocupacao.csv` | Parâmetros medidos de cada estrutura e a ocupação antes e depois da carga de manutenção |
| `referencia.csv`, `conferencia.txt` | Resultado do SQLite e da conferência |
| `tabelas.md`, `tabelas/*.csv` | As 13 tabelas de análise do `tabelas.py` |
| `manifesto.txt` | Compilador, flags, escala, semente, parâmetros e duração |

## 4. Resultados

Os números abaixo vêm da rodada completa em [`resultados_exemplo/`](PrototipoIA/Implementacao/resultados_exemplo/):
- escala 1, política LRU;
- M = 32 molduras nas junções e M = 2 nas seleções;
- de 3 a 10 repetições por combinação.

As 358 linhas medidas batem com o SQLite. Todas as 13 tabelas estão em [`resultados_exemplo/tabelas.md`](PrototipoIA/Implementacao/resultados_exemplo/tabelas.md). Uma rodada nova reproduz as mesmas contagens de blocos; só o tempo e o nº de repetições mudam.

**A base** é o esquema de universidade do enunciado:

| Tabela | Registros | Bytes por registro | Registros por bloco | Blocos (heap) |
|---|---:|---:|---:|---:|
| Aluno | 20.000 | 127 | 31 | 646 |
| Inscrição | 100.000 | 64 | 59 | 1.695 |
| Detalhes | 20.000 | até 256 (VarChar) | 15 | 542 |
| Turma | 6.000 | 24 | 145 | 42 |
| Disciplina | 800 | 52 | 72 | 12 |
| Curso | 200 | 122 | 32 | 7 |

As B+ foram carregadas com 70% de ocupação, e por isso têm mais blocos que o heap: Aluno tem 953 folhas e Inscrição tem 2.440.

### 4.1 Seleções: organização primária × método de acesso

Blocos transferidos pela mesma consulta em cada organização e caminho. Em negrito, o menor custo da linha; "n/a" é uma célula que não se aplica.

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

**O que a tabela mostra**

- **Igualdade (1.000 chaves por lote):**
  - Sem índice, cada chave custa em média metade do heap, cerca de 323 blocos.
  - O hash primário custa 1,01 bloco por chave; a B+ primária, 1,04. A B+ fica perto do hash porque os nós internos ficam na área de índices do buffer (taxa de acerto de 0,66).
  - O hash secundário custa 1,9 bloco por chave: o bucket do índice e depois o bloco de dados. Contra a varredura, os índices reduzem o custo de 169 a 324 vezes.
- **Faixa estreita (0,1%):**
  - Qualquer índice ganha da varredura. A B+ primária lê 4 blocos contra 646 (161×), e a B+ secundária lê 23 (28×).
  - O hash primário não responde a faixa, porque não preserva a ordem das chaves.
- **Faixa larga (50%):**
  - O índice secundário não clustering **perde** da varredura: 9.715 blocos contra 646, 15 vezes mais. Cada um dos 10.044 registros vira um acesso aleatório a um bloco de dados; nesta rodada, os RIDs não são ordenados antes do fetch.
  - A B+ primária ainda ganha (479 × 646), mas só por 1,35×, porque as folhas ficam 70% cheias.
- **Clustering sobre B+ × clustering sobre hash:**
  - Com 0,1%, os dois custam quase o mesmo (8 × 6 blocos).
  - Com 50%, o clustering sobre B+ lê 482 blocos, 99% em sequência. Sobre hash, lê 672 blocos, todos aleatórios. O hash por curso, campo com poucos valores distintos, tem 580 blocos de overflow para 211 buckets.

### 4.2 Junções: o melhor algoritmo depende da situação

Total de blocos de cada algoritmo, na melhor ordem externa/interna, com M = 32. Em negrito, o vencedor.

| Junção | Tipo | Tamanho | Config. | Entradas | BNL | Sort-merge | Hash | Vencedor |
|---|---|---|---|---|---:|---:|---:|---|
| Curso ⋈ Disciplina | natural join | menor ⋈ menor | a | heap ⋈ heap | **19** | 57 | **19** | BNL = hash |
| Curso ⋈ Aluno | natural join | menor ⋈ maior | a | heap ⋈ heap | **653** | 1.959 | **653** | BNL = hash |
| Curso ⋈ Aluno | natural join | menor ⋈ maior | b | heap ⋈ B+ (curso) | **960** | 976 | **960** | BNL = hash |
| Aluno ⋈ Inscrição | natural join | maior ⋈ maior | a | heap ⋈ heap | 70.141 | 10.467 | **7.065** | hash (1,48×) |
| Aluno ⋈ Inscrição | natural join | maior ⋈ maior | b | B+ (dre) ⋈ heap | 102.653 | 9.484 | **7.372** | hash (1,29×) |
| Aluno ⋈ Inscrição | natural join | maior ⋈ maior | c | B+ (dre) ⋈ B+ (dre) | 147.353 | **3.397** | 8.117 | sort-merge (2,39×) |
| Aluno ⋈ Inscrição | natural join | maior ⋈ maior | c | hash (dre) ⋈ hash (dre) | 147.840 | 11.646 | **4.838** | hash (2,41×) |
| Aluno ⋈ Inscrição | natural join | maior ⋈ maior | d | B+ (dre) ⋈ heap + B+ sec. (dre) | 102.653 | 9.484 | **7.372** | hash (1,29×) |
| Aluno ⋈ Detalhes | equijoin UNIQUE (cpf) | maior ⋈ maior | a | heap ⋈ heap | 22.506 | 4.866 | **3.624** | hash (1,34×) |
| Aluno ⋈ Turma | equijoin NOT UNIQUE | maior ⋈ maior | e | heap + B+ sec. ⋈ heap + B+ sec. | 1.980 | **1.932** | 1.936 | sort-merge (1,00×) |

O número entre parênteses é quanto o 2º colocado custa em relação ao vencedor.

**O que a tabela mostra**

- **Uma tabela pequena muda tudo.** Quando uma das tabelas cabe em M − 2 blocos, BNL e hash leem cada tabela uma vez só (b_r + b_s). O sort-merge custa 3 vezes mais, porque ordena e grava runs mesmo assim.
- **Com duas tabelas grandes, o BNL custa de 6 a 43 vezes o vencedor.** Ele relê a interna inteira para cada bloco de 16 páginas da externa: 646 + 41 × 1.695 = 70.141. A exceção é Aluno ⋈ Turma: Turma tem só 42 blocos, e o BNL empata com os outros.
- **Com heaps (configuração a), o hash join vence o sort-merge.** São 7.065 blocos contra 10.467: o hash particiona uma vez (cerca de 3·(b_r + b_s)), enquanto o sort-merge grava runs e ainda faz um passe de intercalação.
- **A organização primária pode decidir o vencedor:**
  - Duas B+ pelo campo da junção dispensam a ordenação: o sort-merge só intercala as folhas (3.397 = 953 + 2.440 + 4 da descida até a primeira folha de cada árvore) e ganha do hash por 2,4×.
  - Dois hash pelo campo poupam particionamento: o hash join usa os buckets de Inscrição como partições, só particiona Aluno e ganha do sort-merge por 2,4×.
  - Já para o BNL, as B+ pioram, porque as folhas a 70% somam mais blocos que o heap.
- **O índice secundário não clustering não ajuda nenhum dos três algoritmos.** A configuração d custa exatamente o mesmo que a b, e na configuração e os três algoritmos leem as tabelas inteiras.

**A ordem das entradas pesa no hash join.** Com Aluno (646 blocos) como build, cada partição cabe na memória: 7.065 blocos. Com Inscrição (1.695 blocos) como build, as partições não cabem, e o hash join faz 31 passes de particionamento recursivo: 11.927 blocos, ou +68,8%. No BNL e no sort-merge, trocar a ordem quase não muda nada (+0,04% e 0%).

### 4.3 Política de substituição: LRU × MRU

Em 40 das 54 combinações, as duas políticas transferem exatamente o mesmo número de blocos. As diferenças estão no BNL entre tabelas grandes, em que a MRU economiza de 0,6% a 2,3%:

| Junção (BNL) | Config. | LRU | MRU | Δ |
|---|---|---:|---:|---:|
| Aluno ⋈ Detalhes | a | 22.506 | 21.995 | −2,3% |
| Inscrição ⋈ Aluno (ordem trocada) | a | 70.171 | 68.580 | −2,3% |
| Turma ⋈ Aluno | e | 1.980 | 1.948 | −1,6% |
| Aluno ⋈ Inscrição | a | 70.141 | 69.525 | −0,9% |
| Aluno ⋈ Inscrição | c (B+ × B+) | 147.353 | 146.452 | −0,6% |

Na LRU, a releitura sequencial da interna sempre expulsa a página que vai ser pedida em seguida (*sequential flooding*). A MRU mantém algumas páginas da interna na reserva, e a taxa de acerto sobe de 0,00 para 0,01–0,02. Fora do BNL, as diferenças ficam em até 0,6%.

### 4.4 Custo estimado × medido

| Família | Combinações | Mediana do erro absoluto | Erro absoluto ≤ 10% | Maior erro |
|---|---:|---:|---|---:|
| Junção: BNL | 11 | 0,0% | 11 de 11 | +0,0% |
| Junção: sort-merge | 11 | 0,5% | 10 de 11 | +60,6% (Aluno ⋈ Detalhes) |
| Junção: hash | 11 | 0,8% | 9 de 11 | +257,4% (Aluno ⋈ Detalhes) |
| Seleção: igualdade (lote) | 5 | 5,2% | 4 de 5 | +187,1% (B+ primária) |
| Seleção: faixa | 16 | 10,0% | 8 de 16 | −51,5% (clustering sobre hash) |

As fórmulas acertam a maioria das junções com menos de 1% de erro. Os erros grandes têm causas identificadas, todas no modelo de custo e não na medida:
- **Aluno ⋈ Detalhes (sort-merge e hash):** a estimativa calcula os blocos das runs e partições como ⌈n/bfr⌉, com bfr pelo tamanho **máximo** do registro. Detalhes tem um campo VarChar, então a estimativa supõe 1.334 blocos, contra 542 reais, e prevê um passe recursivo que não acontece.
- **Hash join com Inscrição como build (+30%):** o termo 4·n_h, que conta blocos parcialmente cheios, cresce com as 961 subpartições da recursão.
- **B+ primária, igualdade (+187%):** a fórmula conta 3 blocos por chave, a altura da árvore, mas os nós internos ficam no buffer.
- **Faixas sobre B+ (de −25% a −38%):** a fórmula supõe folhas cheias, mas a carga usa 70% de ocupação.
- **Clustering sobre hash (−51,5%):** a fórmula ignora as cadeias de overflow do hash por curso.

### 4.5 Ocupação depois da carga de manutenção

A carga de manutenção faz 5% de inserções e 1% de remoções nas variantes de Aluno. Cada organização reage de um jeito:

| Estrutura | Total antes | Total depois | Δ | Como cresceu |
|---|---:|---:|---:|---|
| B+ primária (dre) | 959 | 1.021 | +6,5% | os DREs novos são maiores que todos e caem na última folha, que se divide (+62 folhas) |
| Heap | 647 | 679 | +4,9% | blocos novos no fim |
| Hash primário (dre) | 897 | 935 | +4,2% | overflow: 87 → 125 blocos |
| Hash primário (curso) | 792 | 828 | +4,5% | overflow: 580 → 616 blocos |
| B+ secundária (cr) | 156 | 181 | +16,0% | listas de RIDs com overflow: 93 → 117 blocos |
| B+ primária (curso) | 959 | 961 | +0,2% | as inserções se espalham pelos cursos e cabem nos 30% livres das folhas |

### 4.6 Células que não rodam

Por política, 66 células da matriz do enunciado aparecem como `nao_aplicavel`, com o motivo no CSV:

| Caso | Células | O que é |
|---|---:|---|
| 1 | 54 | Junções dos tipos 2 e 3 nas configurações b, c e d: os campos "não ditam a organização", mas essas configurações exigem que ditem |
| 2 | 2 | Faixa em UNIQUE com índice secundário: o enunciado só admite hash nesse caso, e hash não responde a faixa |
| 3 | 6 | Igualdade em UNIQUE com 0,1% e 50%: ela devolve 1 registro, e por isso é medida no nível "pontual" |
| 4 | 4 | Faixa sobre hash primário |

Os detalhes estão na §9 do [handoff](PrototipoIA/HANDOFF_ARQUITETURA_2026-11-08.md).

## 5. Limitações conhecidas

- **Código ainda não revisado.** É um protótipo gerado por IA. A revisão do código, os testes de estresse e a análise crítica (§7 do enunciado) ainda são tarefa do grupo.
- **Plataforma.** Foi testado só no macOS: com `make`, com CMake e com ASan/UBSan. No Linux e no WSL2, a plataforma oficial do grupo, ainda não foi testado.
- **Modelo de custo.** Ele tem os desvios da §4.4, que podem ser corrigidos usando os blocos reais de cada tabela no lugar de ⌈n/bfr⌉ e o fill da B+.
- **Recursos desligados.** O prefetch e o toss-immediate ficam desligados em todas as configurações. A ordenação dos RIDs antes do fetch existe, mas não é usada na especificação padrão.
- **Escala reduzida.** Com `--escala` abaixo de cerca de 0,72, Turma passa a caber em M e o equijoin NOT UNIQUE muda de "maior ⋈ maior" para "menor ⋈ maior". A bancada avisa e grava o tamanho calculado.

## 6. Mapa do repositório

| Caminho | Conteúdo |
|---|---|
| [`PrototipoIA/Implementacao/`](PrototipoIA/Implementacao/) | Código C++20 (`src/`), testes (`tests/`), scripts Python (`tools/`), `Makefile`, `CMakeLists.txt` e uma rodada completa em `resultados_exemplo/` |
| [`PrototipoIA/Implementacao/README.md`](PrototipoIA/Implementacao/README.md) | Detalhes do protótipo: opções da bancada, onde está cada parte e desvios do handoff |
| [`PrototipoIA/Implementacao/PROMPTS.md`](PrototipoIA/Implementacao/PROMPTS.md) | Log de prompts da geração do código |
| [`PrototipoIA/HANDOFF_ARQUITETURA_2026-11-08.md`](PrototipoIA/HANDOFF_ARQUITETURA_2026-11-08.md) | Arquitetura completa e as decisões 1 a 45 e 100 a 171 |
| [`PrototipoIA/DECISOES_DO_ENUNCIADO_2026-11-08.md`](PrototipoIA/DECISOES_DO_ENUNCIADO_2026-11-08.md) | As 45 decisões que o enunciado pede, com o que foi decidido |
| [`PrototipoIA/DECISOES_POR_CATEGORIA_2026-11-08.md`](PrototipoIA/DECISOES_POR_CATEGORIA_2026-11-08.md) | As decisões separadas em arquitetura e implementação, por importância |
| [`PrototipoIA/METODOS_DE_INTERFACE_2026-11-08.md`](PrototipoIA/METODOS_DE_INTERFACE_2026-11-08.md) | Os métodos de cada interface entre camadas |
| [`PrototipoIA/diagrama_camadas_v5.png`](PrototipoIA/diagrama_camadas_v5.png) | Diagrama das camadas (há também o `.svg` e a versão anterior, v4) |
| [`PrototipoIA/revisao_interfaces_claude_cbd_2026-11-06.md`](PrototipoIA/revisao_interfaces_claude_cbd_2026-11-06.md) | Revisão das interfaces feita por um colega |
| [`PrototipoIA/Avaliacao_2_Indexacao_e_Processamento_de_Consultas.pdf`](PrototipoIA/Avaliacao_2_Indexacao_e_Processamento_de_Consultas.pdf) | Enunciado |

# Bancada de teste de CBD: protótipo gerado por IA

Este é um protótipo funcional da arquitetura descrita em `../HANDOFF_ARQUITETURA_2026-11-08.md` (decisões 1 a 171, com as emendas de 08/11) e no diagrama `../diagrama_camadas_v5.png`. É **código bruto gerado por IA**, ainda sem revisão do grupo. O `PROMPTS.md` traz o log de prompts e as correções que a própria IA fez durante a geração.

Estado em 08/11/2026:
- **compila sem avisos** com Apple clang 21 (`-Wall -Wextra -Wpedantic -Wshadow`);
- **40 testes passam**, também com ASan e UBSan;
- a bancada roda na escala do handoff (Aluno com 20.000 linhas, Inscrição com 100.000) em cerca de 25 s;
- todas as **358 linhas medidas batem com o SQLite**, em número de linhas e checksum. Cada combinação repete até as contagens e o tempo variarem menos de 5% (de 3 a 10 vezes; decisão 132 emendada);
- as células que não rodam aparecem no resultado com o motivo (decisão 170 do handoff): 132 com status `nao_aplicavel` (casos 1 a 4). Nenhuma célula fica `inviavel` desde o particionamento recursivo do hash join (decisão 171).

## Como rodar

```bash
make                 # build/bancada e build/testes
make test            # 40 testes (shim com a sintaxe do GoogleTest)
make asan            # testes com AddressSanitizer + UBSan
make rapido          # bancada em escala 0,1 + reference.py + compare.py + tabelas.py
```

Rodada completa, com conferência e tabelas:

```bash
./build/bancada --saida saida
python3 -I tools/reference.py saida
python3 -I tools/compare.py saida
python3 -I tools/tabelas.py saida     # saida/tabelas.md e saida/tabelas/*.csv
```

O `tabelas.py` aceita `--politica MRU` (as tabelas principais saem com a outra política), `--md ARQUIVO`, `--csv PASTA` e `--sem-csv`.

Opções da bancada:

| Opção | O que faz |
|---|---|
| `--escala X` | multiplica os volumes |
| `--reps-min N` / `--reps-max N` | limites de repetição (padrão 3 e 10); a tolerância vale para as contagens e para o tempo |
| `--tolerancia X` | variação máxima entre repetições (padrão 0,05) |
| `--politicas lru,mru` | políticas a comparar |
| `--m-juncao N` / `--m-selecao N` | M das junções e das seleções (padrão 32 e 2) |
| `--lote N` | chaves por lote de seleção pontual (padrão 1000) |
| `--so TEXTO` | roda só as operações cujo nome contém o texto |
| `--rapido` | escala 0,1, uma repetição, lote de 100 |
| `--tuplas` | grava as tuplas do motor para diff com `compare.py --tuplas` (166) |

O build também pode ser feito com CMake (3.20 ou mais novo). Ele usa o GoogleTest real se estiver instalado; sem ele, usa o shim. Foi testado no macOS com o CMake 4.4 e o Ninja que vêm com o CLion: compila e o `ctest` passa. No Linux e no WSL2 (plataforma oficial, decisão 156), ainda não foi testado:

```bash
cmake -S . -B build-cmake && cmake --build build-cmake && ctest --test-dir build-cmake
```

## Onde está cada parte

| Camada | Arquivos | Entidades |
|---|---|---|
| common/ | `src/common/common.{hpp,cpp}` | BlockId, RID, Intent, Key, Predicate, FieldDef/Schema, PrimarySpec/IndexSpec, IoCounts/Counters, Config, Stats, AccessPath, Occupancy, OpResult, hash |
| 4 | `src/layer4/posix_io.*` | UniqueFd, create_excl, open_existing, pread_full, pwrite_full |
| 4 | `src/layer4/file_manager.*` | FileManager: cabeçalho e bitmap no bloco 0, extents, contagem sequencial/aleatória com cabeça global |
| 4 | `src/layer4/replacement_policy.*` | LRU e MRU com toss-immediate, uma instância por região |
| 4 | `src/layer4/buffer_pool.*` | BufferPool (4 áreas + subpools), PageGuard, Reservation, migração, prefetch |
| 3 | `src/layer3/record.*` | formato do registro (158), RecordView verificada, KeyCodec |
| 3 | `src/layer3/page.*` | cabeçalho de 24 B, SlottedPage única, nó interno da B+ |
| 3 | `src/layer3/bplus.*` | motor B+: bulk load, divisão, redistribuição, fusão, verificação de invariantes |
| 3 | `src/layer3/structures.*` | HeapFile, HashFile, BPlusTreeFile, HashIndex, BPlusIndex (posting lists com overflow, clustering), estados de busca |
| 3 | `src/layer3/handles.*` | BlockScan, BlockCursor, SearchCursor, RidCursor, PinnedRecord, TempFile, ScratchSpace |
| 3 | `src/layer3/catalog.*`, `storage_manager.*` | Catalog (página fixada), StorageManager (fachada, MoveListener → índices, reorganize) |
| 2 | `src/layer2/monitor.*` | Monitor, JoinPlan/SelectPlan com orçamento, ResultSink (checksum canônico), PhaseRecorder |
| 2 | `src/layer2/algorithms.*` | BNL, sort-merge (runs, passes, grupo de duplicatas), partition-hash (com entrada pré-particionada, particionamento recursivo e bailout, decisão 171), seleções, estimativas |
| 1 | `src/layer1/bench.cpp` | gerador da base, especificação padrão, constantes por quantis, lotes, repetições, CSVs, manifesto |
| — | `tools/reference.py`, `tools/compare.py` | conferência com o SQLite (147, 166) |
| — | `tools/tabelas.py` | tabelas de análise a partir de uma saída (Markdown e CSV) |
| — | `tests/` | 40 testes das camadas 2, 3 e 4 |

## Saídas da bancada

| Arquivo | Conteúdo |
|---|---|
| `resultados.csv` | uma linha por política, operação, variante, algoritmo e repetição. Traz a posição na matriz do enunciado (`tipo`, `tamanho`, `config` a–e, `seletividade` pontual/0,1%/50%), o `status` (`ok`, `nao_aplicavel` ou `inviavel`, decisão 170) e o `motivo` de cada célula que não roda. Nas células executadas: linhas, checksum, requisições, acertos, faltas, taxa de acerto, leituras e gravações sequenciais/aleatórias, expulsões sujas, gravações forçadas, alocações, leituras de índice, E/S de temporários, custo estimado (transferências e seeks), `flush_writes` (deve ser 0) e tempo |
| `ocupacao.csv` | blocos por estrutura, antes e depois da carga de manutenção (§6.1.1) |
| `estatisticas.csv` | n, R, bfr, b, altura, nós internos, folhas, distintos, buckets, overflow, fator de carga e cadeias, todos medidos pelo código |
| `fases.csv` | E/S por fase de cada operação (runs, passes, particionamento, build/probe…) |
| `operacoes.csv`, `constantes.csv`, `lotes/` | SQL equivalente, constantes de faixa pelos quantis e lotes de chaves |
| `dados/`, `esquema.csv`, `manifesto.txt` | a base gerada, o esquema e o manifesto da execução |

Uma rodada completa está em `resultados_exemplo/` (sem os dados e os arquivos do motor), com as tabelas do `tabelas.py` em `resultados_exemplo/tabelas.md` e `resultados_exemplo/tabelas/`.

## Primeiros resultados (escala 1, LRU, médias das repetições)

Valores medidos em `resultados_exemplo/`. São só ilustrativos; a análise é tarefa do grupo.

| Operação | Caminho | Leituras | Estimado | Observação |
|---|---|---:|---:|---|
| 1.000 pontos por DRE | heap, varredura | 323.962 | 323.000 | b/2 por chave |
| 1.000 pontos por DRE | B+ primária | 1.046 | 3.000 | nós internos ficam na área de índices: taxa de acerto de 0,66 |
| 1.000 pontos por DRE | hash primário | 1.007 | 1.108 | cadeia média de 1,1 |
| CR ≤ mediana (50%) | heap, varredura | 646 | 646 | |
| CR ≤ mediana (50%) | B+ secundária não clustering | 9.715 | 10.046 | o índice perde da varredura (resultado central, §11) |
| curso ≤ 100 (50%) | clustering sobre B+ | 482 (479 seq.) | 326 | blocos sequenciais |
| curso ≤ 100 (50%) | clustering sobre hash | 672 (todas aleatórias) | 326 | o mesmo resultado, com acesso aleatório (§11) |
| Aluno ⋈ Inscrição | BNL | 70.141 (MRU: 69.525) | 70.141 | |
| Aluno ⋈ Inscrição | sort-merge | 6.404 + 4.063 gravações | 10.413 | |
| Aluno ⋈ Inscrição | partition-hash | 4.703 + 2.362 gravações | 7.127 | |

Números derivados do layout, sem nenhum valor escrito à mão:
- Aluno: R = 127 e bfr = 31; o heap tem b = 646.
- Hash de Aluno por DRE: 809 buckets, α = 0,80.
- Hash de Aluno por curso (decisão 163): 211 buckets, b = 791.
- B+ de Aluno por DRE com fill 0,7: 953 folhas, altura 3.

## Desvios do handoff

Cada desvio fica registrado aqui, para o grupo decidir se o mantém.

1. **Especificação em C++, não em TOML.** A especificação fica em `spec_padrao()`, em `bench.cpp`, e o hash dela vai para o manifesto. A decisão 129 foi emendada para aceitar isso; o TOML ficou opcional.
2. **Testes com shim (`tests/mini_test.hpp`).** O GoogleTest não estava instalado. O shim usa a mesma sintaxe, e o CMake troca pelo GoogleTest real quando ele existir.
3. **Plataforma.** O protótipo foi compilado e testado no macOS (arm64). O Linux/WSL2, que é a plataforma oficial, não foi testado aqui.
4. **Entrada de slot de 4 B e ponteiro de filho de 4 B.** Daí vêm bfr = 31 para Aluno (R = 127) e 509 chaves por nó interno com chave int32. O handoff estimava 339 chaves com ponteiro de 8 B.
5. **Repasses novos para a manutenção e a carga:**
   - `erase_key(tabela, campo, chave)`: a camada 1 não tem como acompanhar RIDs que mudam com as divisões da B+;
   - `encode(tabela, valores)`: o formato do registro é da camada 3;
   - `suggest_buckets(...)`: a fórmula das decisões 39 e 163 precisa da capacidade do bucket, que vem do layout.
6. **Seleção pontual pela varredura em campo UNIQUE para na primeira ocorrência** (b/2 por chave, como nas fórmulas). O handoff foi alinhado a isso na emenda da decisão 146.
7. **BNL com diretório ordenado das chaves do chunk.** É um vetor limitado aos registros das páginas fixadas, como os diretórios do ScratchSpace (§9). Não muda a E/S.
8. **Índice clustering:** cada entrada guarda o primeiro RID do grupo. Inserções e remoções recalculam esse RID com `first_of_group`, o que tem custo só na manutenção.
9. **Catálogo:** guarda só um resumo textual na página fixada. A base não é reaberta entre execuções.
10. **Células não aplicáveis (decisão 170):** os casos 1 a 3 (§9 do handoff) são declarados pela bancada como `nao_aplicavel`, sem executar. A junção por CPF com Detalhes em hash (tipo 2, configuração d) saiu da execução, porque é um caso 1, e a variante `detalhes_h_cpf` foi removida.

## Limitações conhecidas e próximos passos

- A matriz completa do §5.1 ainda não está na especificação padrão. Sem os casos 1 a 3, sobram 27 células de junção. Hoje rodam:
  - no natural join maior ⋈ maior: as configurações a, b, c (B+ e hash) e d;
  - nos outros tamanhos do natural join: a configuração a em menor ⋈ menor, e a e b em menor ⋈ maior;
  - nos tipos 2 e 3: só a célula maior ⋈ maior (configuração a no tipo 2, e no tipo 3).
- O teste do intervalo de medição (149: N gravações provocadas depois do snapshot final) precisa de um gancho de teste que ainda não existe.
- As estimativas de seeks são aproximadas. A comparação estimado × medido deve usar sobretudo as transferências.
- **Hash sobre registros variáveis** (Detalhes por CPF) é dimensionado pelo tamanho máximo do registro (256 B). O resultado é 1.667 buckets, quase todos pouco ocupados, e b = 1.667 contra 542 do heap. Vale uma decisão do grupo, no espírito da 163.
- As estimativas de junção usam b para ler as estruturas e os blocos densos ⌈n/bfr⌉ para runs e partições (emenda da decisão 143). A maior diferença restante é o hash join com Inscrição como build (11.927 medidos contra 15.549 estimados): o termo 4·n_h do livro supõe 961 partições com blocos parcialmente cheios. É uma diferença a explicar no relatório (§6.1.3).
- Prefetch, toss-immediate, carga da B+ a 100% e compactação automática não são exercitados pela bancada. A lista completa está na §9 do handoff, em "Decisões que a bancada não exercita". O prefetch do protótipo segue o número do bloco, e não a ordem do percurso; teria de ser corrigido antes de virar fator.
- Blocagem estendida (spanned) não foi implementada, e chaves de índice VarChar não são suportadas (questão aberta na §12).
- O `POST_MORTEM.md` e o code review do grupo ainda estão por fazer, a partir deste código bruto (veja `PROMPTS.md`).

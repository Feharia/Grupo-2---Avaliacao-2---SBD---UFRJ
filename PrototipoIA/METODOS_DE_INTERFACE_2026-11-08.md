# Métodos das interfaces entre camadas

Estado em 08/11/2026. Este arquivo lista os métodos implementados no protótipo (`Implementacao/src`) que uma camada chama em outra. Uma interface **X ↔ Y** é uma fronteira entre duas camadas: a camada de cima chama e a de baixo implementa. A única exceção é o callback `RecordStream`, em que a camada 3 chama de volta um objeto da camada 1. Os números entre parênteses são as decisões do `HANDOFF_ARQUITETURA_2026-11-08.md`.

**O que entra**
- Os métodos públicos que a camada vizinha chama.
- Os métodos dos objetos que atravessam a fronteira: cursores, guards, views, arquivos temporários e reservas.
- Os destrutores que fazem trabalho visível do outro lado, como soltar uma página fixada ou apagar um arquivo temporário.

**O que fica de fora**
- Chamadas dentro de uma mesma camada:
  - Monitor → `algorithms.cpp`, `ResultSink` e `PhaseRecorder`, na camada 2;
  - estruturas e catálogo, na camada 3;
  - BufferPool → `FileManager` → funções POSIX, e `ReplacementPolicy`, na camada 4.
- As funções auxiliares de `common/`, como `compare_keys` e `KeyRange::contains`.
- Construtores, cópia e move.
- A camada 4 também conversa com o SO por `pread` e `pwrite`, mas o SO não é uma camada da bancada.

**Marca †:** método público que a camada de cima não chama hoje. Ele é usado só pelos testes, só dentro da própria camada, ou por ninguém. A lista completa está no fim.

| Interface | Classes | Métodos |
|---|---|---|
| 1 ↔ 2 | `Monitor`, e o callback `RecordStream` (common, implementado na 1) | 24 |
| 2 ↔ 3 | `StorageManager`, `BlockScan`, `BlockCursor`, `SearchCursor`, `RidCursor`, `RecordView`, `TempFile`, `ScratchSpace` e `ScratchMatches` | 65 |
| 3 ↔ 4 | `BufferPool`, `PageGuard` e `Reservation` | 29 |

---

## 1 ↔ 2 — Bancada ↔ Monitor

A camada 1 só enxerga o `Monitor`. Ele mede as operações (join, select_points e select_range) e repassa à camada 3 tudo o que é carga, catálogo e manutenção.

### Operações medidas e planos

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `join` — `Monitor` | 1 ↔ 2 | Executa uma junção medida. Primeiro planeja (`plan_join`, sem E/S). Se a combinação não se aplica, devolve `NotApplicable` sem tocar no disco (170). Se o plano usar mais que m molduras, lança erro (148). Senão: reserva m molduras; tira um snapshot dos contadores; roda o algoritmo (BNL, sort-merge ou hash); tira outro snapshot; exige zero pedidos de página de dados sem reserva (159); faz o flush de verificação; calcula a estimativa analítica. Se o hash join não couber em M mesmo com recursão, devolve `NotApplicable` do tipo `Inviavel` (151, 171). | `JoinAlgorithm alg`; `JoinInput outer, inner` (caminho de acesso, campo da junção, operando 0 ou 1); `size_t m` (molduras da operação); `ResultShape` (campos de cada operando que entram no checksum e arquivo de dump opcional); `JoinParams` (chunk do BNL, nº de partições do hash, páginas de grupo do merge) | `Outcome`, que é um `OpResult` (linhas, checksum, delta dos contadores, gravações do flush final, estimativa, fases e tempo) ou um `NotApplicable` (motivo e tipo, `NaoAplicavel` ou `Inviavel`) |
| `select_points` — `Monitor` | 1 ↔ 2 | Seleção por igualdade, medida, com um lote de um ou mais valores do mesmo campo. O caminho decide a rotina: no Scan, a varredura filtra na camada 2; no índice secundário não clustering (hash ou B+), `probe` + `fetch`; na B+ primária, no hash primário e na B+ clustering, `find`. Devolve `NotApplicable` se o campo do predicado não for o do caminho. Mesmo protocolo de medida do `join`. | `AccessPath`; `span<const Point> batch` (todos no mesmo campo); `ResultShape`; `SelectParams` (m, linhas esperadas, ordenar RIDs antes do fetch) | `Outcome` |
| `select_range` — `Monitor` | 1 ↔ 2 | Seleção por faixa, medida. No Scan, varredura com filtro; na B+ secundária não clustering, `probe` + `fetch`, com ordenação opcional dos RIDs; na B+ primária e na clustering, `find` sequencial. Num caminho sem suporte a faixa, como o hash, devolve `NotApplicable` pelo `plan_select`. | `AccessPath`; `KeyRange` (campo e limites `lo` e `hi` opcionais, inclusivos ou não); `ResultShape`; `SelectParams` | `Outcome` |
| `plan_join` † — `Monitor` | 1 ↔ 2 | Monta o plano da junção sem E/S. Decide os papéis (troca externa e interna; no hash, a menor vira a build), o chunk do BNL, as runs e os passes do merge, as partições e os níveis do hash (171) e os blocos densos ⌈n/bfr⌉ (emenda da 143). Devolve `NotApplicable` nas células sem sentido (170). É chamado pelo `join`; a camada 1 não chama. | `alg`, `outer`, `inner`, `m`, `JoinParams` | `variant<JoinPlan, NotApplicable>` |
| `plan_select` † — `Monitor` | 1 ↔ 2 | Monta o plano da seleção sem E/S: caminho, filtro na camada 2, tamanho do lote, molduras, páginas de rascunho para RIDs (117) e linhas esperadas. Devolve `NotApplicable` quando o caminho não suporta o predicado. | `AccessPath`, `bool range`, `size_t batch`, `SelectParams` | `variant<SelectPlan, NotApplicable>` |

### Repasses à camada 3

Esses métodos chamam o método de mesmo nome do `StorageManager` sem lógica própria. A descrição completa está na seção 2 ↔ 3.

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `create_table` — `Monitor` | 1 ↔ 2 (repasse à 3) | Cria a tabela com a sua organização primária. | `string name`, `Schema`, `PrimarySpec` | `TableId` |
| `create_index` — `Monitor` | 1 ↔ 2 (repasse à 3) | Cria um índice secundário. | `TableId`, `IndexSpec` | `StructureId` |
| `access_paths` — `Monitor` | 1 ↔ 2 (repasse à 3) | Lista os caminhos de acesso da tabela. | `TableId` | `vector<AccessPath>` |
| `schema` — `Monitor` | 1 ↔ 2 (repasse à 3) | Devolve o esquema da tabela. | `TableId` | `const Schema&` |
| `stats` — `Monitor` | 1 ↔ 2 (repasse à 3) | Devolve as estatísticas do catálogo, sem E/S. | `TableId` | `TableStats` |
| `occupancy` — `Monitor` | 1 ↔ 2 (repasse à 3) | Mede a ocupação de uma estrutura. | `StructureId` | `Occupancy` |
| `structures` — `Monitor` | 1 ↔ 2 (repasse à 3) | Lista as estruturas da tabela. | `TableId` | `vector<StructureId>` |
| `structure_name` — `Monitor` | 1 ↔ 2 (repasse à 3) | Devolve o nome legível de uma estrutura. | `StructureId` | `string` |
| `bulk_load` — `Monitor` | 1 ↔ 2 (repasse à 3) | Carrega a tabela a partir de um fluxo ordenado. | `TableId`, `RecordStream&` | — |
| `insert` — `Monitor` | 1 ↔ 2 (repasse à 3) | Insere um registro codificado. | `TableId`, `span<const byte>` | `RID` |
| `erase` † — `Monitor` | 1 ↔ 2 (repasse à 3) | Remove um registro pelo RID. Só os testes chamam. | `TableId`, `RID` | — |
| `erase_key` — `Monitor` | 1 ↔ 2 (repasse à 3) | Remove todos os registros com a chave dada. | `TableId`, `FieldId`, `Key` | `size_t` (quantos removeu) |
| `reorganize` † — `Monitor` | 1 ↔ 2 (repasse à 3) | Reconstrói a organização primária. Só os testes chamam. | `TableId` | — |
| `encode` — `Monitor` | 1 ↔ 2 (repasse à 3) | Codifica uma linha no formato de registro. | `TableId`, `vector<Value>` | `vector<byte>` |
| `suggest_buckets` — `Monitor` | 1 ↔ 2 (repasse à 3) | Calcula o nº de buckets do hash primário. | `Schema`, `uint64 n`, `uint64 d`, `double alpha` | `uint32` |
| `verify` — `Monitor` | 1 ↔ 2 (repasse à 3) | Confere os invariantes das estruturas. | `TableId` | `string` (vazia se estiver tudo certo) |
| `flush_all` — `Monitor` | 1 ↔ 2 (repasse à 3 e à 4) | Grava as páginas sujas e os cabeçalhos. | — | `uint64` (gravações) |
| `invalidate_all` — `Monitor` | 1 ↔ 2 (repasse à 3 e à 4) | Esvazia o buffer, exceto o catálogo, para a próxima medida começar com cache fria. | — | — |

### Callback da carga

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `next` — `RecordStream` (abstrata em common; `RowsStream` na camada 1) | 1 ↔ 3, passando pela 2 no `bulk_load` | A camada 3 puxa da camada 1 o próximo registro já codificado e ordenado pela chave da organização, um por vez, durante a carga em bloco (158). | — | `optional<span<const byte>>`: o registro, ou vazio no fim |

---

## 2 ↔ 3 — Monitor e algoritmos ↔ StorageManager

A camada 2 não vê páginas nem arquivos. Ela pede varreduras, buscas e memória de trabalho e recebe cursores que entregam `RecordView`. A `Reservation` é uma classe da camada 4 que atravessa a camada 2 de forma opaca: a 2 a recebe de `reserve` e a repassa à 3, sem chamar métodos dela. Quem lê o `id()` é a camada 3 (159).

### StorageManager — leitura usada pelos algoritmos

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `scan_blocks` — `StorageManager` | 2 ↔ 3 | Abre a varredura de todos os blocos de dados da organização primária: o heap na ordem física, a B+ pela cadeia de folhas e o hash pelos buckets e pelas cadeias de overflow. | `TableId`; `Intent` (`reuse` e `sequential`); `Reservation` | `BlockScan` |
| `scan_buckets` — `StorageManager` | 2 ↔ 3 | Varre só os buckets `first..last` do hash primário, com o overflow. O hash join usa para ler uma entrada já particionada pelo campo da junção. Lança se o caminho não for hash primário ou se o intervalo for inválido. | `AccessPath` (PrimaryHash), `uint32 first`, `uint32 last`, `Intent`, `Reservation` | `BlockScan` |
| `bucket_of` — `StorageManager` | 2 ↔ 3 | Diz em que bucket do hash primário cai uma chave, sem E/S. | `AccessPath` (PrimaryHash), `Key` | `uint32` |
| `find` — `StorageManager` | 2 ↔ 3 | Busca registros pelo próprio arquivo de dados. Na B+ primária, desce a árvore e segue as folhas; no hash primário, lê o bucket e o overflow; na B+ clustering, desce o índice e lê os blocos de dados em sequência. Para no primeiro achado quando o campo é UNIQUE (157). Lança em outro tipo de caminho ou se o campo do predicado não for o do caminho. | `AccessPath`; `Predicate` (`Point` ou `KeyRange`); `Intent`; `Reservation` | `SearchCursor` |
| `probe` — `StorageManager` | 2 ↔ 3 | Consulta um índice secundário não clustering (hash ou B+) e devolve só os RIDs, sem ler o arquivo de dados. Não recebe reserva: as páginas do índice ficam na área de Índices. | `AccessPath` (SecondaryHash ou SecondaryBTreeNonClustering), `Predicate`, `Intent` | `RidCursor` |
| `fetch` — `StorageManager` | 2 ↔ 3 | Lê no arquivo de dados primário o registro de um RID e o devolve com a página fixada. Lança se o RID apontar para um registro removido. | `TableId`, `RID`, `Intent`, `Reservation` | `PinnedRecord` (`PageGuard` + `RecordView`): a página fica fixada enquanto o objeto existir |
| `schema` — `StorageManager` | 2 ↔ 3 | Devolve o esquema: campos, tipos, offsets, UNIQUE e anulável. Sem E/S. | `TableId` | `const Schema&` |
| `stats` — `StorageManager` | 2 ↔ 3 | Lê do catálogo n, r, bfr, b e os dados de cada estrutura (altura, folhas, distintos, buckets, overflow e cadeias). Sem E/S (160). | `TableId` | `TableStats` |

### StorageManager — memória de trabalho

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `create_temp` — `StorageManager` | 2 ↔ 3 | Cria um arquivo temporário (`FileKind::Temp`) para runs do merge e partições do hash. As páginas são alocadas no subpool da reserva. Guarda uma cópia do esquema (165). | `const Schema&`, `Reservation` | `TempFile` |
| `create_scratch` — `StorageManager` | 2 ↔ 3 | Fixa `pages` molduras anônimas da reserva como área de rascunho para registros, usada no bloco do BNL, na ordenação de runs e na tabela hash da build. Não gera E/S. Lança com 0 páginas. | `const Schema&`, `Reservation`, `size_t pages` | `ScratchSpace` |
| `create_rid_scratch` — `StorageManager` | 2 ↔ 3 | O mesmo que `create_scratch`, mas para RIDs de 8 bytes. Serve para ordenar RIDs antes do `fetch` (117). | `Reservation`, `size_t pages` | `ScratchSpace` |

### StorageManager — repasses à camada 4

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `reserve` — `StorageManager` | 2 ↔ 3 (repasse à 4) | Repassa a `BufferPool::reserve`: cria o subpool de m molduras da operação medida. | `size_t frames` | `Reservation` |
| `snapshot` — `StorageManager` | 2 ↔ 3 (repasse à 4) | Repassa a `BufferPool::snapshot`: devolve os contadores de E/S acumulados. O Monitor subtrai dois snapshots. | — | `Counters` |
| `flush_all` — `StorageManager` | 2 ↔ 3 (repasse à 4) | Grava as páginas sujas e os cabeçalhos. No fim da medida, deve gravar 0. | — | `uint64` (gravações) |
| `invalidate_all` — `StorageManager` | 2 ↔ 3 (repasse à 4) | Esvazia o buffer, exceto o catálogo. | — | — |

### StorageManager — carga, catálogo e manutenção (chamados pela camada 1 via Monitor)

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `create_table` — `StorageManager` | 2 ↔ 3 | Cria a tabela com a organização primária (heap, B+ ou hash com o nº de buckets dado) e a registra no catálogo. | `string name`; `Schema`; `PrimarySpec` (tipo, chave, fill, buckets e função hash) | `TableId` |
| `create_index` — `StorageManager` | 2 ↔ 3 | Registra um índice secundário: hash só em campo UNIQUE (157); B+ clustering só no campo da organização de uma B+ ou de um hash primários (116). O índice é construído na carga. | `TableId`; `IndexSpec` (tipo, campo, clustering) | `StructureId` |
| `access_paths` — `StorageManager` | 2 ↔ 3 | Lista os caminhos da tabela (Scan, a primária e cada índice), cada um com suporte a ponto e a faixa, ordem e particionamento por hash. Sem E/S. | `TableId` | `vector<AccessPath>` |
| `occupancy` — `StorageManager` | 2 ↔ 3 | Mede a ocupação de uma estrutura: blocos de dados, overflow, cabeçalho, diretório e blocos de cada nível do índice. | `StructureId` | `Occupancy` |
| `structures` — `StorageManager` | 2 ↔ 3 | Devolve o id da primária e os dos índices da tabela. | `TableId` | `vector<StructureId>` |
| `structure_name` — `StorageManager` | 2 ↔ 3 | Nome legível de uma estrutura, usado no CSV. | `StructureId` | `string` |
| `bulk_load` — `StorageManager` | 2 ↔ 3 | Carrega a tabela a partir de um fluxo ordenado pela chave da organização. Depois constrói os índices, atualiza as estatísticas e persiste o catálogo. | `TableId`, `RecordStream&` | — |
| `insert` — `StorageManager` | 2 ↔ 3 | Confere a unicidade nos índices dos campos UNIQUE (lança `DuplicateKey`, 157), grava o registro na primária e atualiza os índices. | `TableId`, `span<const byte>` (registro codificado) | `RID` |
| `erase` — `StorageManager` | 2 ↔ 3 | Remove o registro do RID e atualiza os índices. | `TableId`, `RID` | — |
| `erase_key` — `StorageManager` | 2 ↔ 3 | Remove todos os registros com a chave no campo. Localiza um de cada vez, porque a B+ pode mover registros ao remover. | `TableId`, `FieldId`, `Key` | `size_t` (quantos removeu) |
| `reorganize` — `StorageManager` | 2 ↔ 3 | Reconstrói a organização primária por carga em bloco a partir dos registros atuais, apaga os arquivos antigos e reconstrói os índices, porque os RIDs mudam (141). | `TableId` | — |
| `encode` — `StorageManager` | 2 ↔ 3 | Codifica uma linha no formato de registro da tabela, com null bitmap e VarChar (158). | `TableId`, `vector<Value>` (`monostate` = NULL) | `vector<byte>` |
| `suggest_buckets` — `StorageManager` | 2 ↔ 3 | Calcula o nº de buckets do hash para n registros, d valores distintos e ocupação alvo alpha (39, 163). | `const Schema&`, `uint64 n`, `uint64 distinct`, `double alpha` | `uint32` |
| `verify` — `StorageManager` | 2 ↔ 3 | Confere os invariantes das estruturas: ordem, ocupação mínima, contagens e índices × dados. | `TableId` | `string` (vazia se estiver tudo certo) |
| `block_size` † — `StorageManager` | 2 ↔ 3 | Tamanho do bloco. Só os testes chamam. | — | `uint32` |
| `tables` † — `StorageManager` | 2 ↔ 3 | Nº de tabelas no catálogo. Ninguém fora da camada 3 chama. | — | `size_t` |
| `table_name` † — `StorageManager` | 2 ↔ 3 | Nome de uma tabela. Ninguém fora da camada 3 chama. | `TableId` | `const string&` |

### Cursores

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `next` — `BlockScan` | 2 ↔ 3 | Fixa o próximo bloco do plano de varredura (sequencial, cadeia de folhas ou cadeia de buckets) e devolve um cursor sobre ele. | — | `optional<BlockCursor>` (vazio no fim) |
| `next` — `BlockCursor` | 2 ↔ 3 | Devolve o próximo registro vivo do bloco fixado. A camada 2 solta o bloco destruindo o cursor. | — | `optional<RecordView>` (vale enquanto o cursor existir) |
| `block` † — `BlockCursor` | 2 ↔ 3 | Id do bloco fixado. Ninguém chama. | — | `BlockId` |
| `last_rid` † — `BlockCursor` | 2 ↔ 3 | RID do último registro devolvido. Só a camada 3 usa. | — | `RID` |
| `next` — `SearchCursor` | 2 ↔ 3 | Próximo registro que satisfaz o predicado do `find`. | — | `optional<RecordView>` (vale até a próxima chamada) |
| `next` — `RidCursor` | 2 ↔ 3 | Próximo RID do índice consultado pelo `probe`. | — | `optional<RID>` |

### RecordView (leitura de campos)

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `is_null` — `RecordView` | 2 ↔ 3 | Lê o bit do campo no null bitmap. É falso se o campo não for anulável. | `FieldId` | `bool` |
| `key` — `RecordView` | 2 ↔ 3 | Lê o campo como chave comparável: int32, int64 ou string. Lança se o campo for NULL. | `FieldId` | `Key` |
| `schema` — `RecordView` | 2 ↔ 3 | Esquema do registro. O `ResultSink` usa para codificar o checksum. | — | `const Schema*` |
| `get<T>` † — `RecordView` | 2 ↔ 3 | Lê um campo de tamanho fixo como T. Confere tipo, tamanho e limites e lança em campo NULL (158). Usado pela camada 3 e pelos testes. | `FieldId` | `T` |
| `get_var` † — `RecordView` | 2 ↔ 3 | Lê um VarChar e confere os offsets. Usado pela camada 3 e pelos testes. | `FieldId` | `span<const byte>` |
| `bytes` † — `RecordView` | 2 ↔ 3 | Bytes crus do registro. Usado dentro da camada 3, por exemplo no `TempFile::append`. | — | `span<const byte>` |
| `empty` † — `RecordView` | 2 ↔ 3 | Diz se a view está vazia (sem esquema). Usado dentro da camada 3 e pelos testes. | — | `bool` |

### TempFile (runs e partições)

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `append` — `TempFile` | 2 ↔ 3 | Acrescenta um registro na última página. Quando a página enche, solta-a (ela vira candidata no subpool) e aloca a seguinte. Lança depois do `seal`. | `const RecordView&` ou `span<const byte>` | — |
| `seal` — `TempFile` | 2 ↔ 3 | Fecha o arquivo: grava as páginas sujas, contadas como E/S de temporário, e as retira do pool (139). Necessário antes de ler. | — | — |
| `scan_blocks` — `TempFile` | 2 ↔ 3 | Varre o arquivo em ordem sequencial, usando a reserva do próprio arquivo. Lança antes do `seal`. | `Intent` | `BlockScan` |
| `blocks` — `TempFile` | 2 ↔ 3 | Nº de blocos gravados. Serve para decidir a recursão do hash join e para os passes do merge. | — | `uint32` |
| `~TempFile` — `TempFile` | 2 ↔ 3 | Apaga o arquivo sem gravar e sem contar E/S (155). Ter uma página fixada nesse momento é erro fatal. | — | — |
| `sealed` † — `TempFile` | 2 ↔ 3 | Diz se o arquivo já foi fechado. Ninguém chama. | — | `bool` |
| `records` † — `TempFile` | 2 ↔ 3 | Nº de registros gravados. Ninguém chama. | — | `uint64` |

### ScratchSpace e ScratchMatches (rascunho em memória, dentro da reserva)

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `add` — `ScratchSpace` | 2 ↔ 3 | Copia um registro (ou um RID, no espaço de RIDs) para as páginas de rascunho. | `const RecordView&` ou `RID` | `bool` (falso quando cheio) |
| `size` — `ScratchSpace` | 2 ↔ 3 | Nº de itens guardados. | — | `size_t` |
| `at` — `ScratchSpace` | 2 ↔ 3 | Item i, na ordem atual (após o `sort_by`, ordenado). | `size_t i` | `RecordView` |
| `rid_at` — `ScratchSpace` | 2 ↔ 3 | RID na posição i do espaço de RIDs. | `size_t i` | `RID` |
| `sort_by` — `ScratchSpace` | 2 ↔ 3 | Ordena os itens pelo campo, de forma estável e em memória, sem E/S. Gera as runs do sort-merge. | `FieldId` | — |
| `sort_rids` — `ScratchSpace` | 2 ↔ 3 | Ordena os RIDs por (bloco, slot), para o `fetch` seguir a ordem física (117). | — | — |
| `build_hash` — `ScratchSpace` | 2 ↔ 3 | Monta uma tabela hash em memória sobre o campo, com semente própria, independente da semente do particionamento (153). Chave NULL nunca entra (158). | `FieldId`, `uint64 seed` | — |
| `probe` — `ScratchSpace` | 2 ↔ 3 | Procura uma chave na tabela hash montada. | `const Key&` | `ScratchMatches` |
| `clear` — `ScratchSpace` | 2 ↔ 3 | Esvazia o rascunho e mantém as páginas, para reusar em outro bloco ou partição. | — | — |
| `~ScratchSpace` — `ScratchSpace` | 2 ↔ 3 | Solta as molduras anônimas, que voltam ao subpool. | — | — |
| `capacity` † — `ScratchSpace` | 2 ↔ 3 | Nº máximo de itens. Ninguém chama fora da camada 3. | — | `size_t` |
| `pages` † — `ScratchSpace` | 2 ↔ 3 | Nº de páginas fixadas. Ninguém chama. | — | `size_t` |
| `next` — `ScratchMatches` | 2 ↔ 3 | Próximo registro da tabela hash com a chave procurada. | — | `optional<RecordView>` |

---

## 3 ↔ 4 — StorageManager ↔ BufferPool

A camada 3 só lê e escreve páginas por meio de `PageGuard`. Ela não chama o `FileManager` nem as funções POSIX: isso é interno à camada 4.

### BufferPool

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `create` — `BufferPool` | 3 ↔ 4 | Cria um arquivo novo e grava o cabeçalho e o bitmap no bloco 0, exceto nos temporários. Falha se o arquivo já existir (161). | `const string& name`, `FileKind` (Data, Index, Catalog ou Temp) | `FileId` |
| `fetch` — `BufferPool` | 3 ↔ 4 | Fixa um bloco. Na falta, lê do disco e conta a leitura como sequencial ou aleatória pela cabeça de disco global. Com reserva, a página vai para o subpool; sem reserva, para a área do tipo de arquivo. Página de dados pedida sem reserva incrementa o contador que o Monitor exige ser 0 (159). Lança no bloco 0 ou fora do arquivo. | `BlockId`; `Intent` (`reuse` = falso com toss-immediate deixa a moldura como próxima vítima; `sequential` liga o prefetch); `ReservationId`, sem valor padrão (159) | `PageGuard` |
| `allocate` — `BufferPool` | 3 ↔ 4 | Aloca um bloco novo no bitmap do arquivo (nunca o 0) e o fixa sem ler do disco. | `FileId`, `ReservationId` | `pair<BlockId, PageGuard>` |
| `scratch` — `BufferPool` | 3 ↔ 4 | Fixa uma moldura anônima do subpool, sem bloco de disco. Nunca é gravada. Exige reserva. | `ReservationId` | `PageGuard` (anônimo) |
| `free_block` — `BufferPool` | 3 ↔ 4 | Libera o bloco no bitmap e descarta a página do pool sem gravar. Lança no bloco 0 ou em página fixada (168). | `BlockId` | — |
| `flush_file` — `BufferPool` | 3 ↔ 4 | Grava as páginas sujas do arquivo em ordem de bloco e, se pedido, as retira do pool. Usado no `TempFile::seal`. | `FileId`, `bool evict` | — |
| `remove_file` — `BufferPool` | 3 ↔ 4 | Descarta as páginas do arquivo sem gravar e sem contar, e apaga o arquivo (155). | `FileId` | — (`noexcept`) |
| `block_size` — `BufferPool` | 3 ↔ 4 | Tamanho do bloco em bytes. | — | `uint32` |
| `reserve` — `BufferPool` | 3 ↔ 4 | Tira n molduras da área de Trabalho, despejando vítimas se preciso, e cria um subpool com a política configurada. Lança se não houver molduras. | `size_t frames` (> 0) | `Reservation` |
| `snapshot` — `BufferPool` | 3 ↔ 4 | Copia os contadores de E/S: global, por tipo de arquivo e por arquivo. Conta pedidos, acertos, faltas, alocações, leituras e gravações sequenciais e aleatórias, despejos sujos e pedidos de dados sem reserva. | — | `Counters` |
| `flush_all` — `BufferPool` | 3 ↔ 4 | Grava todas as páginas sujas e os cabeçalhos sujos. | — | `uint64` (gravações) |
| `invalidate_all` — `BufferPool` | 3 ↔ 4 | Esvazia todas as molduras, exceto as do catálogo. Lança com reserva ativa, página fixada ou página suja (exige flush antes). | — | — |
| `open` † — `BufferPool` | 3 ↔ 4 | Abre um arquivo existente e confere o cabeçalho. Falha se não existir (161). Só os testes chamam, porque a bancada sempre cria. | `const string& name`, `FileKind` | `FileId` |
| `header_blocks` † — `BufferPool` | 3 ↔ 4 | Nº de blocos de cabeçalho do arquivo. Ninguém chama fora da camada 4. | `FileId` | `uint32` |
| `file_blocks` † — `BufferPool` | 3 ↔ 4 | Nº de blocos do arquivo, contando o cabeçalho. Ninguém chama fora da camada 4. | `FileId` | `uint32` |
| `file_kind` † — `BufferPool` | 3 ↔ 4 | Tipo do arquivo. Ninguém chama fora da camada 4. | `FileId` | `FileKind` |
| `config` † — `BufferPool` | 3 ↔ 4 | Configuração do pool: áreas, política, prefetch e toss-immediate. Ninguém chama fora da camada 4. | — | `const Config&` |

### PageGuard

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `read` — `PageGuard` | 3 ↔ 4 | Dá acesso de leitura aos bytes da página fixada. | — | `span<const byte>` (vale enquanto o guard existir) |
| `write` — `PageGuard` | 3 ↔ 4 | Dá acesso de escrita e marca a página como suja, exceto a anônima. | — | `span<byte>` |
| `id` — `PageGuard` | 3 ↔ 4 | Bloco fixado. | — | `BlockId` |
| `reset` — `PageGuard` | 3 ↔ 4 | Solta a página antes do fim do escopo. Com zero pins, ela volta a ser candidata da política. | — | — (`noexcept`) |
| `~PageGuard` — `PageGuard` | 3 ↔ 4 | O mesmo que `reset`, por RAII. | — | — |
| `anonymous` † — `PageGuard` | 3 ↔ 4 | Diz se a moldura é de rascunho. Ninguém chama fora da camada 4. | — | `bool` |
| `valid` † — `PageGuard` | 3 ↔ 4 | Diz se o guard ainda segura uma página. Ninguém chama fora da camada 4. | — | `bool` |

### Reservation

| Nome e classe | Interface | O que faz | Input | Output |
|---|---|---|---|---|
| `id` — `Reservation` | 3 ↔ 4 | Número da reserva que a camada 3 passa em `fetch`, `allocate` e `scratch`. | — | `ReservationId` |
| `~Reservation` — `Reservation` | 3 ↔ 4 (e 2, que é a dona) | Devolve as molduras do subpool à área de Trabalho. No Monitor, isso acontece no fim da medida. | — | — (`noexcept`) |
| `capacity` † — `Reservation` | 3 ↔ 4 | Nº de molduras da reserva. Só os testes chamam. | — | `size_t` |
| `pinned` † — `Reservation` | 3 ↔ 4 | Molduras fixadas agora. Só os testes chamam. | — | `size_t` |
| `peak_pinned` † — `Reservation` | 3 ↔ 4 | Maior nº de molduras fixadas ao mesmo tempo. Só os testes chamam. | — | `size_t` |

---

## Tipos que aparecem nas colunas Input e Output

Todos ficam em `common/common.hpp`, exceto os marcados.

| Tipo | Conteúdo |
|---|---|
| `AccessPath` | Tabela, estrutura, tipo de caminho (Scan, PrimaryBTree, PrimaryHash, SecondaryHash, SecondaryBTreeClustering ou SecondaryBTreeNonClustering), campo, UNIQUE, suporte a ponto e a faixa, campo de ordem e particionamento por hash |
| `Predicate` | `Point` (campo e chave) ou `KeyRange` (campo, `lo` e `hi` opcionais, inclusivos ou não) |
| `Intent` | `reuse` (falso: com toss-immediate, a moldura vira a próxima vítima) e `sequential` (verdadeiro: com prefetch, lê adiante no mesmo extent) |
| `JoinInput` (camada 2) | Caminho de acesso, campo da junção e operando (0 ou 1) |
| `ResultShape` (camada 2) | Campos de cada operando que entram no checksum e arquivo de dump opcional |
| `JoinParams` / `SelectParams` (camada 2) | Ajustes opcionais do plano. Junção: chunk, partições e páginas de grupo. Seleção: m, linhas esperadas e ordenação de RIDs |
| `JoinPlan` / `SelectPlan` (camada 2) | O plano calculado sem E/S, com o uso de molduras em `frames_used()` |
| `Outcome` | `OpResult` ou `NotApplicable` |
| `OpResult` | Linhas, checksum, delta dos contadores, gravações do flush final, estimativa (transferências e seeks), fases e tempo |
| `NotApplicable` | Motivo e tipo: `NaoAplicavel` (célula sem sentido, 170) ou `Inviavel` (não cabe em M) |
| `Counters` | Contadores de E/S global, por tipo de arquivo e por arquivo |
| `TableStats` / `Occupancy` | Estatísticas do catálogo (n, r, bfr, b, estruturas) e ocupação medida de uma estrutura |
| `PrimarySpec` / `IndexSpec` | Pedido de criação: tipo, chave, fill, buckets e função hash; ou tipo, campo e clustering |
| `RID` / `BlockId` | (bloco, slot) e (arquivo, bloco) |
| `Key` / `Value` | Chave: int32, int64 ou string. Valor para `encode`: NULL, int64 ou string |
| `PinnedRecord` (camada 3) | `struct` com `guard` (página fixada) e `view` (registro); sem métodos |

---

## Métodos públicos sem chamador na camada de cima (†)

- **Só os testes chamam:**
  - `Monitor`: `plan_join` (o próprio `join` também chama), `erase` e `reorganize`;
  - `StorageManager`: `block_size`;
  - `RecordView`: `get<T>` e `get_var` (a camada 3 também usa);
  - `BufferPool`: `open`;
  - `Reservation`: `capacity`, `pinned` e `peak_pinned`.
- **Só a própria camada usa:**
  - `Monitor::plan_select`;
  - `BlockCursor::last_rid`;
  - `RecordView`: `bytes` e `empty` (`empty` também nos testes);
  - `StorageManager`: `tables` e `table_name`;
  - `ScratchSpace::capacity`.
- **Ninguém chama:**
  - `BlockCursor::block`;
  - `TempFile`: `sealed` e `records`;
  - `ScratchSpace::pages`.
- **Ninguém chama fora da camada 4:**
  - `BufferPool`: `header_blocks`, `file_blocks`, `file_kind` e `config`;
  - `PageGuard`: `anonymous` e `valid`.

Nenhum desses métodos muda o que é medido. Eles podem ficar para diagnóstico e testes, ou sair da interface pública sem afetar a bancada.

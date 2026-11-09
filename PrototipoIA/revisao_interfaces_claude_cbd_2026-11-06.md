## 1. Correções bloqueantes para integrar os módulos

### P0-1 — `PrimarySpec` não distingue organizações UNIQUE e NOT UNIQUE

**Onde:** `PrimarySpec` e `StructureKind`, linhas **608–609 e 633–636**; `create_table`, linhas **125–127**.

**Problema:** há `Heap`, `BTree` e `Hash`, mas o contrato não fornece um sinal explícito para construir **B+ primária UNIQUE/NOT UNIQUE** e **Hash primário UNIQUE/NOT UNIQUE**. Assim, o `create_table` não consegue especificar inequivocamente as cinco organizações exigidas.

**Correção mínima:** incluir `bool unique` ou, preferivelmente, um enum de restrição de chave em `PrimarySpec`; para Heap, definir se o campo é ignorado. Validar as combinações permitidas de primárias/secundárias ao criar o catálogo. `AccessPath.unique` deve refletir a restrição REAL, não somente uma etiqueta.

**Teste de aceitação:** construir B+ e Hash nas duas modalidades; inserir duplicatas e verificar rejeição apenas no caso UNIQUE.

**Responsáveis:** I3, I4 e I6.

### P0-2 — Quota `M` não inclui todas as páginas usadas pelo operador

**Onde:** `reserve(Area,n)` (linhas **155–156, 328–343, 387–406**), `probe`/`RidCursor` (linhas **135, 237–255**), `fetch` com `ReservationId=0` (linha **326**).

**Problema:** a proposta reserva frames em uma única área e admite explicitamente que o `RidCursor` mantém a folha de índice **fora da reserva**. Um operador pode declarar `M=4` e manter quatro páginas de dados/trabalho MAIS páginas de índice, deixando incerto o limite efetivo. O problema também vale para buffers da ordenação e estruturas de hash mantidas em RAM fora do pool.

**Correção recomendada SEM abandonar as quatro áreas:** distinguir **partição física de frames** (Data/Index/Catalog/Work) de **quota lógica do operador**, que soma todos os frames distintos fixados ou alocados em seu nome, independentemente da área. Uma interface possível é `reserve_operator(M)` retornando token usado por `fetch`, `allocate`, `scan_blocks`, `find`, `probe` e `create_temp`; páginas de índice também debitam quota. Se preferirem `reserve(Area,n)`, somar quotas por área com um limite total M explícito e verificável. Impedir bypass acidental via `ReservationId=0` no percurso medido. Definir como memória das tabelas hash/merges fora das páginas é debitada (p. ex., buffers contidos em molduras reservadas). Uma página fixada duas vezes pelo MESMO operador não precisa consumir dois frames na quota, mas são dois pins.

**Teste de aceitação:** executar seleção indexada e join com `M` baixo; inspecionar pico de frames distintos e buffers auxiliares e demonstrar `uso <= M` (ou documentar explicitamente overhead externo se exigido pelo professor). Confirmar que uma área fisicamente cheia produz falha controlada, não crescimento silencioso.

**Responsáveis:** I2 e I5, com I6 definindo/reportando M e tamanhos das áreas.

### P0-3 — Os formatos de registro fixo/variável e NULL não estão cobertos pela API

**Onde:** `RecordView::get<T>` (linhas **257–280**), `FieldDef/Schema` (linhas **623–629**).

**Problema:** `RecordView` usa `field.offset` fixo e `memcpy` direto. Isso não descreve acesso a campos de tamanho variável, NULL bitmap, limites do registro ou metadados do diretório de slots. `assert()` não garante checagem em release. A escolha de dois layouts é válida; **o contrato de decodificação ainda falta**.

**Correção mínima:** definir `RecordCodec` com encode/decode/field access para os dois layouts e `is_null`; o esquema deve indicar campo variável/nullable e o tipo de página/registro. Manter `RecordView` sem cópia, mas encaminhar `get` ao codec e validar tipo, offsets e comprimento **em runtime**. Para páginas de registros fixos, estabelecer slots estáveis/tombstones; para variáveis, `SlottedPage` com compactação que não renumera `SlotId`.

**Teste de aceitação:** registro fixo; variável com comprimentos distintos; NULL e não-NULL; inserção/remoção/compactação; leitura por RID antes e depois de compactar a mesma página; acesso malformado rejeitado sem leitura fora dos limites.

**Responsáveis:** I1 pelo codec/layout, I3/I4 pelos formatos de suas páginas, I6 pelo esquema.

### P0-4 — Falta liberar UMA página através do Buffer Pool

**Onde:** `BufferPool` linhas **325–332**; `FileManager::free_block` linhas **415–420**.

**Problema:** o File Manager possui `free_block`, mas o Buffer Pool só expõe `remove_file()` para remover um arquivo inteiro. B+ tree, Hash e estruturas auxiliares podem precisar liberar uma página individual sem violar o caminho de acesso da camada 3 à 4.

**Correção mínima:** `BufferPool::release(BlockId)` delega a liberação ao FM após verificar que a página não está pinned, retirar sua entrada do cache/política, e estabelecer a regra para páginas dirty liberadas. Definir atualização de bitmap e contadores.

**Teste de aceitação:** liberar uma página não pinned, reutilizá-la sem dados antigos; tentar liberar pinned e obter erro; verificar bitmap após reabrir o arquivo.

**Responsáveis:** I1 e I2, consumido por I3/I4.

### P0-5 — Não existe contrato de manutenção dos RIDs após movimentação entre páginas

**Onde:** `StorageManager::insert`, `erase`, `reorganize` (linhas **147–151**), modelos de B+ e índices (`tables_`/`indexes_`, linhas **161–164**), `RID` (linhas **487–497**).

**Problema:** o RID compacto do Claude é válido como **endereço físico relativo à tabela**, mas não permanece estável quando o registro muda de página durante split/rebuild/redistribuição. Índices secundários existentes podem continuar apontando para RIDs antigos.

**Correção mínima:** o `StorageManager` coordena manutenção dos índices. A primária deve reportar movimentações `(RID_antigo, RID_novo)` — por callback/eventos ou resultado estruturado — para atualizar os índices, ou reconstruir os índices afetados após uma reorganização completa. No movimento dentro da MESMA página, o slot deve permanecer estável. Documentar a política para páginas removidas e reaproveitadas.

**Teste de aceitação:** criar primária + secundário, provocar split e reorganização, consultar por secundário antes/depois e comparar resultados com referência.

**Responsáveis:** I3 e I4, com esquema/descritores de I6.

### P0-6 — Falta especificar o intervalo exato de medição

**Onde:** `Monitor.join/select` (linhas **62–81**) e `OpResult.delta`/`flush_writes` (linhas **573–592**); os métodos em si são apenas declarações. **Na descrição anterior**, o flush ocorria antes do segundo snapshot e, ao mesmo tempo, era declarado fora do `delta`.

**Problema:** o arquivo atual não prova que a contradição anterior foi resolvida. Sem uma sequência normativa, I5 e I6 podem medir intervalos diferentes, duplicar flush ou excluir indevidamente I/O de arquivos temporários.

**Correção mínima:** **preparação e estado cold/warm → snapshot S0 → execução completa do operador (incluindo runs/partições temporárias) → snapshot S1 → flush/limpeza final → snapshot S2**. Reportar `delta=S1-S0` e `flush_delta=S2-S1`, sem sobreposição. Estimativa teórica e leitura de estatísticas devem ocorrer fora de S0–S1 caso provoquem I/O. A destruição de temporários precisa ser posicionada explicitamente: I/O necessário para o algoritmo integra S0–S1; descarte administrativo final é separado.

**Teste de aceitação:** induzir deliberadamente N gravações de flush depois de S1 e confirmar que elas só aparecem na coluna de flush, não no delta principal; comparar total agregado com snapshots.

**Responsáveis:** I5 e I6, conferido por I1/I2.

## 3. Correções adicionais importantes, mas não bloqueiam o esqueleto inicial

### P1-1 — Ciclo de vida e I/O de `TempFile`

**Onde:** linhas **289–309**, `remove_file` linhas **325–332**. Um `TempFile` mantém `tail_` pinned e seu destrutor descarta páginas, inclusive sujas, sem gravar.

**Ajuste:** exigir encerramento do append/soltura de `tail_` antes de varrer a mesma saída; proibir destruição com cursores ainda vivos; distinguir escritas necessárias para produzir runs/partições (medidas) de descarte final (limpeza). Semântica de arquivo temporário puramente em memória não deve ser confundida com externalização sem I/O. **Não é obrigatório gravar uma página dirty que nunca mais será lida**, mas os custos efetivamente exigidos pelo algoritmo precisam ser contabilizados.

**Responsáveis:** I3/I5/I2.

### P1-2 — `open()` e `create()` têm semânticas diferentes

**Onde:** `FileManager::open`, `open_rw` com `O_CREAT` (linhas **412–420 e 467–470**).

**Ajuste:** separar criação e abertura, ou exigir modo explícito (`CreateNew`, `OpenExisting`, etc.). Evita que um erro de caminho silenciosamente crie um arquivo vazio, produzindo dados de experimento incorretos.

**Responsável:** I1.

### P1-3 — RID compacto é válido, mas a codificação persistente deve ser definida

**Onde:** linhas **487–497** indicam cópia byte a byte do `struct RID`.

**Ajuste:** manter 8 bytes e o RID relativo à tabela, porém definir serialização explícita de `block`, `slot` e campos reservados (ordem dos bytes, valores inválidos e versão do formato), sem depender do padding/layout C++ como contrato persistente. O modelo pode continuar 100% compatível com a proposta do Claude.

**Responsáveis:** I1, I3 e I4.

### P1-4 — `ResultSink` e validação lógica

**Onde:** linhas **85–94**.

**Ajuste:** `rows + soma de FNV-1a` é um bom filtro rápido, mas não prova igualdade entre conjuntos/multiconjuntos de resultados. Definir serialização canônica de chaves e tuplas, tratamento de duplicatas, comparação com SQLite/PostgreSQL e, em pelo menos testes de referência representativos, comparar efetivamente o multiconjunto de resultados. A ordem da junção não deve alterar o veredito.

**Responsáveis:** I5/I6.

### P1-5 — Metadados/cache e `invalidate_all()`

**Onde:** `StorageManager` guarda `Catalog`, e `BufferPool::invalidate_all()` existe (linhas **155–164 e 325–332**). Na descrição anterior, o catálogo era descrito como permanentemente pinned.

**Ajuste:** decidir se catálogo é residente, pinned ou recarregado. Um catálogo permanentemente pinned conflita com esvaziamento completo do pool; `invalidate_all()` deve rejeitar pins ativos. Uma possibilidade é manter descritores em RAM após carga, sem manter `PageGuard` ativo. **É uma pendência de semântica, não prova de bug na versão atual.**

**Responsáveis:** I2 e I6, com I3.

### P1-6 — Propriedades de acesso não garantem automaticamente otimizações de join

**Onde:** `AccessPath.ordered_by/hash_partitioned_by` (linhas **552–572**).

**Ajuste:** só eliminar sort/particionamento quando o percurso do caminho de acesso realmente entregar os registros na ordenação/partição prometida e a função hash for compatível. Registrar a condição e testar fallback em caso incompatível; organização Hash primária não é, por si só, prova de partições reutilizáveis para o Partition-Hash Join.

**Responsáveis:** I3/I4/I5.

### P1-7 — Validade e movimentação dos handles

**Onde:** `BlockScan/SearchCursor/RidCursor` guardam ponteiros para objetos donos de dados (linhas **179–255**); `RecordView` não possui os bytes (linhas **257–288**).

**Ajuste:** documentar que cursores/guards não podem sobreviver a `StorageManager`/`BufferPool` e que `RecordView` não pode sobreviver ao guard, nem ser reutilizado após avanço que invalide a página. `Reservation` deve permanecer válida até a destruição de todos os cursores que portam seu ID. Testes com ASan/UBSan em limites de vida útil são especialmente úteis.

**Responsáveis:** I2/I3/I4/I5.


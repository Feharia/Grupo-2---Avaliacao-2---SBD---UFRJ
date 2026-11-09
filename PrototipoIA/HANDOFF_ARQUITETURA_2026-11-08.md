# Arquitetura e decisões da bancada de teste — documento de transferência

Estado em 08/11/2026. Reúne tudo o que foi decidido nas sessões de 06/11/2026 sobre a Avaliação 2 de Construção de Banco de Dados (UFRJ, 2026.2) e foi escrito para ser lido sem nenhum contexto prévio. As decisões 157 a 169 foram acrescentadas em 08/11/2026, a partir da revisão de um colega (revisao_interfaces_claude_cbd_2026-11-06.md) conferida contra este documento.

Este documento é a referência única. Ele substitui ARQUITETURA_V2.md e ARQUITETURA_V3.md; as seções 7 a 10 reproduzem o conteúdo vigente da v3. O CONCLUSOES_SESSAO_2026-11-06.md continua válido como análise de fundo, e o essencial dele está na seção 11.

---

## 0. Como usar este documento

- Seções 1 a 3: contexto do trabalho, arquivos existentes e princípios da arquitetura.
- Seção 4: decisões de parâmetros (1 a 45), com os valores atuais.
- Seção 5: decisões de interface (100 a 171), com o texto final.
- Seções 6 a 10: arquitetura completa — camadas e interfaces, orçamento de molduras, entidades com declarações e métodos, limites a declarar no relatório e testes.
- Seções 11 a 14: acoplamentos que não podem ser esquecidos, questões abertas, pendências e o que não rediscutir.

Regras para quem continuar:
- As seções 4 e 5 são a proposta vigente. Qualquer mudança deve citar o número da decisão e o motivo.
- Nenhum número de capacidade (bfr, fan-out, nº de buckets) deve ser escrito à mão. Todos saem do layout pelo código e depois são publicados no relatório.
- Nenhum trecho de código foi compilado. As declarações são o contrato entre as camadas, não a implementação.

---

## 1. Contexto

**O trabalho.** Construir, com auxílio de IA, uma bancada de teste que mede o desempenho do processamento de operações relacionais sob diferentes configurações do Gerente de Armazenamento. A pergunta a responder é qual configuração do SGBD é mais adequada para processar eficientemente uma dada carga de trabalho sobre uma dada base de dados. A métrica central é o número de blocos transferidos entre a memória secundária e o buffer pool; o tempo de relógio pode ser registrado, mas não substitui a contagem.

**Prazo.** O PDF do enunciado diz 22/11/2026; o grupo informa 29/11/2026. Não foi confirmada nenhuma versão do enunciado posterior à de 02/11.

**Grupo e ferramentas.** Seis integrantes. C++20, GCC 12 ou superior (Clang como validação), CMake, GoogleTest, ASan, UBSan, LSan e clang-tidy. A E/S usa pread e pwrite (POSIX), então o build é feito no Linux ou no WSL2 (decisão 156).

**O que o enunciado exige, em resumo**
- Quatro camadas, cada uma usando só a interface da camada de baixo:
  1. Bancada de Teste: carrega ou gera a base, instancia cada configuração, executa as operações, coleta os indicadores e grava os resultados em CSV.
  2. Mini Monitor de Execução: uma primitiva por algoritmo, com o caminho de acesso escolhido explicitamente pelo experimento. Não há SQL, otimizador nem plano de execução.
  3. Gerente de Armazenamento: organizações primárias, métodos de acesso auxiliares e um mini catálogo.
  4. Gerente de Arquivos e Gerente de Buffer Pool: transferência de blocos e acesso às páginas em memória. Nenhuma camada acima do buffer acessa blocos por fora dele.
- Cinco organizações primárias: heap; B+ por campo UNIQUE e por campo NOT UNIQUE; hash estático por campo UNIQUE e por campo NOT UNIQUE.
- Métodos de acesso (§4.2):
  - heap: direto, sem índice; índice secundário hash para campo UNIQUE; índice secundário B+ sem repetição de chave nas folhas para campo NOT UNIQUE;
  - B+: direto pelo índice primário; índice secundário clustering, só para NOT UNIQUE, em B+;
  - hash: direto por UNIQUE; direto por NOT UNIQUE; índice secundário clustering para NOT UNIQUE, em B+.
- Junções (§5.1): três tipos (natural mestre-detalhe por PK e FK; equijoin em campos UNIQUE que não ditam a organização; equijoin em campos NOT UNIQUE que não ditam a organização), três algoritmos (nested-loop em blocos, sort-merge, partition-hash), cinco configurações de índice (a a e) e três combinações de tamanho (menor com menor, menor com maior, maior com maior; a menor é a que cabe na área reservada). A seletividade da junção não varia, e a escolha entre externa e interna deve ser considerada.
- Seleções (§5.2): igualdade em UNIQUE; faixa em UNIQUE; faixa em NOT UNIQUE. Três configurações (sem índice, campo da organização primária, índice secundário) e duas seletividades (0,1% e 50%).
- Metodologia (§6): variar um fator por vez em torno de uma configuração-base declarada, repetindo cada medida até a variação ficar abaixo de 5%, com no mínimo três repetições. Indicadores: ocupação em blocos antes e depois da manutenção; acessos a blocos por operação (leituras, gravações, requisições, acertos, taxa, seeks se modelados); custo estimado pelas fórmulas dos livros ao lado do medido.
- Fatores (§6.2): política de substituição (duas entre LRU, MRU, FIFO e Clock), tamanho das tabelas, seletividade.
- Base (§6.4): esquema de universidade com Aluno, Curso, Disciplina, Turma, Inscrição_Aluno_Turma e Detalhes_do_Aluno; volumes e atributos a critério do grupo.
- Conferir cada primitiva contra uma implementação de referência (SQLite ou PostgreSQL).
- Uso de IA e entrega (§7): procurar ativamente os defeitos listados em §7.1; entregar repositório, relatório técnico (todas as opções, tabelas e gráficos de medido × estimado, conclusões e recomendações por carga de trabalho), relatório de IA (log de prompts e código bruto, code review, testes de estresse, análise crítica e POST_MORTEM.md na raiz, que é o principal critério de avaliação da autoria), folha de rosto, apresentação com slides e postagem no AVA.

---

## 2. Arquivos na pasta do trabalho

- **Avaliacao_2_Indexacao_e_Processamento_de_Consultas.pdf** — o enunciado.
- **CONCLUSOES_SESSAO_2026-11-06.md** — análise de fundo: erros aritméticos, acoplamentos entre decisões, contratos entre camadas. Resumida na seção 11.
- **HANDOFF_ARQUITETURA_2026-11-08.md** — este documento.
- **revisao_interfaces_claude_cbd_2026-11-06.md** — revisão de um colega, feita sobre uma versão anterior das interfaces (provavelmente a V2). O que ainda era válido virou as decisões 157 a 169; o resto já estava resolvido aqui.
- **ARQUITETURA_V3.md** e **ARQUITETURA_V2.md** — versões anteriores deste conteúdo, mantidas como histórico.
- **diagrama_camadas_v5.png** e **.svg** — diagrama de entidades por camada vigente, com as decisões até a 171 e as emendas de 08/11. Versões anteriores: diagrama_camadas.png (v1), diagrama_camadas_v2.png, diagrama_camadas_v3.png e diagrama_camadas_v4.png (decisões até a 169).
- **PrototipoIA/** — protótipo gerado por IA em 08/11 a partir deste documento (decisões até a 170): código bruto ainda não revisado, com 37 testes e conferência com o SQLite. Os desvios em relação a este documento estão no README dele, e o log de prompts está em PROMPTS.md.
- **diagrama_camada4_posix.png** e **.svg** — a camada 4 antes e depois da troca de std::fstream por pread/pwrite.

---

## 3. Princípios da arquitetura

- Camadas estritas. A configuração desce por injeção na construção; a medida sobe no OpResult; as chamadas que precisam atravessar uma camada são repasses sem lógica.
- Uma fonte para cada número: o tamanho do bloco vem do Config; as capacidades vêm do layout; M é o tamanho da reserva; os parâmetros de cada operação são resolvidos uma única vez num plano.
- A memória de um operador é um subpool exclusivo de M molduras, com política de substituição própria.
- Nenhuma estrutura da linguagem sem limite guarda registros: a memória de trabalho fica em molduras (ScratchSpace).
- A contagem física acontece num único lugar, o FileManager, com classificação sequencial ou aleatória por uma cabeça de disco global.
- Todo recurso tem um único dono: handles RAII que só podem ser movidos, e destrutores que nunca lançam exceção.
- O resultado é conferível: o checksum é canônico e reproduzido pelo script de referência sobre o SQLite.
- Uma combinação impossível é NotApplicable com o motivo, nunca uma troca silenciosa de caminho.

---

## 4. Decisões de parâmetros (1 a 45)

Proposta vigente para a tabela do grupo, cuja coluna "valor decidido" ainda precisa ser preenchida. A camada responsável está entre colchetes. As capacidades assumem bloco de 4.096 B e cabeçalho de bloco de 24 B e precisam ser recalculadas pelo código quando o layout existir.

### Gerente de Arquivos (§2.1)
- **1. Tamanho do bloco** [4] — 4.096 B. Fonte única: Config.block_size.
- **2. Técnica de alocação** [4] — Cada arquivo lógico é um arquivo do SO. O arquivo cresce de extent em extent (8 blocos) com ftruncate. Não há mapa lógico → físico: o número do bloco é a posição no arquivo (decisão 101).
- **3. Cabeçalho do arquivo** [4] — Bloco 0: magic, versão, tamanho do bloco, tamanho do extent, nº de blocos, nº de registros, bitmap de blocos livres e referência ao catálogo.
- **4. Gerência de blocos livres** [4] — Bitmap no bloco 0, mantido em memória pelo FileManager e gravado no flush e no fechamento. Os blocos de dados começam no 1 (decisão 154).
- **5. Contadores** [4] — Por arquivo, por tipo de arquivo e globais, separando leitura e gravação sequenciais de aleatórias. Há seek quando muda o arquivo ou quando o bloco ≠ anterior + 1, com "anterior" global (decisão 102).

### Gerente de Buffer Pool (§2.2)
- **6. Nº total de páginas** [4] — 84 molduras (336 KiB), que é a soma das áreas.
- **7. Blocos por página** [4] — 1.
- **8. Repartição do pool** [4] — Por tipo de página para os acessos sem reserva, e um subpool exclusivo por operador para os acessos com reserva (decisão 136).
- **9. Áreas e tamanhos** [4] — Dados 32, índices 16, catálogo 4 (fixado), trabalho 32. Os subpools saem da área de trabalho, então 32 é o maior M possível. A área de dados só é usada fora das operações medidas.
- **10. Áreas fixas ou ajustáveis** [4] — Fixas. Os subpools são criados e devolvidos a cada operação.
- **11. Políticas comparadas** [4] — LRU × MRU, com uma instância da política por região (área ou subpool).
- **12. Toss-immediate** [4, emitido pela 2] — Desligado no baseline. Quando ligado, uma página pedida com Intent.reuse = false vira a próxima vítima ao ser solta.
- **13. Fixação de blocos** [4, dirigida pela 2] — Pin e unpin por PageGuard (RAII); a camada 2 controla quantas páginas ficam fixadas por meio dos cursores e da reserva.
- **14. Gravação forçada** [4] — Não há gravação forçada de dados: páginas sujas são gravadas na expulsão. O flush_all depois da carga faz parte da carga e não é medido. Antes de cada repetição e no fim de cada operação, o flush_all é uma verificação e deve gravar zero.
- **15. Substituição com prioridade** [4] — Não há. A retenção preferencial de índice e catálogo é feita pelas áreas próprias, dentro das quais a política continua atuando. O campo priority_retention foi retirado do Config.
- **16. Prefetch** [4, acionado pelo Intent] — Desligado no baseline. Quando ligado, com Intent.sequential, lê até extent_blocks − 1 blocos seguintes, expulsando só páginas não fixadas; as leituras contam como sequenciais (decisão 150).
- **17. Métricas do buffer** [4] — Requisições, acertos, faltas, taxa de acerto (derivada), alocações, leituras e gravações sequenciais e aleatórias, gravações de páginas sujas na expulsão e gravações forçadas.

### Formato dos registros e layout dos blocos (§3.1)
- **18. Fator de blocagem** [3] — Derivado: ⌊(bloco − cabeçalho − diretório de slots) / tamanho do registro⌋. Com a entrada de slot de 4 B adotada no protótipo: Aluno (R = 127, com 1 B de null bitmap) tem bfr 31, e Inscrição (R = 64), bfr 59. O valor real sai do código e é publicado no relatório (emendada em 08/11).
- **19. Campos fixos × variáveis** [3] — Tabelas de medição 100% de tamanho fixo; campos variáveis só em Detalhes_do_Aluno. Formato do registro na decisão 158.
- **20. Campos comuns** [3] — Prefixo fixo, na ordem do esquema.
- **21. Campos opcionais** [3] — Null bitmap, sem reservar espaço para o valor ausente.
- **22. Campos multivalorados** [3] — Tabela externa (e-mails, telefones), não inline.
- **23. Campos estruturados** [3] — Achatados em colunas (endereço → logradouro, nº, CEP).
- **24. Campos derivados** [3] — Não materializados nas tabelas de medição. Curso.carga_horaria_total é o único, recalculado no reorganize.
- **25. Null bitmap** [3] — ⌈n_anuláveis / 8⌉ bytes no cabeçalho do registro.
- **26. Organização dos registros no bloco** [3] — Uma só implementação de SlottedPage para todo o sistema. Nas tabelas de medição os registros são de tamanho fixo, o que mantém o bfr determinístico.
- **27. Registros removidos** [3] — Marcados no slot. Slots nunca são renumerados, o que mantém o RID estável.
- **28. Slotted page** [3] — Sim, em todas as tabelas, com a mesma implementação.
- **29. Blocagem** [3] — Não estendida nas tabelas de medição; estendida só para registro maior que o bloco. Ainda falta confirmar com o professor se a estendida é obrigatória.

### Heap (§3.2)
- **30. Gerência dos removidos** [3] — Mapa de espaço livre, sem lista de livres; inserção sempre no fim, o que preserva a ordem física. O erase do heap nunca libera blocos (162).
- **31. Compactação** [3] — Quando o espaço livre passa de 30% do arquivo, durante a carga de manutenção (reorganize): o heap é reescrito sem blocos vazios e os índices são reconstruídos (141, 162). Fora do reorganize, a compactação é só dentro da página, sem renumerar slots.

### Árvore B+ (§3.4)
- **32. Chave de ordenação** [3] — Campo único (DRE, CPF ou curso).
- **33. Nó interno e fan-out** [3] — Derivado do layout. Com chave de 4 B, ponteiro de 4 B (o número do bloco, decisão 101) e cabeçalho de 24 B: 509 chaves e 510 ponteiros, ou seja, fan-out de 510 (emendada em 08/11).
- **34. Nó folha** [3] — B+ primária: as folhas guardam os registros (capacidade = bfr), são encadeadas e ficam no arquivo de dados; os nós internos ficam num arquivo do tipo Index (decisão 145). B+ secundária: folhas com (chave, posting list de RIDs), encadeadas, sem repetição de chave.
- **35. Ocupação para divisão** [3] — Divide a 100%.
- **36. Redistribuição** [3] — Redistribui antes de fundir, se o irmão tiver mais que ⌈n/2⌉ entradas.
- **37. Ocupação mínima para fusão** [3] — ⌈n/2⌉.
- **38. Carga** [3] — Bulk load com 70% de ocupação no baseline; 100% como ponto de comparação; inserção registro a registro na manutenção.

### Hash externo (§3.5)
- **39. Tamanho da tabela** [3] — Derivado: ⌈N / (α × capacidade do bucket)⌉, levado ao primo seguinte se a função usar módulo primo. Aluno por DRE (N = 20.000, α = 0,8, capacidade 31): 807, ou 809 se primo. Para campo NOT UNIQUE com d valores distintos, o primo ≥ min(d, ⌈N / (α × capacidade)⌉) (decisão 163).
- **40. Estrutura do bucket** [3] — Um bloco: cabeçalho, registros e ponteiro de overflow.
- **41. Tamanho do bucket** [3] — Um bloco: 31 registros de Aluno (R = 127, slot de 4 B) (emendada em 08/11).
- **42. Função de hash** [3] — Inteiros: multiplicativa de Knuth; textos: FNV-1a. Nunca std::hash, que dá valores diferentes em libstdc++ e libc++. Particionamento e tabela de hash em memória usam sementes diferentes (decisão 153).
- **43. Colisão** [3] — Encadeamento de overflow por bucket.
- **44. Desbalanceamento** [3, depende da 1] — Cardinalidade do campo ≥ N / (capacidade do bucket × cadeia máxima aceitável), verificada no load_spec. Hash em sexo é proibido; curso (200 valores) é adequado.
- **45. Fator de carga** [3] — α real registrado antes e depois da manutenção, com o comprimento médio e máximo das cadeias.

---

## 5. Decisões de interface (100 a 171)

Texto final, já com todas as revisões. A camada ou fronteira está entre colchetes.

- **100. Canal de configuração e medida** [transversal] — A configuração desce por injeção: a camada 1 monta o Config e o entrega ao Monitor, que constrói o StorageManager, que constrói o BufferPool. A medida sobe no OpResult; a camada 1 nunca lê contadores nem o catálogo diretamente.
- **101. Endereço de bloco** [4→3] — BlockId{arquivo, bloco}, em que o bloco é a posição no arquivo do SO. O bloco 0 é o cabeçalho. Não há mapa de extents.
- **102. Modelo de seek** [4] — Cabeça global: há seek quando muda o arquivo ou quando o bloco ≠ anterior + 1. É o modelo das fórmulas dos livros (o BNL do Silberschatz conta 2·b_r seeks pela alternância entre arquivos). É um modelo, não uma medida do dispositivo.
- **103. Pedido de página** [4→3] — fetch(BlockId, Intent, reserva); o Intent tem dois eixos (decisão 137) e chega à camada 4 por todos os métodos de leitura. A reserva é sempre passada por escrito; o acesso sem reserva usa kNoReservation (159).
- **104. Página nova** [4→3] — allocate não lê o disco: a moldura é zerada, marcada suja e fixada. Soma em allocations (decisão 138).
- **105. Acesso aos bytes** [4→3] — PageGuard::read() devolve um span só de leitura; write() devolve um span mutável e marca a página como suja. O contrato 4.3 das conclusões passa a significar: a camada 3 recebe bytes só através do guard, e nenhum ponteiro sobrevive ao unpin. A exigência do enunciado (ninguém acessa blocos por fora do buffer) continua cumprida.
- **106. Área de cada página** [4] — Nos acessos sem reserva, a área vem do tipo do arquivo, registrado na abertura (Data, Index, Catalog, Temp). Nos acessos com reserva, a página vai para o subpool.
- **107. Reserva** [4→3] — reserve(n) cria um subpool exclusivo de n molduras (decisão 136). A cota conta molduras, não pins. Fixar além do subpool lança exceção.
- **108. M e tabela pequena** [4, 2] — M é o tamanho da reserva da operação; o máximo é a área de trabalho. Tabela pequena (menor) é a que tem b ≤ M − 2: cabe na reserva com 1 página de entrada e 1 de saída, e assim cada algoritmo a processa numa passada (BNL com o chunk do tamanho dela, hash sem particionar, sort-merge numa run). A bancada calcula o tamanho de cada junção pelas estatísticas e avisa se ele diverge do declarado (emendada em 08/11).
- **109. Leitura dos contadores** [4] — snapshot() devolve uma cópia imutável (global, por tipo, por arquivo); a medida é a diferença entre dois snapshots; não há reset.
- **110. Estado entre repetições** [4, 1] — Antes de cada repetição: flush_all (deve gravar zero) e invalidate_all, que esvazia tudo menos a área de catálogo. O flush depois da carga faz parte da carga.
- **111. RID** [3] — {bloco u32, slot u16, reservado u16}, 8 B, layout fixo (167). Estável enquanto o registro fica na mesma página; quando ele muda de página, o RID muda e os índices são atualizados (141, 169).
- **112. Granularidade da varredura** [3→2] — scan_blocks devolve um BlockScan, que entrega um BlockCursor por bloco; a camada 2 pode manter vários cursores vivos (é o chunk do BNL).
- **113. Registro entregue** [3→2] — RecordView: visão sem posse, válida enquanto o guard do bloco existir. key(campo) devolve o campo como Key tipado. A leitura é sempre verificada, inclusive em release, e cobre NULL e campos variáveis (158).
- **114. Tipos de chave** [3] — int32, int64 e texto de tamanho fixo. CR em int32 × 100, porque o tipo Key não tem alternativa int16.
- **115. Predicado** [3→2] — Só Point e KeyRange sobre o campo do caminho. find recusa o caminho Scan; a seleção sem índice é filtrada pela camada 2 (decisão 146).
- **116. Descritor do caminho** [3→2] — AccessPath com tabela, estrutura, tipo, campo, unique (copiado do FieldDef, decisão 157), supports_point e supports_range (o que find ou probe respondem), ordered_by (só na B+ primária e no índice clustering) e hash_partitioned_by (só no hash primário). Índice clustering só é aceito sobre B+ ou hash primários e no campo da organização.
- **117. Saída do índice secundário** [3→2] — RIDs entregues por um RidCursor, na ordem das posting lists, que é o baseline e corresponde à fórmula h + s. A variante que ordena os RIDs por bloco usa um ScratchSpace de RIDs e é declarada à parte.
- **118. Arquivos temporários** [3→2] — TempFile: append, seal (grava as páginas sujas, conta como temp e as retira do pool), leitura só depois do seal e com a própria reserva, apagado no destrutor.
- **119. Estatísticas** [3→2] — Sempre medidas na estrutura, uma vez, e guardadas no catálogo; stats() não faz E/S (160). b é o número de blocos do arquivo de dados: no heap, todos; na B+, as folhas; no hash, buckets mais overflow. Altura e nós internos da B+ ficam nas estatísticas da estrutura.
- **120. Carga e manutenção** [3→2→1] — bulk_load, insert, erase (delete é palavra reservada), reorganize e occupancy. Na B+ e no hash, blocos esvaziados são liberados por free_block; o heap não libera blocos fora do reorganize (162).
- **121. Assinatura da junção** [2→1] — join(algoritmo, externa, interna, m, ResultShape, JoinParams). O load_spec valida m ≤ área de trabalho.
- **122. Externa e interna** [1] — Regra declarada a partir das estatísticas: no BNL, a menor por fora; no hash join, a menor como externa, que é o build; no sort-merge, o lado UNIQUE por dentro, para não precisar guardar grupo de duplicatas (se os dois ou nenhum forem UNIQUE, a menor por fora) (emendada em 08/11). Uma variante com os papéis trocados responde ao §5.1.3.
- **123. Parâmetros derivados de M** [2] — Resolvidos uma única vez no plano, com o orçamento de molduras (decisões 143 e 148).
- **124. Algoritmo × caminho de acesso** [2] — O BNL não usa índices (as configurações a a e só diferem pelo b de cada organização). O sort-merge pula a ordenação de entrada com ordered_by. O hash join usa um arquivo hash como partições já prontas, por scan_buckets, e particiona a outra entrada com bucket_of. As condições para aproveitar ordem e partições estão na 164.
- **125. Seleções** [2→1] — select_points (lote de cerca de 1.000 chaves, gravado em arquivo antes da execução) e select_range, ambos devolvendo Outcome.
- **126. Destino do resultado** [2] — ResultSink: conta as linhas e calcula o checksum canônico; a gravação da saída não entra no custo, como nas fórmulas dos livros (decisões 142 e 152).
- **127. Conteúdo do OpResult** [2→1] — Linhas, checksum, delta (antes do flush de fim), flush_writes (verificação, deve ser zero), custo estimado, fases (PhaseRecorder) e tempo de relógio.
- **128. Célula inaplicável** [2→1] — NotApplicable com o motivo, nunca troca silenciosa de caminho. Casos: faixa em UNIQUE pelo índice secundário (só existe hash), faixa no hash primário, plano inviável em M e falta de espaço na execução (decisão 151). Desde a 170, distingue a célula que não existe (nao_aplicavel) da que não cabe em M (inviavel).
- **129. Especificação do experimento** [1] — Baseline, variações OFAT, tabelas, operações (com o SQL equivalente) e sementes, versionados e citados no relatório. No protótipo, a especificação é a função spec_padrao() da bancada, e o hash dela vai para o manifesto; o TOML fica opcional, porque não muda o resultado (emendada em 08/11).
- **130. Gerador e constantes** [1] — Um CSV por tabela, com semente; CR em int32 × 100 e textos sem preenchimento. As constantes de faixa e as linhas esperadas saem dos quantis dos CSVs e são gravadas em constantes.csv.
- **131. Carga de manutenção** [1] — Proporção de inserções e remoções, distribuição das chaves, volume e semente.
- **132. Repetições** [1] — A semente da amostra muda a cada repetição; o pool começa vazio; pelo menos 3 repetições, tolerância de 5%, no máximo 10. A tolerância vale para as leituras e gravações e para o tempo de relógio: fora dos lotes, as contagens são determinísticas, e o que varia é o tempo (emendada em 08/11).
- **133. Saída** [1] — CSV longo de resultados (uma linha por experimento, configuração, operação e repetição), CSV de ocupação por estrutura antes e depois da manutenção, e manifesto da execução (commit, compilador, flags, hash da especificação).
- **134. Repasses** [transversal] — A camada 3 repassa à 2 as chamadas de controle da 4 (reserve, snapshot, flush_all, invalidate_all); a camada 2 repassa à 1 as de definição, carga, manutenção e consulta. Repasses não têm lógica.
- **135. Tipos que atravessam camadas** [transversal] — Da camada 4 até a 2 chegam só o PageGuard (dentro de cursores e do PinnedRecord) e a Reservation, opaca (só id, capacidade e molduras fixadas). Valores sem comportamento ficam em common/.
- **136. Subpool** [4] — A reserva separa n molduras da área de trabalho, com uma instância própria da política. Toda página pedida pela reserva vive ali.
- **137. Intent** [common] — Duas flags independentes: reuse e sequential. Constantes: kRandomReuse (nós de índice), kRandomOnce (fetch por RID), kSeqReuse (interna do BNL), kSeqOnce (externa do BNL, runs, partições).
- **138. Contagem de allocate** [4] — allocate soma em allocations e não em requisições; requisições = acertos + faltas.
- **139. Selagem de temporários** [3] — seal() grava as páginas sujas do temporário, contadas como gravações comuns do tipo temp, e as retira do pool; a releitura é sempre física.
- **140. Memória de trabalho** [3] — ScratchSpace em molduras anônimas da reserva, que nunca vão ao disco nem são contadas. Guarda registros ou RIDs (estes como chave Int64 = bloco << 16 | slot), ordena, monta hash e responde sondagens. Os diretórios internos são limitados pela capacidade.
- **141. RID estável** [3] — Toda movimentação de registro (divisão, fusão, redistribuição, absorção de overflow) é avisada a um MoveListener, que atualiza os índices. O reorganize reconstrói os índices.
- **142. Checksum canônico** [2, 1] — Campos na ordem dos operandos da consulta, cada um precedido de um byte de marca (0 = NULL, 1 = valor; decisão 158); inteiros como int64 little-endian; textos em UTF-8 sem preenchimento, com prefixo de tamanho; FNV-1a de 64 bits por tupla; soma módulo 2⁶⁴.
- **143. Plano resolvido** [2] — JoinPlan e SelectPlan são calculados uma vez e usados igualmente pela execução e pela estimativa. O tamanho de runs e partições usa os blocos densos d = ⌈n/bfr⌉, porque elas são gravadas cheias. As estimativas usam b para ler a estrutura e d para gravar e reler runs e partições; com b em tudo, a B+ com fill 0,7 e o hash com α = 0,8 ficavam superestimados (emendada em 08/11).
- **144. Catálogo fixado** [3, 4] — O catálogo fixa suas páginas na abertura e só as solta no fim; invalidate_all não toca na área de catálogo.
- **145. B+ primária em dois arquivos** [3] — Nós internos num arquivo do tipo Index (área de índices, fora de M); folhas com os registros no arquivo de dados (pela reserva).
- **146. Seleção sem índice** [2] — O caminho Scan sempre executa, com scan_blocks e filtro na camada 2. Na seleção pontual em campo UNIQUE, a varredura de cada chave para no primeiro registro encontrado (b/2 em média, como nas fórmulas) (emendada em 08/11). NotApplicable só para caminho de índice ou hash que não responde ao predicado.
- **147. Conferência com o SQLite** [1] — Lotes e constantes gravados em arquivo antes da execução; tools/reference.py grava referencia.csv; tools/compare.py cruza com resultados.csv e termina com erro se algo divergir.
- **148. Orçamento de molduras** [2] — Todas as parcelas de memória estão no plano, e frames_used() ≤ m é verificado antes de executar (seção 7).
- **149. Ordem de medição** [2] — O snapshot final e o fechamento das fases vêm antes do flush de fim.
- **150. Migração e prefetch** [4] — Página pedida pela reserva que esteja em outra região é copiada para o subpool, e a moldura de origem fica vazia; se estiver fixada na origem, fetch lança exceção. O prefetch expulsa só páginas não fixadas.
- **151. Falta de espaço** [2] — Se um grupo de duplicatas ou uma partição não couber no ScratchSpace, o algoritmo lança DoesNotFit, e o Monitor devolve NotApplicable{"não cabe em M"}. Desde a 171, isso vale só para o grupo do sort-merge: a partição do hash join que não cabe é particionada de novo ou vai para o bailout.
- **152. Papéis e ordem canônica** [2] — No hash join, a externa é o build. Cada JoinInput diz se é o operando 0 (mestre) ou 1 da consulta; swapped é calculado só pelo plano e entregue ao ResultSink, o que funciona também em self-join.
- **153. Sementes de hash** [2, 3] — Particionamento e tabela de hash em memória usam sementes diferentes. Desde a 171, cada passe de particionamento usa uma semente própria.
- **154. Cabeçalho e bitmap** [4] — Bitmap no bloco 0; blocos de dados a partir do 1; cabeçalho gravado na criação, lido e conferido na abertura (161) e regravado no flush e no fechamento, com as E/S contadas no arquivo; temporários só crescem e não gravam cabeçalho; header_blocks expõe o tamanho do cabeçalho para a ocupação. Um bloco de cabeçalho cobre até 32.576 blocos, (4.096 − 24) × 8, cerca de 127 MiB por arquivo; acima disso, a alocação de blocos falha (emendada em 08/11).
- **155. Ciclo de vida** [transversal] — Destrutores, remove_file e release nunca lançam exceção; encontrar uma página fixada nesse momento é erro de programação e encerra o programa com diagnóstico. fetch, allocate e scratch com o id de uma reserva já liberada lançam logic_error.
- **156. E/S POSIX** [4] — Decisão do grupo em 06/11: o FileManager usa pread, pwrite e ftruncate, com descritores em UniqueFd e laços que tratam transferência curta e EINTR (retorno 0 é erro, decisão 161). Plataforma oficial: Linux, incluindo WSL2; quem usa Windows trabalha no WSL2, com o repositório no sistema de arquivos do Linux. O CMake recusa Windows nativo e verifica pread. O data_dir fica em ext4, fora do OneDrive.

### Decisões 157 a 169 (08/11/2026)

Acrescentadas depois de conferir a revisão de um colega (revisao_interfaces_claude_cbd_2026-11-06.md). Cada uma diz o que altera e por quê; os itens da revisão que já estavam resolvidos não geraram decisão.

- **157. Unicidade e anulabilidade são do campo** [common, 3, 2] — Altera 116. FieldDef ganha unique e nullable. AccessPath.unique é copiado de schema.field(campo).unique (falso no Scan). O JoinPlan lê a unicidade da chave da interna pelo esquema, por JoinInput.field, e não pelo caminho, o que cobre também o caminho Scan. Ela decide group_pages = 0 no sort-merge, se o find por igualdade para no primeiro registro e qual fórmula a estimativa usa. O load_spec só aceita hash secundário em campo unique (§4.2). O insert rejeita duplicata com DuplicateKey quando o campo é unique e alguma estrutura o indexa; campo unique sem estrutura não é verificado, e o gerador garante a unicidade. Motivo: o contrato não dizia de onde vinha AccessPath.unique (revisão, P0-1).
- **158. Formato do registro e leitura verificada** [3, 2] — Detalha 19 a 25 e altera 113 e 142. O registro é [null bitmap de ⌈n_anuláveis / 8⌉ bytes, em que o bit i é o i-ésimo campo anulável na ordem do esquema][campos fixos na ordem do esquema][um u16 por campo VarChar com o fim do campo, relativo ao início do registro][bytes dos VarChar]. FieldDef.offset dos campos fixos já desconta o bitmap; FieldType ganha VarChar, cujo size é o máximo. RecordView ganha is_null e get_var. get<T> confere sempre, também em release, que o campo é fixo, que sizeof(T) é o tamanho do campo e que ele cabe no registro; get_var confere que os offsets são crescentes e caem dentro do registro; key() de campo NULL lança. Chave NULL nunca casa numa junção, como no SQL. No checksum, cada campo vem precedido de um byte de marca (0 = NULL, 1 = valor). As verificações só custam CPU e não mudam a contagem de blocos. Motivo: o esquema não conseguia descrever NULL nem campo variável, e o assert só valia em debug e não pegava leitura com tipo de tamanho errado (revisão, P0-3; §7.1 do enunciado).
- **159. Reserva sempre explícita** [4→3] — Altera 103. fetch e allocate perdem o valor padrão do ReservationId; o acesso sem reserva passa kNoReservation por escrito. IoCounts ganha data_area_requests (pedidos sem reserva a arquivos Data), e o Monitor lança logic_error se o delta de uma operação medida não for zero. Os nós internos e os índices continuam fora de M (145). Motivo: com o valor padrão, um cursor que esquecesse a reserva cairia em silêncio na área de dados e M deixaria de valer sem erro (revisão, P0-2).
- **160. Estatísticas sem E/S** [3→2] — Altera 119. As estatísticas são medidas percorrendo a estrutura ao fim de bulk_load e de reorganize e ficam no catálogo, que é residente (144); stats() só lê o catálogo. insert e erase marcam as estatísticas como desatualizadas, e stats() desatualizada lança logic_error. Motivo: o plano é resolvido depois do invalidate_all e antes do snapshot inicial; se stats() lesse a estrutura, aqueceria o pool antes da medida, e essa E/S sumiria do delta (revisão, P0-6).
- **161. Criar e abrir são operações distintas** [4] — Altera 154 e 156. BufferPool e FileManager têm create, que usa O_CREAT | O_EXCL (falha se o arquivo existir) e inicializa e grava o bloco 0, exceto em temporários, e open, sem O_CREAT (falha se o arquivo não existir), que lê o bloco 0 e confere magic, versão e block_size contra o Config. pread_full trata retorno 0 como erro (fim de arquivo), não como transferência curta. A camada 1 cria cada build num data_dir vazio. Motivo: com O_CREAT na abertura, um caminho errado criava um arquivo vazio, e o pread_full repetiria para sempre a leitura de um cabeçalho inexistente (revisão, P1-2).
- **162. O heap não libera blocos fora do reorganize** [3] — Altera 31 e 120 e mantém 30. O erase do heap marca o slot e atualiza o mapa de espaço livre, sem chamar free_block. Blocos vazios ficam no arquivo, contam em b e na ocupação e só saem no reorganize, que reescreve o heap e reconstrói os índices (141). free_block fica para a B+ e o hash, cujos percursos seguem ponteiros (cadeia de folhas, cadeia de overflow) e não alcançam um bloco liberado. Motivo: allocate_block reaproveita o primeiro bloco livre, o que quebraria a inserção no fim; e, como free_block descarta a moldura sem gravar, o disco poderia guardar a versão do bloco com os registros ainda válidos, que a varredura sequencial, que não vê o bitmap, devolveria. Encontrado ao conferir a revisão.
- **163. Hash NOT UNIQUE dimensionado pelos valores distintos** [3, 1] — Altera 39. Com d valores distintos, o nº de buckets é o primo ≥ min(d, ⌈N / (α × capacidade)⌉). A camada 1 tira d dos CSVs e passa o valor pronto em PrimarySpec.buckets. Motivo: a fórmula do UNIQUE ignora d. Para Aluno por curso (d = 200), ela dá 807 buckets: uns 630 ficam vazios, os usados têm cadeias de cerca de 4 blocos, e b chega a cerca de 1.300, contra 646 do heap; com 211 buckets, b fica perto de 730. Esses números são estimativas, e os reais saem do código. Encontrado ao conferir a revisão.
- **164. Condições para aproveitar ordem e partições** [2] — Detalha 124. O sort-merge só pula a ordenação de uma entrada se o ordered_by do caminho for o campo da junção. Uma entrada hash só é usada como pré-particionada se o campo do hash for o da junção. Se as duas entradas forem, as duas só são aproveitadas se function_id e buckets coincidirem; senão, aproveita-se a maior (o que economiza 2·b dela), e a outra é particionada pelo bucket_of da maior. Motivo: a organização, sozinha, não garante ordem nem partições compatíveis (revisão, P1-6).
- **165. Esquema com dono nos handles** [3] — BlockScan, BlockCursor, TempFile e ScratchSpace guardam std::shared_ptr<const Schema> em vez de const Schema*. O catálogo guarda os esquemas das tabelas em shared_ptr; create_temp e create_scratch copiam o esquema recebido. RecordView continua com ponteiro simples, porque só vale enquanto o handle que a criou existir. Motivo: create_temp recebia o esquema do chamador por referência e guardava o endereço; um esquema temporário, como o de uma projeção, deixaria o ponteiro pendente (revisão, P1-7).
- **166. Diagnóstico do checksum** [2, 1] — ResultShape ganha dump, um caminho opcional. Com ele, o ResultSink grava cada tupla codificada em hexadecimal, uma por linha, por std::ofstream, fora do buffer pool e sem afetar a contagem; reference.py e compare.py ganham a opção --tuplas, que grava as tuplas do SQLite do mesmo jeito e faz um diff ordenado. Usado só nos testes com base pequena, nunca nas medições, porque altera o tempo de relógio. Motivo: o checksum detecta a divergência mas não diz qual tupla diverge (revisão, P1-4).
- **167. RID inválido e ordem dos bytes** [common] — kInvalidRid = {0, 0}: o bloco 0 é sempre cabeçalho, então nunca é um RID real. static_assert(std::endian::native == std::endian::little). Os formatos em disco (RID, cabeçalhos de bloco, slots e nós) são a representação do host, lidos só pelo mesmo binário; não são formato de troca. Motivo: deixar escrito o que o layout fixo supõe (revisão, P1-3).
- **168. Liberar página fixada** [4] — free_block com o bloco fixado, ou com o bloco 0, lança logic_error. Diferente de remove_file e release (155), free_block não é chamado em destrutor e pode lançar. Motivo: o texto dizia "exige o bloco sem pin" sem dizer o que acontecia (revisão, P0-4).
- **169. Sentido de "RID estável"** [3] — Corrige a redação da 111. O RID é estável enquanto o registro fica na mesma página, porque os slots não são renumerados (27). Quando o registro muda de página (divisão, fusão, redistribuição, absorção de overflow ou reorganize), o RID muda, e o MoveListener ou a reconstrução atualizam os índices (141). Motivo: o texto anterior dizia "estável sob qualquer movimentação", o que levaria a supor que um RID nunca muda (revisão, P0-5).

### Decisão 170 (08/11/2026, releitura do enunciado)

- **170. Células que não rodam: não aplicável × inviável** [2, 1] — Altera 128. NotApplicable ganha um tipo:
  - `NaoAplicavel`: a célula não existe. São os casos 1 a 4, descritos na §9.
  - `Inviavel`: a célula existe, mas não cabe em M no nosso desenho (plano inviável ou DoesNotFit, 151).

  No resultados.csv, o status é `ok`, `nao_aplicavel` ou `inviavel`, e cada linha traz a posição na matriz do enunciado (tipo, tamanho, configuração e seletividade). Os casos 1 a 3 são declarados pela bancada sem executar; o caso 4 vem do plano.

  A igualdade em UNIQUE é medida num nível próprio, "pontual", como faz o §6.4 do enunciado e como faz o benchmark Wisconsin. Os níveis de 0,1% e 50% valem como "cerca de" (§6.2 do enunciado) para as seleções por faixa.

  Motivo: o enunciado pede células que contradizem as próprias definições. O resultado precisa mostrá-las com o motivo, sem confundi-las com limites do protótipo.

### Decisão 171 (08/11/2026)

- **171. Hash join com particionamento recursivo e bailout** [2] — Altera 151, 153, a §7 e os limites da §9.
  - **1º passe:** usa até m − 1 partições, ou os grupos de buckets do arquivo hash (164).
  - **Recursão:** uma partição cujo build não cabe em m − 1 páginas é particionada de novo, em até m − 1 subpartições, com uma semente nova a cada passe. A tabela em memória continua com table_seed (153).
  - **Parada:** a partição vai para o bailout em dois casos: quando uma subpartição não diminuiu em relação à partição mãe (grupo indivisível), ou depois de 6 passes.
  - **Bailout:** laço em blocos dentro da partição, com m − 2 páginas de build, 1 do build em leitura e 1 do probe. É o que o SQL Server chama de *hash bailout*.
  - **Partições vazias** são puladas.
  - **Orçamento de molduras:** continua ≤ m em todas as fases.
  - **Plano e estimativa:** o plano prevê o nº de passes pela fórmula do livro, L = ⌈log_{m−1}(b_build) − 1⌉ (JoinPlan.levels). A estimativa é (2L + 1)(b_r + b_s), menos 2b para cada entrada pré-particionada, mais 4·n_h pelos blocos parcialmente cheios.
  - **Resumo da execução:** o fases.csv recebe uma fase final "resumo" com o nº de passes recursivos e de bailouts.

  Motivo: sem recursão, o hash join só valia para b_build ≤ (m − 1)²/1,2, cerca de 800 blocos com m = 32, e ficava inviável em 5 das 6 células do natural join maior ⋈ maior. O particionamento recursivo é a solução dos livros (Silberschatz) e a que os sistemas reais usam (SQL Server: *hash recursion* e *bailout*; PostgreSQL: aumento do nº de lotes).

### Revisão de 08/11/2026 (sem decisões novas)

A reavaliação da arquitetura não criou decisões: corrigiu as existentes e tirou da bancada o que não afeta o resultado.
- **Emendadas por erro que afetava resultados:**
  - 108: tabela menor passa a ser b ≤ M − 2;
  - 132: as repetições olham também o tempo;
  - 143: blocos densos nas runs, partições e estimativas;
  - 146: a varredura pontual em campo UNIQUE para no primeiro registro.
- **Emendadas por inconsistência do documento:** 18 e 41 (bfr medido), 33 (fan-out 510), 122 (regra do sort-merge), 129 (especificação no código), 154 (limite do cabeçalho).
- **Mantidas, mas fora da bancada:** veja "Decisões que a bancada não exercita", na §9.

---

## 6. Camadas e interfaces

O diagrama vigente é o diagrama_camadas_v5.png, com as decisões até a 171 e as emendas de 08/11. Em relação ao v4, ele acrescenta: os repasses encode, suggest_buckets, erase_key e access_paths; o NotApplicable, que distingue não aplicável de inviável (170); e o BPlusEngine. Em texto:

**Camada 1 — Bancada de teste.** Funções do roteiro (main, geração de dados, constantes, lotes, manifesto, execução) e os scripts tools/reference.py e tools/compare.py.
- Chama a camada 2: as primitivas medidas (join, select_points, select_range), que devolvem OpResult ou NotApplicable, e os repasses (definição, carga, manutenção, stats, occupancy, access_paths, encode, suggest_buckets, erase_key, flush_all, invalidate_all).
- Tipos da interface 1 → 2, no cabeçalho público da camada 2: JoinInput, ResultShape, JoinParams e SelectParams.

**Camada 2 — Mini monitor de execução.** Monitor (serviço), JoinPlan e SelectPlan (planos), ResultSink, PhaseRecorder, funções dos algoritmos e das estimativas.
- Chama a camada 3 para acesso (scan_blocks, scan_buckets, bucket_of, find, probe, fetch, schema, create_temp, create_scratch, create_rid_scratch) e para controle, por repasse da camada 4 (reserve, snapshot, flush_all, invalidate_all).
- Recebe BlockScan, SearchCursor, RidCursor, PinnedRecord, TempFile, ScratchSpace e a Reservation, opaca.

**Camada 3 — Gerente de armazenamento.** StorageManager (fachada), cursores (BlockScan, BlockCursor, SearchCursor, RidCursor), RecordView, PinnedRecord, TempFile, ScratchSpace e, por dentro, Catalog, PrimaryFile (HeapFile, BPlusTreeFile, HashFile), SecondaryIndex, MoveListener (IndexMaintainer) e BPlusEngine, o motor B+ comum à B+ primária e aos índices B+ secundários.
- Chama a camada 4 para acesso (create, open, fetch, allocate, scratch, free_block, flush_file, remove_file, header_blocks) e controle (reserve, snapshot, flush_all, invalidate_all).
- Recebe PageGuard e Reservation.

**Camada 4 — Gerente de arquivos e buffer pool.** BufferPool, Reservation, PageGuard, ReplacementPolicy (Lru, Mru), FileManager e UniqueFd.
- O FileManager chama pread, pwrite e ftruncate e é o único ponto que conta leituras e gravações físicas.

**common/.** BlockId, RID, Counters, Config, Stats, AccessPath, OpResult, NotApplicable, Intent, Occupancy e Schema/FieldDef, usados por todas as camadas.

---

## 7. Orçamento de molduras

Toda operação medida usa uma reserva de m molduras. O plano calcula cada parcela, e frames_used() é o maior uso simultâneo, que precisa ser ≤ m.

**BNL**
- chunk = b da externa quando ela é menor (b ≤ m − 2), e o BNL faz uma passada só. Senão, chunk = ⌊m/2⌋: o que sobra fica para a interna, e é aí que a política de substituição faz diferença (§3.1.1 das conclusões).
- Uso simultâneo: chunk páginas da externa + 1 da interna + as páginas da interna guardadas pela política, limitado a m.

**Sort-merge**
- Geração de runs: run_pages = m − 2 páginas de ScratchSpace, mais 1 página de entrada e 1 página de saída = m. Número de runs de cada entrada: ⌈d / run_pages⌉, com d = ⌈n/bfr⌉ (143).
- Passes intermediários: fan_in = m − 1 runs de entrada mais 1 página de saída = m. Cada passe divide o número de runs por fan_in.
- Intercalação final, que é a própria junção: group_pages páginas de ScratchSpace para o grupo de duplicatas da interna (0 se o campo da interna for unique no esquema, decisão 157). final_fan_in = m − group_pages. Os passes intermediários continuam até que runs da externa + runs da interna ≤ final_fan_in.
- Uma entrada já ordenada pelo campo da junção (ordered_by, nas condições da 164) conta como uma única run na intercalação final.
- merge_passes_outer e merge_passes_inner vão para o plano e para a estimativa.

**Hash join (externa = build)**
- Partição: 1 página de entrada + partitions páginas de saída ≤ m, logo partitions ≤ m − 1.
- Build e probe: build_pages = m − 1 páginas de ScratchSpace + 1 página do probe = m.
- Sem particionamento quando d_build ≤ m − 1: os registros cabem densos no ScratchSpace, e o diretório do hash fica fora das molduras (140).
- Senão, partitions = min(m − 1, ⌈d_build × f / build_pages⌉), com fator de folga f = 1,2 declarado.
- **Recursão (171):** uma partição que não cabe é particionada de novo, com as mesmas parcelas de molduras. O nº de passes previsto pela fórmula do livro é L = ⌈log_{m−1}(d_build) − 1⌉, no mínimo 1. A execução decide pelo tamanho real das partições.
- **Bailout (171):** m − 2 páginas de build + 1 do build em leitura + 1 do probe = m.
- Entrada já organizada por hash, nas condições da 164: cada partição é um grupo de ⌈nbuckets / partitions⌉ buckets, lido por scan_buckets.

**Seleções**
- m = 2: uma página do cursor e uma do fetch.
- Variante que ordena os RIDs (117): scratch_pages = m − 2, com m declarado maior.

---

## 8. Entidades e o que cada uma faz

Organizado por camada, de cima para baixo, seguido dos valores de common/ e dos tipos auxiliares.

### Camada 1 — Bancada de teste

#### Roteiro do experimento
Gera os dados, resolve as constantes, sorteia os lotes, executa as operações com repetições e grava resultados, ocupação e manifesto.

Tipo: programa, feito de funções livres. Os tipos desta seção só existem na camada 1. Eles são o desenho com TOML; no protótipo, a especificação é a função spec_padrao() (129).

```cpp
struct PathRef { std::string table; PathKind kind; std::string field; };   // resolvido por access_paths

struct BuildSpec       { PrimarySpec primary; std::vector<IndexSpec> indexes; };
struct TableSpec       { std::string name; Schema schema; std::filesystem::path csv;
                         std::uint64_t rows; std::vector<BuildSpec> builds; };
struct MaintenanceSpec { double insert_ratio, delete_ratio; KeyDistribution keys; std::uint64_t ops; };

enum class OperationKind : std::uint8_t { Join, SelectPoints, SelectRange };
struct Operation {
    std::string   id;
    OperationKind kind;
    std::string   sql;                       // consulta equivalente; o lote é a tabela "lote"
    PathRef       first, second;             // junção: na ordem da consulta; seleção: só first
    std::string   first_field, second_field; // campos da junção
    bool          first_is_outer = true;     // regra da 122
    JoinAlgorithm alg{};
    std::size_t   m = 32;
    ResultShape   shape;
    JoinParams    join_params;
    SelectParams  select_params;             // expected_rows vem dos quantis
    double        selectivity = 0;           // alvo; a faixa sai dos quantis
};

struct ExperimentSpec {
    Config                 baseline;
    std::vector<Variation> variations;       // OFAT (129)
    std::vector<TableSpec> tables;           // (130)
    std::vector<Operation> operations;
    MaintenanceSpec        maintenance;      // (131)
    std::uint64_t          seed;
    int                    min_reps  = 3;    // (132)
    int                    max_reps  = 10;
    double                 tolerance = 0.05;
};

ExperimentSpec load_spec(const std::filesystem::path&);
void generate_data(const ExperimentSpec&);
void resolve_constants(ExperimentSpec&);
void prepare_batches(const ExperimentSpec&);
void write_manifest(const std::filesystem::path&, const ExperimentSpec&);
void run_experiment(const ExperimentSpec&, CsvWriter& results, CsvWriter& occupancy);

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "uso: bancada <especificacao.toml>\n"; return 2; }
    auto spec = load_spec(argv[1]);
    generate_data(spec);
    resolve_constants(spec);
    prepare_batches(spec);
    write_manifest("manifesto.txt", spec);
    CsvWriter results{"resultados.csv"}, occupancy{"ocupacao.csv"};
    run_experiment(spec, results, occupancy);
}
```

- **main(argc, argv)** — Confere os argumentos e chama as etapas na ordem.
- **load_spec(path)** — Lê o TOML e valida: soma das áreas, m de cada operação ≤ área de trabalho, índice clustering só sobre B+ ou hash e no campo da organização, hash secundário só em campo unique (157), cardinalidade mínima para hash, nº de buckets do hash NOT UNIQUE pelos distintos (163) e limites de repetição.
- **generate_data(spec)** — Gera um CSV por tabela, com semente. CR sai em int32 × 100 e os textos sem preenchimento, para que o SQLite leia os mesmos valores.
- **resolve_constants(spec)** — Calcula, pelos quantis dos CSVs, as constantes de cada faixa e as linhas esperadas, e grava constantes.csv para o script de referência.
- **prepare_batches(spec)** — Sorteia o lote de cada seleção pontual e repetição, com a semente da repetição, e grava em lotes/<operação>_<repetição>.csv.
- **write_manifest(path, spec)** — Grava commit, compilador, flags, data e um hash da especificação (133).
- **run_experiment(spec, results, occupancy)** — Executa o roteiro abaixo.

Roteiro de run_experiment:
1. Para cada configuração e cada build: cria o Monitor sobre um data_dir vazio (161), cria tabelas e índices, carrega com bulk_load e chama flush_all, que faz parte da carga e não é medido.
2. Resolve cada PathRef com access_paths e monta os JoinInput.
3. Grava a ocupação de antes da manutenção.
4. Para cada operação e repetição: chama flush_all (deve gravar zero; se não, registra aviso) e invalidate_all, lê o lote da repetição, executa e grava uma linha no CSV com linhas e checksum.
5. Encerra as repetições com pelo menos min_reps e variação abaixo da tolerância, ou ao atingir max_reps.
6. Aplica a carga de manutenção e grava a ocupação de depois. Nenhuma operação medida roda depois da manutenção sem um reorganize, porque as estatísticas ficam desatualizadas (160).

#### Scripts de conferência (tools/)
Reproduzem cada operação no SQLite e comparam com o motor.

Tipo: scripts Python, fora do motor; usam só a biblioteca padrão.

- **reference.py** — Carrega os CSVs no SQLite com os tipos do esquema da especificação. Para cada operação e repetição, carrega o lote como tabela "lote", substitui as constantes de constantes.csv, executa o SQL, ordena os campos como o ResultShape manda e grava em referencia.csv: operação, repetição, linhas e checksum canônico.
- **compare.py** — Cruza resultados.csv com referencia.csv por operação e repetição, lista as divergências e termina com código de erro se houver alguma.
- **Opção --tuplas** (nos dois scripts) — reference.py grava as tuplas codificadas como o ResultSink, uma por linha, e compare.py faz o diff ordenado contra o dump do motor. Só para testes com base pequena (166).

### Camada 2 — Mini monitor de execução

#### Tipos da interface com a camada 1
Ficam no cabeçalho público da camada 2, porque a camada 1 os monta.

Tipo: valores.

```cpp
struct JoinInput {
    AccessPath   path;
    FieldId      field;
    std::uint8_t operand;   // 0 = primeiro operando da consulta (mestre); 1 = segundo
};
struct ResultShape {
    std::vector<FieldId> operand0_fields, operand1_fields;   // vazio = todos, na ordem do esquema
    std::optional<std::filesystem::path> dump;               // só em testes: grava as tuplas (166)
};
struct JoinParams {
    std::size_t chunk = 0;          // 0 = derivar de m (seção 7)
    std::size_t partitions = 0;
    std::size_t group_pages = 2;    // grupo de duplicatas do sort-merge
};
struct SelectParams {
    std::size_t m = 2;
    double      expected_rows = 0;
    bool        sort_rids = false;  // variante da 117
};
```

#### Monitor
Fachada da camada 2: executa as operações medidas e repassa à camada 1 a definição, a carga e a manutenção.

Tipo: serviço, uma classe não copiável por configuração. É dona do StorageManager por composição.

```cpp
class Monitor {
public:
    explicit Monitor(const Config&);
    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    // primitivas medidas
    Outcome join(JoinAlgorithm, const JoinInput& outer, const JoinInput& inner,
                 std::size_t m, const ResultShape&, const JoinParams& = {});
    Outcome select_points(const AccessPath&, std::span<const Point> batch,
                          const ResultShape&, const SelectParams& = {});
    Outcome select_range (const AccessPath&, const KeyRange&,
                          const ResultShape&, const SelectParams& = {});

    // repasses da camada 3 (não medidos)
    TableId     create_table(std::string name, Schema, PrimarySpec);
    StructureId create_index(TableId, IndexSpec);
    std::vector<AccessPath> access_paths(TableId) const;
    TableStats  stats(TableId) const;
    Occupancy   occupancy(StructureId) const;
    void bulk_load(TableId, RecordStream& sorted);
    RID  insert(TableId, std::span<const std::byte> record);
    void erase(TableId, RID);
    void reorganize(TableId);
    std::size_t erase_key(TableId, FieldId, const Key&);          // manutenção por chave
    std::vector<std::byte> encode(TableId, const std::vector<Value>&) const;   // formato do registro (158)
    std::uint32_t suggest_buckets(const Schema&, std::uint64_t n, std::uint64_t distinct, double alpha) const;   // (39, 163)
    std::uint64_t flush_all();
    void          invalidate_all();
private:
    std::optional<JoinPlan>   plan_join(JoinAlgorithm, const JoinInput&, const JoinInput&,
                                        std::size_t m, const JoinParams&) const;
    std::optional<SelectPlan> plan_select(const AccessPath&, std::size_t batch,
                                          const SelectParams&) const;
    StorageManager storage_;
};
```

- **construtor (Config)** — Constrói o StorageManager, que constrói o BufferPool e o FileManager. Nada é medido.
- **join(alg, externa, interna, m, shape, params)** — Executa uma junção medida, seguindo os passos abaixo. No hash join, a externa é o build (152).
- **select_points(caminho, lote, shape, params)** — Busca cada chave do lote e soma tudo numa medida. Caminho Scan: uma varredura com filtro por chave, na camada 2. B+ primária, hash primário ou índice clustering: find. Hash secundário ou B+ não-clustering: probe e depois fetch.
- **select_range(caminho, faixa, shape, params)** — Uma busca por faixa pelas mesmas regras. O caminho Scan sempre executa. Hash primário e hash secundário devolvem NotApplicable com o motivo (128).
- **create_table, create_index, access_paths, stats, occupancy, bulk_load, insert, erase, erase_key, reorganize, encode, suggest_buckets, flush_all, invalidate_all** — Repasses da camada 3, sem lógica e sem medição. erase_key, encode e suggest_buckets existem porque a camada 1 não acompanha RIDs que mudam, não conhece o formato do registro e não sabe a capacidade do bucket.
- **plan_join e plan_select** (privados) — Resolvem o plano completo, com o orçamento de molduras da seção 7. Devolvem vazio se a combinação for impossível ou inviável em m.

Passos das primitivas medidas:
1. Resolve o plano. Se for vazio, devolve NotApplicable sem executar nada. O plano só lê estatísticas do catálogo e não faz E/S (160).
2. Verifica plan.frames_used() ≤ m.
3. Reserva m molduras.
4. Tira o snapshot inicial e cria o PhaseRecorder.
5. Executa o algoritmo ou a busca, mandando os resultados ao ResultSink. Se o algoritmo lançar DoesNotFit, libera a reserva e devolve NotApplicable{"não cabe em M"}.
6. Tira o snapshot final e fecha as fases. Se o delta de data_area_requests não for zero, lança logic_error: alguma página de dados foi pedida sem reserva (159).
7. Chama flush_all e guarda o retorno em flush_writes, que deve ser zero.
8. Calcula o custo estimado com o mesmo plano e as estatísticas medidas.
9. Libera a reserva e devolve o OpResult, com delta = snapshot final − inicial.

#### JoinPlan e SelectPlan
Os parâmetros resolvidos de uma operação, incluindo o orçamento de molduras, usados igualmente pela execução e pela estimativa.

Tipo: valores internos da camada 2.

```cpp
struct JoinPlan {
    JoinAlgorithm alg;
    std::size_t   m;
    bool          swapped = false;                      // externa é o operando 1
    // BNL
    std::size_t   chunk = 0;
    // sort-merge
    bool          sort_outer = true, sort_inner = true;
    std::size_t   run_pages = 0;                        // m − 2
    std::size_t   fan_in = 0;                           // m − 1
    std::size_t   group_pages = 0;                      // 0 se o campo da interna for unique (157)
    std::size_t   final_fan_in = 0;                     // m − group_pages
    std::size_t   merge_passes_outer = 0, merge_passes_inner = 0;
    // hash join (externa = build)
    bool          prepartitioned_outer = false, prepartitioned_inner = false;
    std::size_t   partitions = 0;                       // ≤ m − 1
    std::size_t   levels = 0;                           // passes de particionamento previstos (171)
    std::size_t   build_pages = 0;                      // m − 1
    std::uint64_t partition_seed = 1, table_seed = 2;   // independentes (153)
    std::size_t   frames_used() const;                  // maior uso simultâneo
};
struct SelectPlan {
    PathKind    path;
    bool        filter_in_layer2 = false;               // caminho Scan
    std::size_t batch = 1;
    std::size_t m = 2;
    std::size_t scratch_pages = 0;                      // variante 117
    double      expected_rows = 0;
    bool        sort_rids = false;
    std::size_t frames_used() const;
};
```

- **frames_used()** — Devolve o maior número de molduras usadas ao mesmo tempo em qualquer fase do plano. O Monitor exige que seja ≤ m.

#### Algoritmos e estimativas
Cada algoritmo executa uma junção dentro da reserva, seguindo o plano. Cada estimativa aplica a fórmula dos livros ao mesmo plano.

Tipo: funções livres, sem estado entre chamadas. DoesNotFit é a exceção lançada quando a memória de trabalho não comporta um grupo ou uma partição.

```cpp
struct DoesNotFit : std::runtime_error { using std::runtime_error::runtime_error; };

void bnl_join       (StorageManager&, const JoinInput& outer, const JoinInput& inner,
                     const JoinPlan&, const Reservation&, ResultSink&, PhaseRecorder&);
void sort_merge_join(StorageManager&, const JoinInput& outer, const JoinInput& inner,
                     const JoinPlan&, const Reservation&, ResultSink&, PhaseRecorder&);
void hash_join      (StorageManager&, const JoinInput& outer, const JoinInput& inner,
                     const JoinPlan&, const Reservation&, ResultSink&, PhaseRecorder&);

Cost estimate_join  (const JoinPlan&, const TableStats& outer, const TableStats& inner);
Cost estimate_select(const SelectPlan&, const TableStats&, const StructureStats* index);
```

- **bnl_join** — Mantém plan.chunk BlockCursors da externa fixados (kSeqOnce) e, para cada chunk, percorre a interna inteira (kSeqReuse), emitindo os pares. As molduras restantes guardam páginas da interna conforme a política.
- **sort_merge_join** — Para cada entrada a ordenar: enche um ScratchSpace de run_pages páginas, ordena, grava num TempFile e sela; depois faz merge_passes passes com fan_in. Uma entrada com ordered_by é lida em ordem direto. Na intercalação final, guarda o grupo de duplicatas da interna num ScratchSpace de group_pages; se não couber, lança DoesNotFit.
- **hash_join** — Com a externa como build: particiona em plan.partitions TempFiles cada entrada não pré-particionada, usando FNV-1a com partition_seed, ou bucket_of quando a outra entrada é um arquivo hash; sela as partições. Uma entrada pré-particionada é lida por scan_buckets. Para cada partição, carrega o build num ScratchSpace de build_pages, chama build_hash com table_seed e percorre o probe. Se o build não couber, particiona a partição de novo, com outra semente; se ela não diminuir, ou depois de 6 passes, usa o bailout em blocos (171).
- **estimate_join(plano, externa, interna)** — Aplica a fórmula do algoritmo usando b de cada tabela (119), o fan-in, os passes e as partições do plano. Devolve transferências e seeks.
- **estimate_select(plano, tabela, índice)** — Aplica a fórmula do caminho de acesso: altura e nós internos da B+, buckets e overflow do hash, ou b do arquivo no Scan, com o lote e as linhas esperadas.

#### ResultSink
Conta os resultados e calcula o checksum canônico, o mesmo do script de referência.

Tipo: acumulador, só move: com dump, é dono do arquivo de saída (166).

```cpp
class ResultSink {
public:
    ResultSink(ResultShape, bool swapped);             // swapped vem do plano
    void emit(const RecordView& r);                    // seleção: operando 0
    void emit_pair(const RecordView& outer, const RecordView& inner);
    std::uint64_t rows() const;
    std::uint64_t checksum() const;
private:
    ResultShape   shape_;
    bool          swapped_;
    std::uint64_t rows_ = 0, sum_ = 0;
    std::unique_ptr<std::ofstream> dump_;               // só com shape.dump (166)
};
```

- **construtor (shape, swapped)** — Guarda a forma e se a externa é o operando 1.
- **emit(registro)** — Soma uma linha e adiciona ao checksum o FNV-1a da codificação canônica: cada campo precedido do byte de marca de NULL (158), inteiros como int64 little-endian, textos em UTF-8 sem preenchimento com prefixo de tamanho. Com shape.dump, grava também a tupla codificada em hexadecimal, uma por linha (166).
- **emit_pair(externa, interna)** — Põe o par na ordem dos operandos, codifica e soma. O checksum não muda quando os papéis são trocados.
- **rows() e checksum()** — Devolvem os totais para o OpResult.

#### PhaseRecorder
Mede cada fase de uma operação entre dois snapshots.

Tipo: auxiliar sem recursos.

```cpp
class PhaseRecorder {
public:
    explicit PhaseRecorder(StorageManager&);
    void begin(std::string name);
    std::vector<Phase> finish();
private:
    StorageManager& sm_;
    std::optional<std::pair<std::string, Counters>> open_;
    std::vector<Phase> done_;
};
```

- **begin(nome)** — Tira um snapshot, fecha a fase aberta com a diferença e abre a nova.
- **finish()** — Fecha a última fase e devolve a lista. O Monitor chama antes do flush de fim.

### Camada 3 — Gerente de armazenamento

#### StorageManager
Fachada da camada 3: cria tabelas e índices, esconde as organizações e o catálogo, e oferece acesso, memória de trabalho, carga e manutenção.

Tipo: serviço, classe não copiável. O destrutor é definido no .cpp, porque PrimaryFile e SecondaryIndex são incompletos no cabeçalho.

```cpp
class StorageManager {
public:
    explicit StorageManager(const Config&);
    ~StorageManager();
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    // definição
    TableId       create_table(std::string name, Schema, PrimarySpec);
    StructureId   create_index(TableId, IndexSpec);
    const Schema& schema(TableId) const;

    // acesso
    std::vector<AccessPath> access_paths(TableId) const;
    BlockScan     scan_blocks(TableId, Intent, const Reservation&);
    BlockScan     scan_buckets(const AccessPath&, std::uint32_t first, std::uint32_t last,
                               Intent, const Reservation&);
    std::uint32_t bucket_of(const AccessPath&, const Key&) const;
    SearchCursor  find(const AccessPath&, const Predicate&, Intent, const Reservation&);
    RidCursor     probe(const AccessPath&, const Predicate&, Intent);
    PinnedRecord  fetch(TableId, RID, Intent, const Reservation&);

    // memória de trabalho e temporários
    TempFile      create_temp(const Schema&, const Reservation&);
    ScratchSpace  create_scratch(const Schema&, const Reservation&, std::size_t pages);
    ScratchSpace  create_rid_scratch(const Reservation&, std::size_t pages);

    // estatística e ocupação
    TableStats stats(TableId) const;
    Occupancy  occupancy(StructureId) const;

    // carga e manutenção
    void bulk_load(TableId, RecordStream& sorted);
    RID  insert(TableId, std::span<const std::byte> record);
    void erase(TableId, RID);
    void reorganize(TableId);

    // repasses da camada 4
    Reservation   reserve(std::size_t frames);
    Counters      snapshot() const;
    std::uint64_t flush_all();
    void          invalidate_all();
private:
    BufferPool      pool_;          // declarado primeiro: destruído por último
    Catalog         catalog_;
    std::unordered_map<TableId, std::unique_ptr<PrimaryFile>>        tables_;
    std::unordered_map<StructureId, std::unique_ptr<SecondaryIndex>> indexes_;
    IndexMaintainer maintainer_;    // MoveListener que atualiza os índices
};
```

Definição
- **create_table(nome, esquema, primária)** — Cria a organização pedida e registra a tabela no catálogo. Heap e hash usam um arquivo de dados. A B+ primária usa dois: um arquivo Index para os nós internos e o arquivo de dados para as folhas (145). Os arquivos são criados com create (161); unicidade e anulabilidade vêm do esquema (157).
- **create_index(tabela, spec)** — Cria um índice secundário, hash ou B+, sempre num arquivo Index. O clustering só é aceito sobre B+ ou hash primários e no campo da organização (116). Se a tabela tiver dados, constrói o índice a partir deles.
- **schema(tabela)** — Devolve o esquema, para a camada 2 criar temporários e espaços de trabalho.

Acesso
- **access_paths(tabela)** — Lista os caminhos com os descritores. Só consulta o catálogo fixado.
- **scan_blocks(tabela, intent, reserva)** — Devolve um BlockScan sobre o arquivo de dados, na ordem da organização (no heap, sequencial; na B+, pela cadeia de folhas; no hash, buckets com overflow). Não lê nada até o primeiro next().
- **scan_buckets(caminho, primeiro, último, intent, reserva)** — Só no hash primário: BlockScan sobre o intervalo de buckets, com as cadeias de overflow.
- **bucket_of(caminho, chave)** — Bucket da chave pela função de hash do arquivo. Não lê nada.
- **find(caminho, predicado, intent, reserva)** — Para B+ primária, hash primário (só igualdade) e índice clustering. Os nós internos e as páginas do índice são pedidos sem reserva, na área de índices; as páginas de dados, pela reserva. Devolve um SearchCursor. Recusa o caminho Scan. Com campo unique e predicado Point, para no primeiro registro encontrado (157).
- **probe(caminho, predicado, intent)** — Para hash secundário e B+ não-clustering: devolve um RidCursor. As páginas do índice ficam na área de índices.
- **fetch(tabela, RID, intent, reserva)** — Fixa o bloco do RID pela reserva e devolve um PinnedRecord.

Memória de trabalho e temporários
- **create_temp(esquema, reserva)** — Cria um arquivo temporário (create, 161), fora do catálogo, e devolve um TempFile que guarda o id da reserva e uma cópia do esquema em shared_ptr (165).
- **create_scratch(esquema, reserva, páginas)** — Pede as molduras anônimas com BufferPool::scratch e devolve um ScratchSpace para registros do esquema, com uma cópia dele (165).
- **create_rid_scratch(reserva, páginas)** — O mesmo, com o esquema sintético de RID (140).

Estatística e ocupação
- **stats(tabela)** — N, R, bfr e b (blocos do arquivo de dados, decisão 119), e as estatísticas de cada estrutura, incluindo altura e blocos internos da B+. Só lê o catálogo, sem E/S. As estatísticas são medidas ao fim de bulk_load e de reorganize; insert e erase as marcam como desatualizadas, e então stats() lança logic_error (160).
- **occupancy(estrutura)** — Blocos de dados, overflow, cada nível de índice, cabeçalho e diretório. O número de blocos de cabeçalho vem de BufferPool::header_blocks.

Carga e manutenção
- **bulk_load(tabela, fluxo)** — Constrói a organização a partir de um fluxo ordenado e depois os índices.
- **insert(tabela, registro)** — Insere na organização primária e nos índices. Registros movidos são avisados ao MoveListener. Se o campo for unique e alguma estrutura o indexar, uma duplicata lança DuplicateKey (157).
- **erase(tabela, RID)** — Remove e atualiza os índices. Na B+ e no hash, libera blocos esvaziados com free_block; no heap, só marca o slot e atualiza o mapa de espaço livre (162).
- **reorganize(tabela)** — Compacta, reconstrói ou refaz o hash, recalcula campos derivados e reconstrói os índices. No heap, reescreve o arquivo sem blocos vazios (162). Mede de novo as estatísticas (160).

Repasses
- **reserve, snapshot, flush_all, invalidate_all** — Só encaminham ao BufferPool.

#### Entidades internas da camada 3
Não aparecem na interface, mas sustentam as correções.

```cpp
class Catalog;          // fixa as páginas da área de catálogo na abertura e só as solta no fim;
                        // guarda os esquemas em shared_ptr<const Schema> (165) e as estatísticas (160)
class PrimaryFile;      // HeapFile, BPlusTreeFile (dois arquivos), HashFile
class SecondaryIndex;   // HashIndex, BPlusIndex (clustering ou não)

struct DuplicateKey : std::runtime_error { using std::runtime_error::runtime_error; };   // (157)

struct MoveListener {
    virtual ~MoveListener() = default;
    virtual void moved(TableId, RID from, RID to, const RecordView& rec) = 0;
};

struct SearchState {                       // implementado por cada organização e pelo clustering
    virtual ~SearchState() = default;
    virtual std::optional<RecordView> next() = 0;
protected:
    static RecordView make_view(std::span<const std::byte>, const Schema*);   // amizade não é herdada
};
struct RidState {
    virtual ~RidState() = default;
    virtual std::optional<RID> next() = 0;
};
```

- **MoveListener::moved(tabela, de, para, registro)** — Chamado a cada registro movido. O IndexMaintainer atualiza a entrada em cada índice da tabela.
- **SearchState::make_view(bytes, esquema)** — Cria um RecordView para as subclasses, que não herdam a amizade.

#### BlockScan
Percorre os blocos de um arquivo na ordem física da organização e entrega um BlockCursor por bloco.

Tipo: cursor, só move. Guarda o id da reserva e um ScanPlan, sem apontar para objetos que possam ser movidos.

```cpp
struct ScanPlan {
    enum class Order : std::uint8_t { Sequential, LeafChain, BucketChains } order;
    FileId        file;
    std::uint32_t first;   // 1º bloco de dados (≥ 1), 1ª folha ou 1º bucket
    std::uint32_t count;   // nº de blocos ou de buckets
};

class BlockScan {
public:
    BlockScan(BlockScan&&) noexcept = default;
    BlockScan& operator=(BlockScan&&) noexcept = default;
    BlockScan(const BlockScan&) = delete;
    std::optional<BlockCursor> next();
private:
    friend class StorageManager;
    friend class TempFile;
    BlockScan(BufferPool*, ScanPlan, std::shared_ptr<const Schema>, Intent, ReservationId);
    BufferPool*                   pool_;
    ScanPlan                      plan_;
    std::shared_ptr<const Schema> schema_;   // (165)
    Intent                 intent_;
    ReservationId          res_;
    std::optional<BlockId> next_;
};
```

- **next()** — Fixa o próximo bloco no subpool e o entrega num BlockCursor. Antes, lê no cabeçalho do bloco o endereço do seguinte. No fim, devolve vazio. Se a reserva já foi liberada, lança logic_error.

#### BlockCursor
Mantém um bloco fixado e percorre os registros válidos dele.

Tipo: handle e cursor, só move.

```cpp
class BlockCursor {
public:
    BlockCursor(BlockCursor&&) noexcept = default;
    BlockCursor& operator=(BlockCursor&&) noexcept = default;
    BlockCursor(const BlockCursor&) = delete;
    std::optional<RecordView> next();
    BlockId block() const;
private:
    friend class StorageManager;
    friend class BlockScan;
    BlockCursor(PageGuard, std::shared_ptr<const Schema>);
    PageGuard                     guard_;
    std::shared_ptr<const Schema> schema_;   // (165)
    std::uint16_t slot_ = 0;
};
```

- **next()** — Próximo registro válido do bloco, pulando os removidos.
- **block()** — BlockId do bloco.
- **destrutor** — Solta o pin.

#### SearchCursor
Entrega os registros que satisfazem o predicado, pela organização primária ou pelo índice clustering.

Tipo: handle e cursor, só move, com estado polimórfico.

```cpp
class SearchCursor {
public:
    SearchCursor(SearchCursor&&) noexcept;              // definidos no .cpp
    SearchCursor& operator=(SearchCursor&&) noexcept;
    SearchCursor(const SearchCursor&) = delete;
    ~SearchCursor();
    std::optional<RecordView> next();
private:
    friend class StorageManager;
    explicit SearchCursor(std::unique_ptr<SearchState>);
    std::unique_ptr<SearchState> state_;
};
```

- **next()** — Próximo registro que satisfaz o predicado. Solta o bloco anterior ao passar ao seguinte e para quando a chave passa do limite ou a cadeia termina. O registro vale até a próxima chamada.

#### RidCursor
Entrega os RIDs de um índice secundário, na ordem das posting lists, sem acumular a lista.

Tipo: handle e cursor, só move.

```cpp
class RidCursor {
public:
    RidCursor(RidCursor&&) noexcept;
    RidCursor& operator=(RidCursor&&) noexcept;
    RidCursor(const RidCursor&) = delete;
    ~RidCursor();
    std::optional<RID> next();
private:
    friend class StorageManager;
    explicit RidCursor(std::unique_ptr<RidState>);
    std::unique_ptr<RidState> state_;
};
```

- **next()** — Próximo RID. Passa à folha seguinte ou ao overflow da posting list e para quando a chave passa do limite.

#### RecordView
Lê os campos de um registro diretamente da moldura, sem copiar.

Tipo: visão, leve e copiável; vale enquanto o guard do bloco existir.

```cpp
class RecordView {
public:
    template <class T> T get(FieldId f) const {             // verificação sempre ligada (158)
        static_assert(std::is_trivially_copyable_v<T>);
        const auto& field = schema_->field(f);
        if (field.type == FieldType::VarChar || sizeof(T) != field.size ||
            field.offset + sizeof(T) > bytes_.size())
            throw std::out_of_range("RecordView::get: tipo, tamanho ou limite");
        if (is_null(f)) throw std::logic_error("RecordView::get: campo NULL");
        T v;
        std::memcpy(&v, bytes_.data() + field.offset, sizeof v);
        return v;
    }
    bool is_null(FieldId f) const;                          // bit do null bitmap; falso se não anulável
    std::span<const std::byte> get_var(FieldId f) const;    // VarChar; confere os offsets
    Key key(FieldId f) const;                               // lança se o campo for NULL
    std::span<const std::byte> bytes() const { return bytes_; }
private:
    friend class BlockCursor;
    friend class StorageManager;
    friend class ScratchSpace;
    friend class ScratchMatches;
    friend struct SearchState;      // só o auxiliar make_view usa
    RecordView(std::span<const std::byte>, const Schema*);
    std::span<const std::byte> bytes_;
    const Schema* schema_;
};
```

- **get<T>(campo)** — Lê um campo fixo pelo deslocamento do esquema, com memcpy. Confere sempre, também em release, que o campo é fixo, que sizeof(T) é o tamanho do campo e que ele cabe no registro; campo NULL lança (158).
- **is_null(campo)** — Lê o bit do campo no null bitmap; campo não anulável devolve falso.
- **get_var(campo)** — Bytes de um campo VarChar, delimitados pela tabela de offsets do registro. Lança se os offsets não forem crescentes ou saírem do registro.
- **key(campo)** — O campo como Key (int32, int64 ou texto sem preenchimento). Usado no filtro da camada 2, no checksum e em bucket_of. Lança se o campo for NULL: filtros e junções testam is_null antes, e chave NULL nunca casa (158).
- **bytes()** — Os bytes do registro, para cópia em temporários e espaços de trabalho.

#### PinnedRecord
Um registro buscado por RID, junto com o pin do bloco.

Tipo: handle, só move.

```cpp
struct PinnedRecord {
    PageGuard  guard;   // declarado antes: destruído depois da visão
    RecordView view;
};
```

Não tem métodos. Ao ser destruído, solta o pin.

#### TempFile
Arquivo temporário para runs e partições: selado antes de ser lido e apagado quando o objeto é destruído.

Tipo: handle RAII, só move. Guarda o id da reserva e usa só ela.

```cpp
class TempFile {
public:
    TempFile(TempFile&&) noexcept;
    TempFile& operator=(TempFile&&) noexcept;
    TempFile(const TempFile&) = delete;
    ~TempFile();                                // noexcept; página fixada aqui é erro fatal
    void          append(const RecordView& r);
    void          seal();
    bool          sealed() const;
    std::uint32_t blocks() const;
    BlockScan     scan_blocks(Intent);          // usa a reserva do próprio arquivo
private:
    friend class StorageManager;
    TempFile(BufferPool*, FileId, std::shared_ptr<const Schema>, ReservationId);
    BufferPool*                   pool_;
    FileId                        file_;
    std::shared_ptr<const Schema> schema_;   // cópia própria (165)
    ReservationId            res_;
    std::optional<PageGuard> tail_;
    std::uint32_t            blocks_ = 0;
    bool                     sealed_ = false;
};
```

- **append(registro)** — Copia o registro para a página em escrita; quando ela enche, solta e aloca a próxima pela reserva. Depois do seal, lança exceção.
- **seal()** — Solta a página em escrita e chama flush_file com retirada. As gravações contam como gravações do tipo temp.
- **sealed() e blocks()** — Estado e tamanho.
- **scan_blocks(intent)** — Exige o arquivo selado e devolve um BlockScan sequencial na reserva do arquivo.
- **destrutor** — Solta a página em escrita e chama remove_file. Não lança: um cursor ainda vivo sobre o arquivo encerra o programa com diagnóstico (155).

#### ScratchSpace
Memória de trabalho do operador, em molduras da reserva: guarda registros ou RIDs, ordena, monta hash e responde sondagens.

Tipo: handle RAII, só move. As molduras são anônimas, nunca vão ao disco e não são contadas. Os diretórios internos são estruturas da linguagem limitadas pela capacidade (cerca de 4 B por registro).

```cpp
class ScratchSpace {
public:
    ScratchSpace(ScratchSpace&&) noexcept;
    ScratchSpace& operator=(ScratchSpace&&) noexcept;
    ScratchSpace(const ScratchSpace&) = delete;
    ~ScratchSpace();                                      // noexcept
    bool           add(const RecordView& r);              // false quando cheio
    bool           add(RID rid);                          // só no espaço de RIDs
    std::size_t    size() const;
    std::size_t    capacity() const;
    RecordView     at(std::size_t i) const;
    RID            rid_at(std::size_t i) const;
    void           sort_by(FieldId);
    void           sort_rids();                           // por (bloco, slot)
    void           build_hash(FieldId, std::uint64_t seed);
    ScratchMatches probe(const Key&) const;
    void           clear();
private:
    friend class StorageManager;
    ScratchSpace(std::vector<PageGuard> pages, std::shared_ptr<const Schema>);
    std::vector<PageGuard>        pages_;
    std::shared_ptr<const Schema> schema_;                // cópia própria (165); sintético no espaço de RIDs
    std::vector<std::uint32_t> slots_;
    std::vector<std::uint32_t> heads_, chain_;
    FieldId                    hash_field_{};
    std::uint64_t              hash_seed_ = 0;
};

class ScratchMatches {
public:
    std::optional<RecordView> next();
};
```

- **add(registro)** — Copia o registro, com o tamanho real (bytes()), para as molduras; devolve false quando não cabe mais.
- **add(RID)** — Guarda o RID como chave Int64 = bloco << 16 | slot.
- **size() e capacity()** — Itens guardados e quantos cabem.
- **at(i) e rid_at(i)** — O i-ésimo item na ordem atual.
- **sort_by(campo) e sort_rids()** — Ordenam o diretório; os itens não mudam de lugar.
- **build_hash(campo, semente)** — Monta a tabela de hash com FNV-1a semeado, diferente da semente do particionamento.
- **probe(chave)** — Registros com a chave, depois do build_hash.
- **clear()** — Esvazia sem soltar as molduras.
- **destrutor** — Devolve as molduras ao subpool. Não lança.

### Camada 4 — Gerente de arquivos e buffer pool

#### BufferPool
Mantém as molduras divididas em regiões (quatro áreas e os subpools das reservas), decide entre acerto e falta, escolhe vítimas pela política de cada região e soma as métricas do buffer.

Tipo: serviço, classe não copiável. Cada região tem uma instância própria da política.

```cpp
class BufferPool {
public:
    explicit BufferPool(const Config&);
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    ~BufferPool();

    // acesso
    FileId        create(const std::string& name, FileKind);   // falha se existir (161)
    FileId        open(const std::string& name, FileKind);     // falha se não existir (161)
    PageGuard     fetch(BlockId, Intent, ReservationId);       // sem valor padrão (159)
    std::pair<BlockId, PageGuard> allocate(FileId, ReservationId);
    PageGuard     scratch(ReservationId);
    void          free_block(BlockId);
    void          flush_file(FileId, bool evict);
    void          remove_file(FileId) noexcept;
    std::uint32_t header_blocks(FileId) const;

    // controle
    Reservation   reserve(std::size_t frames);
    Counters      snapshot() const;
    std::uint64_t flush_all();
    void          invalidate_all();
private:
    friend class PageGuard;
    friend class Reservation;
    struct Region { std::vector<FrameId> frames; std::unique_ptr<ReplacementPolicy> policy; };
    struct Frame  { BlockId id; std::uint32_t pins = 0; bool dirty = false;
                    bool anonymous = false; std::uint32_t region = 0; };
    void unpin(FrameId, Intent);
    void release(ReservationId) noexcept;
    std::vector<std::byte> arena_;
    std::vector<Frame>     frames_;
    std::unordered_map<BlockId, FrameId, BlockIdHash> page_table_;
    std::vector<Region>    regions_;                   // 0–3: áreas; depois, subpools ativos
    std::unordered_map<ReservationId, std::uint32_t> subpool_of_;
    FileManager files_;
    Counters    counters_;
};
```

Acesso
- **create(nome, tipo)** — Cria o arquivo pelo FileManager e associa o tipo à área usada nos acessos sem reserva. Falha se o arquivo já existir (161).
- **open(nome, tipo)** — O mesmo para um arquivo existente. Falha se ele não existir ou se o cabeçalho não conferir com o Config (161).
- **fetch(BlockId, intent, res)** — Com reserva, a página vai para o subpool; sem reserva, para a área do tipo do arquivo. Acerto soma requisição e acerto. Falta soma requisição e falta, escolhe vítima na região, grava a vítima se estiver suja e lê o bloco. Se a página estiver em outra região, migra para a região pedida (150). Com prefetch e intent.sequential, lê antecipadamente até extent_blocks − 1 blocos, expulsando só páginas não fixadas (150). Lança exceção se todas as molduras da região estiverem fixadas ou se a reserva já foi liberada. Um pedido sem reserva (kNoReservation) a arquivo Data soma em data_area_requests (159).
- **allocate(arquivo, res)** — Pede um bloco ao FileManager (nunca o 0) e o põe numa moldura zerada, suja e fixada, sem ler o disco. Soma em allocations.
- **scratch(res)** — Entrega uma moldura anônima do subpool, sem bloco, que nunca vai ao disco. Lança exceção se o subpool estiver todo fixado.
- **free_block(BlockId)** — Descarta a moldura do bloco sem gravar e libera o bloco no bitmap. Bloco fixado, ou bloco 0, lança logic_error (168). Só a B+ e o hash usam (162).
- **flush_file(arquivo, evict)** — Grava as páginas sujas do arquivo, contadas como gravações do tipo dele. Com evict, também as tira do pool.
- **remove_file(arquivo)** — Descarta as molduras do arquivo sem gravar e sem contar e apaga o arquivo. Não lança; uma página fixada encerra o programa com diagnóstico.
- **header_blocks(arquivo)** — Número de blocos de cabeçalho do arquivo, vindo do FileManager.

Controle
- **reserve(n)** — Tira n molduras livres da área de trabalho e forma um subpool com uma política nova. Lança exceção se não houver n livres.
- **snapshot()** — Copia os contadores do pool e do FileManager para um Counters.
- **flush_all()** — Grava todas as páginas sujas e os cabeçalhos sujos e devolve quantas gravações fez.
- **invalidate_all()** — Esvazia todas as molduras, menos as da área de catálogo, sem gravar e sem contar. Exige flush prévio e nenhuma página fixada fora do catálogo.
- **unpin(frame, intent)** (privado) — Decrementa os pins; ao chegar a zero, avisa a política da região, ou devolve a moldura anônima ao subpool.
- **release(res)** (privado) — Devolve as molduras do subpool à área de trabalho. Não lança.

#### ReplacementPolicy
Escolhe a vítima dentro de uma região.

Tipo: interface; implementações Lru e Mru, uma instância por região.

```cpp
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;
    virtual void on_access(FrameId) = 0;
    virtual void on_unpin(FrameId, Intent) = 0;
    virtual void on_remove(FrameId) = 0;
    virtual std::optional<FrameId> pick_victim() = 0;
};
```

- **on_access(moldura)** — A página foi fixada e sai das candidatas.
- **on_unpin(moldura, intent)** — Os pins chegaram a zero e a moldura volta às candidatas. Com intent.reuse falso e toss-immediate ligado, vira a próxima vítima.
- **on_remove(moldura)** — A moldura foi esvaziada fora da política e deixa de ser candidata.
- **pick_victim()** — Escolhe entre as candidatas: a usada há mais tempo no LRU, a mais recente no MRU.

#### PageGuard
Um pin numa página: dá acesso aos bytes da moldura e solta o pin quando é destruído.

Tipo: handle RAII, só move. Só o BufferPool cria guards.

```cpp
#if defined(__clang__)
#  define LIFETIMEBOUND [[clang::lifetimebound]]
#else
#  define LIFETIMEBOUND
#endif

class PageGuard {
public:
    PageGuard(PageGuard&&) noexcept;
    PageGuard& operator=(PageGuard&&) noexcept;
    PageGuard(const PageGuard&) = delete;
    ~PageGuard();
    std::span<const std::byte> read() const LIFETIMEBOUND;
    std::span<std::byte>       write() LIFETIMEBOUND;
    BlockId id() const;
    bool    anonymous() const;
private:
    friend class BufferPool;
    PageGuard(BufferPool*, FrameId, Intent);
    BufferPool* pool_ = nullptr;
    FrameId     frame_{};
    Intent      intent_{};
};
```

- **read()** — Span só de leitura. Não soma nada.
- **write()** — Span mutável. Marca a página como suja, exceto nas molduras anônimas.
- **id()** — BlockId da página; inválido numa moldura anônima.
- **anonymous()** — Diz se a moldura é de memória de trabalho.
- **destrutor** — Solta o pin. Um guard movido não faz nada.

#### Reservation
O subpool de um operador: m molduras exclusivas, com política própria.

Tipo: handle RAII, só move, opaco para a camada 2 (135). Quem precisa da reserva depois guarda o id.

```cpp
class Reservation {
public:
    Reservation(Reservation&&) noexcept;
    Reservation& operator=(Reservation&&) noexcept;
    Reservation(const Reservation&) = delete;
    ~Reservation();                        // noexcept
    ReservationId id() const;
    std::size_t   capacity() const;
    std::size_t   pinned() const;
private:
    friend class BufferPool;
    Reservation(BufferPool*, ReservationId);
    BufferPool*   pool_ = nullptr;
    ReservationId id_ = 0;
};
```

- **id()** — Id estável da reserva.
- **capacity() e pinned()** — Tamanho do subpool (m) e molduras fixadas agora.
- **destrutor** — Chama release e devolve o subpool à área de trabalho. Não lança; uma página ainda fixada encerra o programa com diagnóstico.

#### FileManager
Converte o BlockId em deslocamento no arquivo do SO, lê e grava blocos com pread/pwrite, mantém o cabeçalho com o bitmap e é o único lugar que conta leituras e gravações físicas.

Tipo: serviço, membro privado do BufferPool. Cada arquivo lógico é um arquivo do SO; o bloco 0 é o cabeçalho, e o extent só define o crescimento. Exige POSIX (Linux ou WSL2).

```cpp
class FileManager {
public:
    explicit FileManager(const Config&);
    FileId        create(const std::string& name, FileKind);   // grava o cabeçalho (exceto temp)
    FileId        open(const std::string& name, FileKind);     // lê e confere o cabeçalho
    std::uint32_t allocate_block(FileId);                     // nunca devolve 0
    void          free_block(FileId, std::uint32_t);
    void          remove(FileId) noexcept;
    void read_block (BlockId, std::span<std::byte> out);
    void write_block(BlockId, std::span<const std::byte> in);
    std::uint64_t flush_headers();                            // grava cabeçalhos sujos
    std::uint32_t header_blocks(FileId) const;
    void add_counts(Counters&) const;
private:
    struct OpenFile {
        UniqueFd              fd;
        std::filesystem::path path;
        FileKind              kind;
        std::uint32_t         blocks = 0;          // inclui o cabeçalho
        std::vector<bool>     free_map;            // espelho do bitmap do bloco 0
        bool                  header_dirty = false;
    };
    off_t offset_of(std::uint32_t block) const {
        return static_cast<off_t>(block) * static_cast<off_t>(block_size_);
    }
    void grow(OpenFile&);
    std::uint32_t block_size_;
    std::uint32_t extent_blocks_;
    std::vector<OpenFile> files_;
    struct { FileId file; std::uint32_t block; bool valid = false; } last_;
    Counters io_;
};
```

- **create(nome, tipo)** — Cria o arquivo em data_dir com create_excl. Se não for temporário, inicializa o bloco 0 (magic, versão, tamanho do bloco e do extent, bitmap vazio) e o grava, contando a gravação (161).
- **open(nome, tipo)** — Abre um arquivo existente com open_existing, lê o cabeçalho (bitmap), contando a leitura, e confere magic, versão e tamanho do bloco contra o Config; divergência lança (161).
- **allocate_block(arquivo)** — Procura um bloco livre no bitmap, a partir do 1; se não houver, chama grow. Marca o cabeçalho como sujo, exceto em temporários.
- **free_block(arquivo, bloco)** — Marca o bloco como livre e o cabeçalho como sujo.
- **remove(arquivo)** — Fecha o descritor e apaga o arquivo. Não lança.
- **read_block(BlockId, out)** — Lê com pread_full e soma uma leitura física, sequencial se for no mesmo arquivo e no bloco seguinte ao último acessado em todo o disco simulado.
- **write_block(BlockId, in)** — Grava com pwrite_full e soma uma gravação física, com a mesma classificação.
- **flush_headers()** — Grava os cabeçalhos sujos e devolve quantos gravou. Chamado por flush_all.
- **header_blocks(arquivo)** — Número de blocos de cabeçalho: 1, que cobre até 32.576 blocos com bloco de 4 KiB (154).
- **add_counts(Counters&)** — Acrescenta os contadores físicos ao snapshot.
- **offset_of(bloco)** (privado) — Bloco × tamanho do bloco, em 64 bits antes de multiplicar.
- **grow(arquivo)** (privado) — Aumenta o arquivo em um extent com ftruncate, sem gravar.

#### UniqueFd e funções POSIX
O dono de um descritor de arquivo e as chamadas POSIX encapsuladas.

Tipo: handle RAII, só move; membro de FileManager::OpenFile.

```cpp
static_assert(sizeof(off_t) == 8, "off_t precisa ter 64 bits");

class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    UniqueFd(UniqueFd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    UniqueFd& operator=(UniqueFd&& o) noexcept {
        if (this != &o) { reset(); fd_ = std::exchange(o.fd_, -1); }
        return *this;
    }
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    ~UniqueFd() { reset(); }
    int  get() const noexcept { return fd_; }
    void reset() noexcept { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } }
private:
    int fd_ = -1;
};

[[noreturn]] void throw_errno(const char* what);
UniqueFd create_excl(const std::filesystem::path&);     // O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC (161)
UniqueFd open_existing(const std::filesystem::path&);   // O_RDWR | O_CLOEXEC, sem O_CREAT (161)
void pread_full (int fd, std::span<std::byte> out, off_t offset);
void pwrite_full(int fd, std::span<const std::byte> in, off_t offset);
```

- **UniqueFd::get()** — O descritor, sem transferir a posse.
- **UniqueFd::reset()** — Fecha o descritor, se houver, e deixa o objeto vazio.
- **destrutor** — Chama reset(); o descritor é fechado exatamente uma vez.
- **create_excl(path) e open_existing(path)** — Criam o arquivo, falhando se ele existir, ou abrem um existente, falhando se ele não existir (161).
- **pread_full e pwrite_full** — Transferem exatamente o tamanho pedido, repetindo em transferência curta e em EINTR. Retorno 0 lança: no pread é fim de arquivo, e repetir travaria o laço (161).
- **throw_errno(what)** — Lança std::system_error com o errno atual.

### Valores em common/

#### BlockId
Endereço de um bloco: qual arquivo e qual bloco dentro dele.

Tipo: valor; struct trivial, comparável e com hash.

```cpp
struct BlockId {
    FileId        file;
    std::uint32_t block;   // posição no arquivo do SO; 0 é o cabeçalho
    auto operator<=>(const BlockId&) const = default;
};
struct BlockIdHash {
    std::size_t operator()(BlockId b) const noexcept {
        return (std::uint64_t{b.file} << 32) | b.block;
    }
};
```

#### RID
Identifica um registro no arquivo da tabela e é gravado nas posting lists.

Tipo: valor; layout fixo de 8 B. Estável enquanto o registro fica na mesma página; quando ele muda de página, os índices são atualizados (141, 169).

```cpp
struct RID {
    std::uint32_t block;
    std::uint16_t slot;
    std::uint16_t reserved = 0;
    auto operator<=>(const RID&) const = default;
};
static_assert(sizeof(RID) == 8 && std::is_trivially_copyable_v<RID>);
static_assert(std::endian::native == std::endian::little);   // formatos em disco são os do host (167)
inline constexpr RID kInvalidRid{0, 0};                       // o bloco 0 é sempre cabeçalho (167)
```

#### Counters
Fotografia dos contadores. A diferença entre duas é a medida de uma operação.

Tipo: valor; struct copiável com subtração.

```cpp
struct IoCounts {
    std::uint64_t requests = 0, hits = 0, misses = 0;   // requests = hits + misses
    std::uint64_t allocations = 0;
    std::uint64_t reads_seq = 0, reads_rand = 0;
    std::uint64_t writes_seq = 0, writes_rand = 0;
    std::uint64_t dirty_evictions = 0, forced_writes = 0;
    std::uint64_t data_area_requests = 0;   // pedidos sem reserva a arquivos Data; 0 nas operações medidas (159)
    IoCounts& operator-=(const IoCounts&);
};
struct Counters {
    IoCounts global;
    std::array<IoCounts, 4> per_kind;                   // indexado por FileKind
    std::unordered_map<FileId, IoCounts> per_file;
    friend Counters operator-(Counters a, const Counters& b);
};
```

- **operator-** — Subtrai dois snapshots campo a campo.

#### Config
Os parâmetros de uma configuração do SGBD, passados de cima para baixo.

Tipo: valor; agregado com o baseline como padrão.

```cpp
struct Config {
    std::uint32_t block_size    = 4096;
    std::uint32_t extent_blocks = 8;
    std::array<std::uint32_t, 4> area_frames{32, 16, 4, 32};   // Data, Index, Catalog, Work
                                                               // Work = maior m possível
    Policy policy         = Policy::Lru;   // áreas e subpools
    bool   toss_immediate = false;
    bool   prefetch       = false;
    std::filesystem::path data_dir;        // ext4 (Linux/WSL2), fora do OneDrive
    std::uint32_t frames() const;
};
// variação OFAT:  Config mru{.policy = Policy::Mru};
```

#### Stats
Números reais da tabela e de cada estrutura, medidos depois da construção.

Tipo: valor; dois structs.

```cpp
struct StructureStats {
    StructureId   id;
    StructureKind kind;
    std::uint32_t blocks;
    std::optional<std::uint16_t> height;                     // B+
    std::optional<std::uint32_t> internal_blocks;            // B+: nós internos
    std::optional<std::uint32_t> leaves, distinct;           // B+
    std::optional<std::uint32_t> buckets, overflow_blocks;   // hash
    std::optional<double>        load_factor, chain_avg;     // hash
    std::optional<std::uint32_t> chain_max;                  // hash
};
struct TableStats {
    std::uint64_t n;
    std::uint32_t r, bfr;
    std::uint32_t b;     // heap: todos; B+: folhas; hash: buckets + overflow (119)
    std::vector<StructureStats> structures;
};
```

#### AccessPath
Descreve um caminho de acesso: tabela, estrutura, campo e o que find ou probe respondem.

Tipo: valor.

```cpp
enum class PathKind : std::uint8_t {
    Scan, PrimaryBTree, PrimaryHash,
    SecondaryHash, SecondaryBTreeClustering, SecondaryBTreeNonClustering
};
struct HashPartitioning { std::uint32_t function_id; std::uint32_t buckets; };

struct AccessPath {
    TableId     table;
    StructureId id;
    PathKind    kind;
    FieldId     field;                              // sem significado no Scan
    bool        unique;                             // = schema.field(field).unique; falso no Scan (157)
    bool        supports_point, supports_range;     // o que find/probe respondem; o Scan sempre executa
    std::optional<FieldId>          ordered_by;     // só PrimaryBTree e SecondaryBTreeClustering
    std::optional<HashPartitioning> hash_partitioned_by;   // só PrimaryHash
};
```

#### OpResult
Tudo o que a camada 1 recebe de uma operação.

Tipo: valor.

```cpp
struct Cost  { double transfers; double seeks; };
struct Phase { std::string name; Counters delta; };

struct OpResult {
    std::uint64_t      rows;
    std::uint64_t      checksum;
    Counters           delta;          // snapshot final − inicial, antes do flush de fim
    std::uint64_t      flush_writes;   // verificação: deve ser 0
    Cost               estimate;
    std::vector<Phase> phases;
    std::chrono::nanoseconds wall_time;
};
struct NotApplicable {                     // (128, 170)
    enum class Kind : std::uint8_t { NaoAplicavel, Inviavel };
    std::string reason;
    Kind kind = Kind::NaoAplicavel;
    NotApplicable(std::string r, Kind k = Kind::NaoAplicavel) : reason(std::move(r)), kind(k) {}
};
using Outcome = std::variant<OpResult, NotApplicable>;
```

#### Intent
O que o chamador sabe sobre o acesso.

Tipo: valor; duas flags independentes.

```cpp
struct Intent {
    bool reuse      = true;    // false: com toss-immediate, a moldura vira a próxima vítima
    bool sequential = false;   // true: com prefetch, lê adiante no mesmo extent
};
inline constexpr Intent kRandomReuse{true,  false};   // nós de índice
inline constexpr Intent kRandomOnce {false, false};   // fetch por RID
inline constexpr Intent kSeqReuse   {true,  true};    // interna do BNL
inline constexpr Intent kSeqOnce    {false, true};    // externa do BNL, runs, partições
```

#### Occupancy
Blocos de cada parte de uma estrutura (§6.1.1).

Tipo: valor.

```cpp
struct Occupancy {
    std::uint32_t data = 0, overflow = 0, header = 0, directory = 0;
    std::vector<std::uint32_t> index_levels;   // da raiz para baixo
};
```

### Tipos auxiliares

```cpp
using FileId        = std::uint32_t;
using FieldId       = std::uint16_t;
using FrameId       = std::uint32_t;
using TableId       = std::uint32_t;
using StructureId   = std::uint32_t;
using ReservationId = std::uint32_t;   // 0 = sem reserva
inline constexpr ReservationId kNoReservation = 0;   // passado sempre por escrito (159)

enum class FileKind      : std::uint8_t { Data, Index, Catalog, Temp };
enum class Area          : std::uint8_t { Data, Index, Catalog, Work };
enum class Policy        : std::uint8_t { Lru, Mru };
enum class StructureKind : std::uint8_t { Heap, BTree, Hash, SecHash, SecBTree };
enum class JoinAlgorithm : std::uint8_t { Bnl, SortMerge, Hash };
enum class FieldType     : std::uint8_t { Int32, Int64, Char, VarChar };   // VarChar (158)

using Key = std::variant<std::int32_t, std::int64_t, std::string>;   // CR em int32 (× 100)
struct Point    { FieldId field; Key key; };
struct KeyRange { FieldId field; std::optional<Key> lo, hi;
                  bool lo_incl = true, hi_incl = true; };
using Predicate = std::variant<Point, KeyRange>;

struct FieldDef {
    std::string   name;
    FieldType     type;
    std::uint16_t offset, size;          // VarChar: offset sem uso, size = tamanho máximo
    bool          unique   = false;      // (157)
    bool          nullable = false;      // (157, 158)
};
struct Schema {
    std::vector<FieldDef> fields;
    std::uint16_t record_size;           // só campos fixos: exato; com VarChar: máximo
    std::uint16_t null_bytes;            // ⌈anuláveis / 8⌉ (25); os offsets fixos já o descontam
    std::uint16_t var_fields;            // nº de VarChar: um u16 de fim por campo, após os fixos (158)
    const FieldDef& field(FieldId f) const { return fields.at(f); }
};

struct PrimarySpec { StructureKind kind; FieldId key;    // a chave é ignorada no Heap
                     double fill = 0.7;
                     std::uint32_t buckets = 0;          // hash: calculado pela camada 1 (39, 163)
                     std::uint32_t hash_function = 0; };
struct IndexSpec   { StructureKind kind; FieldId key; bool clustering = false; };

class RecordStream {
public:
    virtual ~RecordStream() = default;
    virtual std::optional<std::span<const std::byte>> next() = 0;
};
```

---

## 9. Limites a declarar no relatório

- A seleção pontual sem índice faz uma varredura por chave do lote. Em campo UNIQUE, ela para no primeiro registro encontrado: b/2 em média (146).
- Cada arquivo tem no máximo 32.576 blocos, cerca de 127 MiB (154). Com os volumes atuais, Inscrição chegaria a esse limite perto da escala 19.
- O grupo de duplicatas do sort-merge tem de caber em group_pages; se não couber, a célula é inviável (151).
- O hash join faz até 6 passes de particionamento. Depois disso, ou quando uma partição não diminui (grupo de chaves iguais maior que m − 1 páginas), ele usa o bailout em blocos, cujo custo é o de um BNL dentro da partição (171).
- Nós internos da B+, páginas de índice e catálogo ficam fora de M, nas próprias áreas.
- Com prefetch ligado, a leitura antecipada expulsa páginas não fixadas do subpool.
- Página pedida pela reserva que esteja em outra região migra para o subpool e conta como acerto.
- Os diretórios do ScratchSpace são estruturas da linguagem, limitadas a 8 a 16 B por item que cabe nas molduras reservadas (posição, cabeças e cadeia do hash).
- Temporários são gravados e retirados do pool no seal, então a releitura é sempre física.
- A área de dados só é usada fora das operações medidas.
- O flush antes de cada repetição e o flush de fim devem gravar zero; são verificações, não medidas.
- Campo unique que nenhuma estrutura indexa não tem a unicidade verificada na inserção; o gerador a garante (157).
- O heap mantém os blocos esvaziados até o reorganize, e a ocupação depois da manutenção os inclui (162).
- No hash NOT UNIQUE, valores diferentes podem cair no mesmo bucket e dividir a cadeia (163).
- Depois da carga de manutenção, nenhuma operação medida roda sem reorganize, porque as estatísticas ficam desatualizadas (160).

### Células não aplicáveis (decisão 170)

As quatro situações abaixo aparecem no resultados.csv com status `nao_aplicavel` e o número do caso no motivo. Os casos 1, 2 e 3 são contradições do enunciado e estão em aberto na §12. O caso 4 é uma impossibilidade técnica.

**Caso 1 — Junções dos tipos 2 e 3 nas configurações b, c e d.** São 18 das 45 células, ou 54 execuções por política.
- **Contradição:** o §5.1 define o tipo 2 como equijunção em campos UNIQUE "que não ditam a organização primária", e o tipo 3 do mesmo jeito, com campos NOT UNIQUE. Já as configurações b e d exigem que um dos campos seja o da organização primária, e a c exige que os dois sejam. Para esses tipos, só existem as configurações a e e.
- **Leitura alternativa:** "não ditam a organização" significaria "não é a chave primária lógica". Nesse caso, as células existiriam com tabelas organizadas pelo campo da junção, por exemplo Detalhes por CPF.
- **Tratamento atual:** a bancada declara as 18 células, com os 3 algoritmos, como `nao_aplicavel`. A célula tipo 2 × d que rodava antes (Detalhes em hash por CPF) saiu da execução.

**Caso 2 — Faixa em campo UNIQUE pelo índice secundário.** São 2 células: 0,1% e 50%.
- **Contradição:** o §5.2 pede a configuração c (índice secundário) para faixa em UNIQUE. Mas o §4.2 diz que o secundário de campo UNIQUE no heap "deve ser um Hash", e que a B+ só admite secundário clustering "somente para campo NOT UNIQUE". Hash não responde a faixa.
- **Ressalva:** no hash primário, o §4.2 cita só o secundário clustering NOT UNIQUE, sem dizer "somente". Se a lista não for exaustiva, uma B+ secundária sobre campo UNIQUE num arquivo hash faria a célula existir.
- **Tratamento atual:** `nao_aplicavel` em todas as organizações.

**Caso 3 — Igualdade em campo UNIQUE com 0,1% e 50%.** São 6 células: configurações a, b e c × 2 seletividades.
- **Contradição:** o §5.2 pede as duas seletividades para cada tipo de seleção, mas uma igualdade em UNIQUE devolve uma única linha. A seletividade é s = 1/N, ou 0,005% com N = 20.000.
- **Tratamento atual:** a igualdade é medida num nível próprio, "pontual". É assim que o §6.4 do enunciado a apresenta, separada das seletividades, e assim que o benchmark Wisconsin (Bitton, DeWitt e Turbyfill, 1983) mede a busca de uma tupla, à parte dos percentuais.
- **Leitura do lote:** as 1.000 chaves são 1.000 consultas de um registro. O custo por consulta é o total dividido por 1.000, já com o cache de índice aquecido.
- As 6 células de 0,1% e 50% saem como `nao_aplicavel`, e as medidas ficam nas linhas com seletividade "pontual".

**Caso 4 — Faixa pela organização primária quando ela é hash** (UNIQUE e NOT UNIQUE).
- **Não é contradição:** o hash não preserva a ordem das chaves e não responde a faixa. A célula "faixa × configuração b" existe pela B+; só a variante hash não roda.
- **Tratamento atual:** `nao_aplicavel` vindo do plano (decisão 128). É um resultado a relatar.

**Diferente dos quatro casos, o status `inviavel`** marca células que existem mas não cabem em M no nosso desenho:
- nº de partições pedido explicitamente (JoinParams.partitions) acima de m − 1;
- grupo de duplicatas maior que group_pages no sort-merge;
- m pequeno demais para o algoritmo (151).

### Decisões que a bancada não exercita

Estas decisões continuam registradas, porque o enunciado pede que o grupo as tome. Mas a bancada não as varia nem as mede, e nenhuma delas muda os resultados:
- **12 e 16 (toss-immediate e prefetch):** ficam desligados em todas as configurações. O prefetch do protótipo lê pelo número do bloco, e não pela ordem do percurso: lê blocos livres do extent e folhas fora de ordem. Antes de usá-lo como fator, ele teria de seguir o percurso.
- **22 e 24 (multivalorados e campo derivado):** as tabelas externas de e-mails e telefones e o campo Curso.carga_horaria_total não fazem parte das tabelas de medição.
- **31 (compactação a 30%):** a carga de manutenção remove 1% dos registros e não chega ao critério.
- **38 (carga a 100%):** todas as B+ usam fill 0,7.
- **44 (cardinalidade mínima do hash):** a base gerada tem cardinalidades fixas, escolhidas para atender ao critério.
- **45 (α depois da manutenção):** depois da manutenção, a ocupação em blocos, com o overflow, basta para o §6.1.1.

## 10. Testes que verificam as correções

- **Subpool:** com m = 8, nenhuma operação fixa mais de 8 molduras, e fixar a nona lança exceção.
- **Sem cache oculto:** o BNL com m = 16 dá as mesmas leituras com área de trabalho 16 e 32.
- **Orçamento:** para cada plano gerado nos testes, frames_used() ≤ m, e o pico de molduras fixadas medido na execução coincide com frames_used(). *(Ainda não implementado no protótipo.)*
- **Nós internos fora de M:** select_points com 1.000 chaves na B+ primária lê a raiz do disco uma única vez. *(Ainda não implementado no protótipo.)*
- **Scan:** select_range sem índice executa e devolve as mesmas linhas que o SQLite.
- **seal:** uma run de k blocos gera k gravações temp e, no merge, k leituras físicas.
- **Conferência:** compare.py não encontra divergência em nenhuma operação e repetição, inclusive com papéis trocados.
- **RIDs estáveis:** depois de inserções que dividem folhas e de um reorganize, toda busca pelos índices encontra o registro certo, conferido contra um std::map.
- **Catálogo:** invalidate_all com o catálogo fixado não lança exceção.
- **Flushes:** o flush antes de cada repetição e o de fim gravam zero.
- **Sementes:** com partições e buckets potências de 2, as cadeias da tabela de hash ficam equilibradas. *(Ainda não implementado no protótipo.)*
- **allocate:** requisições = acertos + faltas em toda operação.
- **Reserva liberada:** fetch com o id de uma reserva já liberada lança logic_error.
- **Unicidade (157):** B+ e hash primários, em campo UNIQUE e NOT UNIQUE: a duplicata é rejeitada só no UNIQUE, e o find por igualdade em campo unique para no primeiro registro encontrado.
- **Registro (158):** registros fixos e com VarChar de tamanhos diferentes, com e sem NULL, lidos antes e depois de remoções e compactação na mesma página; get com tipo de tamanho errado e offset de VarChar corrompido lançam sem ler fora do registro; o checksum com NULL coincide com o do reference.py.
- **Caminho sem reserva (159):** data_area_requests = 0 em toda operação medida; pedir uma página de dados sem reserva no meio de uma operação faz o Monitor lançar logic_error.
- **Plano sem E/S (160):** dois snapshots em volta de plan_join e plan_select dão diferença zero; stats() depois de um insert lança logic_error.
- **Intervalo de medição (149):** sujar N páginas depois do snapshot final faz flush_writes = N e não muda o delta. *(Ainda não implementado no protótipo.)*
- **Criação e abertura (161):** open de arquivo inexistente e create de arquivo existente lançam; open com block_size diferente lança; um cabeçalho truncado faz o pread_full lançar em vez de travar.
- **Heap (162):** esvaziar um bloco do meio do heap e varrer não devolve registro apagado; a inserção seguinte vai para o fim; depois do reorganize, o arquivo não tem blocos vazios.
- **Partições (164):** duas tabelas hash pelo campo da junção, com nº de buckets diferente: o plano aproveita só a maior, e o checksum é igual ao do BNL.
- **Esquema (165):** TempFile criado com um esquema temporário, movido e depois varrido, sob ASan, sem erro.
- **free_block (168):** liberar um bloco não fixado e realocá-lo devolve página zerada; liberar um bloco fixado lança logic_error; o bitmap confere depois de fechar e reabrir.
- **Tuplas (166):** com dump ligado, o diff das tuplas contra o SQLite é vazio na base pequena.
- **Tabela menor (108):** com b ≤ m − 2, o BNL lê b_externa + b_interna, e o hash join não particiona nem grava.
- **Hash recursivo (171):** com m = 3, 4 e 5, o hash join dá o mesmo checksum do BNL, inclusive com as entradas pré-particionadas pelo arquivo hash. Com dois valores de 600 linhas cada e m = 4, o bailout é usado e o resultado confere com o BNL.

---

## 11. Acoplamentos e erros a não repetir

Resumo da análise de 06/11 (CONCLUSOES_SESSAO_2026-11-06.md), atualizado com o que foi decidido depois.

### Erros aritméticos já corrigidos
Todos superestimavam a capacidade, por ignorar cabeçalhos:
- bfr de Aluno: 31, não 32 (30 se a entrada de slot tiver 4 B). b de Aluno (N = 20.000): 646, ou 667 com bfr 30.
- bfr de Inscrição (R = 64): 63, não 64.
- B+: com cabeçalho de 24 B e ponteiro de 4 B, 509 chaves e fan-out de 510 (33). O valor 340, que estava aqui, supunha ponteiro de 8 B e cabeçalho de 16 B.
- Hash de Aluno por DRE (α = 0,8): 807 buckets, ou 809 se primo; não 787 nem 806.
- Null bitmap: ⌈n_anuláveis / 8⌉ bytes, não 1 byte fixo.
- Cadeia de um hash em sexo: cerca de 323 blocos (metade de 646), não 313.

### Acoplamentos que esvaziam um fator experimental
- Se o chunk do BNL ocupar todo M menos uma moldura, LRU e MRU dão números idênticos. Por isso o chunk padrão é ⌊m/2⌋.
- Toss-immediate ligado no baseline faz LRU e MRU convergirem; fica desligado e vira fator secundário.
- Uma prioridade que proteja índice e catálogo remove o único lugar onde o LRU se destaca. Não há prioridade: as áreas são separadas, e a política continua atuando dentro de cada uma.
- LRU, FIFO e Clock se comportam quase igual em varredura sequencial. O par LRU × MRU é o único em que cada uma vence numa carga diferente.
- "Tabela pequena" precisa de uma memória com tamanho declarado: é a que tem b ≤ M − 2 (108). Com b ≤ M apenas, uma tabela "menor" ainda podia exigir duas passadas no BNL e partições no hash.
- A seleção pontual custa cerca de 3 blocos, e variação de 5% sobre 3 blocos não é mensurável. Por isso ela roda em lotes de cerca de 1.000 chaves, o que também cria reuso de páginas de índice.

### Acoplamentos que invertem conclusões
- O tamanho do bloco muda b, bfr, fan-out e M ao mesmo tempo; é um parâmetro fixo, não um fator. Se um dia for comparado, fixar os bytes totais do pool, nunca o número de molduras.
- O bulk load decide a sequencialidade física das folhas da B+; inserção registro a registro fragmenta.
- Inserção sempre no fim do heap preserva a ordem física; lista de livres a embaralharia.
- Prefetch ligado no baseline favorece sistematicamente a varredura.
- O índice secundário não-clustering perde da varredura com seletividade de 50%. Esse é um resultado central, e a busca dos RIDs na ordem das posting lists (117) o preserva.
- O arquivo hash é maior que o heap (buckets mais overflow), então uma varredura nele é mais cara.
- O hash NOT UNIQUE dimensionado pela fórmula do UNIQUE tem cerca do dobro dos blocos do heap; a varredura nele pareceria pior por causa da configuração, não da organização (163).

### Viabilidade das estruturas
- A cardinalidade do campo decide se hash NOT UNIQUE e posting lists são viáveis (decisão 44). Exemplo: professor_orientador, com 500 valores e 40 RIDs por valor, cabe; sexo não.
- Volumes das tabelas e M precisam ser dimensionados juntos, para que b/M atravesse regimes diferentes.
- As seletividades de 0,1% e 50% são propriedades dos dados: as constantes saem dos quantis da base gerada (decisão 130).

### Semântica do índice clustering
Índice esparso, com uma entrada por valor distinto apontando para o início do grupo, e permitido só no campo que dita a organização primária. Sobre a B+ por curso, uma faixa lê blocos sequenciais. Sobre o hash por curso, lê o mesmo número de blocos, todos aleatórios. Esse contraste é um dos melhores resultados da bancada.

### Os cinco builds por tabela
O caminho de acesso é parâmetro explícito da primitiva, então os índices são construídos uma vez por build e o caminho "sem índice" simplesmente não os usa.
1. Heap, com hash(CPF) e B+ não-clustering em professor_orientador e em CR.
2. B+ primária em DRE.
3. B+ primária em curso, com clustering B+(curso).
4. Hash primário em DRE.
5. Hash primário em curso, com clustering B+(curso).

A ocupação é reportada por estrutura, porque inclui índices que uma dada configuração não usa.

---

## 12. Questões abertas

- **Registros variáveis e blocagem estendida são obrigatórios?** Perguntar ao professor. A proposta atual mantém as tabelas de medição com registros fixos.
- **Biblioteca de B+ é admitida?** Proposta: implementação paginada própria como motor medido; uma biblioteca ou std::map como oráculo nos testes.
- **§5.2 diz "dois tipos de seleção" e lista três.** Assumir os três.
- **A lista de métodos de acesso do §4.2 é exaustiva?** A resposta decide o caso 2 no hash primário (§9, decisão 170). Hoje a faixa em UNIQUE pelo índice secundário é `nao_aplicavel` em todas as organizações.
- **Junções 2 e 3 × configurações b a d (caso 1, §9).** Essas junções são definidas sobre campos "que não ditam a organização primária", mas as configurações b, c e d exigem que um ou os dois campos sejam o da organização.
  - **Tratamento atual (170):** as células são `nao_aplicavel`, e a matriz cai de 45 para 27 células (de 135 para 81 execuções por política).
  - **Se o professor adotar a outra leitura:** basta criar tabelas organizadas pelo campo da junção, como Detalhes_do_Aluno por CPF.
  - Perguntar ao professor.
- **Seletividade da igualdade em UNIQUE (caso 3, §9).** Adotado (170): a igualdade é um nível "pontual" à parte, e 0,1% e 50% valem como "cerca de" para as faixas. Confirmar com o professor.
- **Volumes e M (resolvido no protótipo):** entrada de slot de 4 B. Volumes: Aluno 20.000, Inscrição 100.000, Turma 6.000, Disciplina 800, Curso 200 e Detalhes 20.000. M = 32 nas junções e 2 nas seleções. Revisar se o professor pedir outra escala.
- **Versão do enunciado e cortes.** O PDF não tem as seções 3.3, 4.1 e 6.3, e o §6.1.1 cita uma "matriz do grid file" que não aparece em mais nenhum lugar. Isso sugere seções cortadas. Confirmar se há versão posterior a 02/11 e se houve cortes comentados em aula.
- **Prazo:** 22/11 no PDF, 29/11 segundo o grupo.
- **macOS:** o protótipo foi desenvolvido e testado no macOS (Apple clang). O CI continua previsto para Linux, onde o build com CMake ainda não foi testado.

---

## 13. Pendências e próximos passos

- Preencher a coluna "valor decidido" das decisões 1 a 45 e atualizar, no documento do grupo, o texto das decisões 100 a 133 para o da seção 5. As decisões 157 a 171 e as emendas de 08/11 também precisam entrar lá.
- Enviar ao professor as perguntas da seção 12.
- Definir volumes e cardinalidades de cada tabela e, a partir deles, o M do baseline.
- Escrever a especificação TOML com o esquema físico, os builds e as operações (com o SQL equivalente).
- Preparar o ambiente: WSL2 para quem usa Windows, CMake com a verificação de POSIX, CI em Ubuntu.
- Implementar de baixo para cima, com os testes da seção 10 junto de cada parte:
  1. UniqueFd e FileManager;
  2. BufferPool, ReplacementPolicy, PageGuard e Reservation;
  3. organizações primárias e índices;
  4. cursores, RecordView, TempFile e ScratchSpace;
  5. Monitor, planos, algoritmos e estimativas;
  6. roteiro da camada 1;
  7. scripts de conferência.
- Registrar desde o início os prompts e o código bruto devolvido pela IA, e manter o POST_MORTEM.md (§7.2.3).
- Nenhum trecho de código foi compilado até agora.

---

## 14. O que não rediscutir

Linhas de raciocínio já percorridas e encerradas:
- **Taxonomia "fatores a variar × combinações de teste".** Foi um erro. Base de dados e carga de trabalho são o domínio, enumerado por completo; a configuração é a incógnita, explorada por OFAT.
- **Comparação intra-bloco vetorizada.** Era contorno para Python; em C++ é irrelevante.
- **"Reduzir M em vez de aumentar N".** Só o argumento de desenho sobrevive: dimensionar M a partir dos volumes, para atravessar regimes.
- **Par de políticas:** LRU × MRU.
- **E/S:** pread/pwrite (POSIX), e não std::fstream, por decisão do grupo em 06/11.
- **Memória do operador:** subpool exclusivo, e não pool compartilhado com cota.
- **B+ primária em dois arquivos,** e não flag de classe de página no Intent.
- **Memória de trabalho em molduras (ScratchSpace),** e não estruturas da linguagem.
- **Conferência com o SQLite fora do motor,** com lotes e constantes em arquivo.
- **Índices fora de M** (145): a revisão de 06/11 propôs uma cota única por operador, que incluísse as páginas de índice. Fica como está; o que a revisão tinha de válido virou a 159.
- **Itens da revisão de 06/11 já resolvidos:** liberação de página (free_block), manutenção de RIDs (MoveListener), sequência de medição (149), ciclo de vida do TempFile (118, 139, 155) e catálogo fixado × invalidate_all (144).

# Decisões da bancada por categoria e importância

Estado em 08/11/2026. Classifica as 117 decisões do `HANDOFF_ARQUITETURA_2026-11-08.md` (1 a 45 e 100 a 171), já com as emendas de 08/11. O número da decisão vem no início do título. O texto completo e o motivo de cada uma estão no handoff.

**Critério das categorias**
- **Arquitetura:** define as camadas e os contratos entre elas, o modelo de memória e de medição, ou uma escolha que muda os números medidos ou as conclusões do relatório.
- **Implementação:** diz como um componente cumpre o seu contrato, sem mudar o que é medido. Inclui formatos internos, tratamento de erros, ciclo de vida e ferramentas.

**Escala de importância**
- **Crítica:** sem ela, a medida não vale, ou uma exigência central do enunciado deixa de ser cumprida.
- **Alta:** muda números medidos ou conclusões, ou é necessária para que os resultados sejam corretos.
- **Média:** afeta robustez, detalhe de medida ou desempenho de uma estrutura, sem mudar as conclusões principais.
- **Baixa:** detalhe, documentação, ou decisão registrada que a bancada não exercita.

Total: 71 decisões de arquitetura (11 críticas, 31 altas, 22 médias e 7 baixas) e 46 de implementação (8 altas, 24 médias e 14 baixas). Nenhuma decisão de implementação é crítica. Na tabela de implementação, as decisões 21 e 25 e as decisões 26 e 28 dividem uma linha, porque tratam do mesmo assunto.

---

## Decisões de arquitetura (71)

| Importância | Título | Explicação da decisão | O que foi decidido |
|---|---|---|---|
| Crítica | 100. Canal de configuração e medida | Como a configuração chega às camadas e como a medida volta, sem a bancada mexer no motor | A configuração desce por injeção (Config → Monitor → StorageManager → BufferPool); a medida sobe no OpResult; a camada 1 nunca lê contadores nem o catálogo diretamente |
| Crítica | 105. Acesso aos bytes só pelo buffer | Exigência do enunciado: nenhuma camada acima do buffer pode ler blocos por fora, senão a contagem fica subestimada | A camada 3 recebe bytes só através do PageGuard; nenhum ponteiro sobrevive ao unpin |
| Crítica | 5. Contadores físicos | São a métrica central do trabalho: blocos transferidos | Contagem por arquivo, por tipo e global, separando sequencial e aleatória, num único lugar (o FileManager) |
| Crítica | 8. Repartição do pool | Como a memória é dividida entre os tipos de página e as operações | Por tipo de página nos acessos sem reserva; um subpool exclusivo por operador nos acessos com reserva |
| Crítica | 9. Áreas e tamanhos | Quanto de memória cada uso recebe e qual é o maior M | Dados 32, índices 16, catálogo 4 (fixado), trabalho 32. Os subpools saem da área de trabalho, então M ≤ 32. A área de dados só é usada fora das medidas |
| Crítica | 107. Reserva | Garante que o operador use exatamente M molduras, como pedem as fórmulas | reserve(n) cria um subpool exclusivo; a cota conta molduras, não pins; passar de n lança exceção |
| Crítica | 108. M e tabela menor | Define o fator "tamanho da tabela" (§5.1, §6.2) de modo coerente com os algoritmos | Tabela menor é a que tem b ≤ M − 2, e cada algoritmo a processa numa passada; a bancada calcula o tamanho pelas estatísticas (emendada) |
| Crítica | 110. Estado entre repetições | Cada repetição precisa começar do mesmo estado, senão as medidas não são comparáveis | Antes de cada repetição: flush_all (deve gravar zero) e invalidate_all (esvazia tudo menos o catálogo) |
| Crítica | 11. Políticas comparadas | O fator de buffer que o enunciado manda variar (§2.2, §6.2) | LRU × MRU, com uma instância da política por região (área ou subpool) |
| Crítica | 124. Algoritmo × caminho de acesso | Diz o que as configurações a a e significam para cada algoritmo de junção | O BNL não usa índices; o sort-merge aproveita a entrada ordenada (ordered_by); o hash join aproveita um arquivo hash como partições prontas |
| Crítica | 140. Memória de trabalho em molduras | Evita estruturas da linguagem maiores que a área reservada (§7.1) | ScratchSpace em molduras anônimas da reserva, nunca gravadas nem contadas; os diretórios internos são limitados pela capacidade |
| Alta | 102. Modelo de seek | Define o que conta como acesso aleatório | Cabeça de disco global: há seek quando muda o arquivo ou quando o bloco não é o anterior + 1, como nas fórmulas dos livros |
| Alta | 149. Ordem de medição | Evita que gravações de fim de operação entrem ou saiam da medida por engano | O snapshot final e o fechamento das fases vêm antes do flush de fim |
| Alta | 160. Estatísticas sem E/S | O plano é resolvido antes do snapshot inicial; se ele lesse estruturas, aqueceria o cache | Estatísticas medidas ao fim da carga e do reorganize e guardadas no catálogo; stats() não faz E/S |
| Alta | 109. Leitura dos contadores | Como se obtém a medida de uma operação | snapshot() devolve uma cópia imutável; a medida é a diferença entre dois snapshots; não há reset |
| Alta | 14. Gravação forçada e flushes | Separa o custo da carga do custo da operação | Páginas sujas são gravadas na expulsão; o flush depois da carga não é medido; os flushes de verificação devem gravar zero |
| Alta | 148. Orçamento de molduras | Garante que nenhum algoritmo use mais que M | Todas as parcelas de memória estão no plano, e frames_used() ≤ m é verificado antes de executar |
| Alta | 136. Subpool com política própria | Faz a política de substituição atuar dentro da memória do operador | Cada reserva tem a sua instância de LRU ou MRU |
| Alta | 145. B+ primária em dois arquivos | Separa nós internos (área de índices, fora de M) e folhas com dados (pela reserva) | Nós internos num arquivo do tipo Index; folhas com os registros no arquivo de dados |
| Alta | 1. Tamanho do bloco | Muda b, bfr, fan-out e M ao mesmo tempo | 4.096 B, fixo; não é fator do experimento |
| Alta | 6. Nº total de páginas | Tamanho do buffer pool | 84 molduras (336 KiB), a soma das áreas |
| Alta | 17. Métricas do buffer | O que o enunciado pede para registrar (§2.2) | Requisições, acertos, faltas, taxa de acerto, alocações, leituras e gravações sequenciais e aleatórias, gravações sujas na expulsão e gravações forçadas |
| Alta | 103. Pedido de página | Leva ao buffer a reserva e a intenção de uso de cada acesso | fetch(BlockId, Intent, reserva), com a reserva sempre explícita |
| Alta | 106. Área de cada página | Decide em que região do pool cada página vive | Sem reserva, a área vem do tipo do arquivo; com reserva, a página vai para o subpool |
| Alta | 119. Estatísticas e definição de b | b entra em todas as fórmulas e na classificação de tamanho | b = todos os blocos no heap, as folhas na B+, buckets mais overflow no hash; altura e nós internos à parte |
| Alta | 143. Plano resolvido | A execução e a estimativa precisam usar os mesmos parâmetros | O plano é calculado uma vez. Runs e partições usam blocos densos ⌈n/bfr⌉; as estimativas usam b para ler e esse valor para gravar e reler (emendada) |
| Alta | 122. Externa e interna | A escolha dos papéis muda o custo (§5.1.3) | BNL: a menor por fora. Hash: a menor como build. Sort-merge: o lado UNIQUE por dentro. Há uma variante com os papéis trocados (emendada) |
| Alta | 171. Hash join recursivo | Sem recursão, o hash join só valia para builds de até cerca de 800 blocos com m = 32 | Particionamento recursivo, com semente nova por passe, parada e bailout em blocos; estimativa pela fórmula do livro |
| Alta | 164. Condições para aproveitar ordem e partições | A organização sozinha não garante ordem nem partições compatíveis | O sort-merge só pula a ordenação se ordered_by for o campo da junção. Hash pré-particionado só no campo da junção; com duas entradas hash incompatíveis, aproveita-se a maior |
| Alta | 117. Saída do índice secundário | Preserva o resultado central de que o índice não clustering perde para a varredura a 50% | RIDs na ordem das posting lists (fórmula h + s); a variante que ordena os RIDs fica à parte |
| Alta | 125. Seleções em lote | A seleção pontual isolada (cerca de 3 blocos) não é mensurável com 5% de variação | Lote de cerca de 1.000 chaves, gravado antes da execução; seleção por faixa à parte |
| Alta | 130. Gerador e constantes | A seletividade precisa ser controlada a partir dos dados | Um CSV por tabela, com semente; as faixas saem dos quantis da base gerada |
| Alta | 132. Repetições | Metodologia do §6: repetir até a variação ficar abaixo de 5% | De 3 a 10 repetições; a tolerância vale para as contagens e para o tempo (emendada) |
| Alta | 147. Conferência com o SQLite | Garante que cada primitiva devolve o resultado certo (§5.2) | Lotes e constantes em arquivo; reference.py e compare.py conferem linhas e checksum fora do motor |
| Alta | 128. Célula inaplicável | Nunca trocar de caminho em silêncio | NotApplicable com o motivo |
| Alta | 170. Não aplicável × inviável | Separa as contradições do enunciado dos limites do protótipo e põe cada linha na matriz | Status ok, nao_aplicavel ou inviavel; colunas tipo, tamanho, configuração e seletividade; casos 1 a 3 declarados sem executar |
| Alta | 116. Descritor do caminho | A camada 2 precisa saber o que cada caminho responde | AccessPath com estrutura, campo, unique, o que find e probe respondem, ordered_by e hash_partitioned_by |
| Alta | 157. Unicidade e anulabilidade no campo | A unicidade decide o grupo de duplicatas, a parada do find e a fórmula de custo | FieldDef ganha unique e nullable; AccessPath.unique vem do esquema |
| Alta | 34. Nó folha | Exigido pelo §4.2 (B+ sem repetição de chave nas folhas) | B+ primária com os registros nas folhas encadeadas; B+ secundária com (chave, lista de RIDs), sem repetição de chave |
| Alta | 38. Carga da B+ | Decide a ocupação e a sequência física das folhas, e com isso o b | Bulk load com 70% de ocupação; a comparação com 100% fica fora da bancada |
| Alta | 39. Tamanho da tabela hash | Muda o nº de buckets e de overflows, e com isso o b | ⌈N / (α × capacidade)⌉ com α = 0,8, levado ao primo seguinte |
| Alta | 163. Hash NOT UNIQUE pelos valores distintos | Sem isso, o hash por curso teria cerca do dobro dos blocos do heap | O nº de buckets é o primo ≥ min(d, ⌈N / (α × capacidade)⌉) |
| Média | 2. Alocação por extents | Determina a contiguidade dos blocos e, com ela, o que conta como sequencial | Um arquivo do SO por arquivo lógico, que cresce de 8 em 8 blocos; o número do bloco é a posição no arquivo |
| Média | 10. Áreas fixas | Áreas adaptáveis mudariam a memória entre configurações | Áreas fixas; os subpools são criados e devolvidos a cada operação |
| Média | 15. Sem substituição com prioridade | Uma prioridade para índices esconderia a diferença entre as políticas | Sem prioridade; a retenção vem das áreas separadas |
| Média | 19. Campos fixos × variáveis | Registros fixos mantêm o bfr determinístico | Tabelas de medição só com campos fixos; campos variáveis só em Detalhes_do_Aluno |
| Média | 29. Blocagem | Pedida pelo §3.1.5 | Não estendida; ainda falta confirmar com o professor se a estendida é obrigatória |
| Média | 30. Heap | Preservar a ordem física mantém a varredura sequencial | Mapa de espaço livre; inserção sempre no fim; o erase nunca libera blocos |
| Média | 43. Colisão no hash | Pedida pelo §3.5.4 | Encadeamento de páginas de overflow por bucket |
| Média | 101. Endereço de bloco | Forma de identificar um bloco entre as camadas | BlockId{arquivo, bloco}; o bloco 0 é o cabeçalho |
| Média | 112. Granularidade da varredura | O BNL precisa manter vários blocos fixados ao mesmo tempo | BlockScan entrega um BlockCursor por bloco; vários cursores podem ficar vivos |
| Média | 115. Predicado | Restringe o que um caminho de acesso responde | Só igualdade (Point) e faixa (KeyRange) no campo do caminho; o caminho Scan é filtrado na camada 2 |
| Média | 118. Arquivos temporários | Runs e partições precisam de arquivo próprio, com custo contado | TempFile: append, seal, leitura só depois do seal, apagado no destrutor |
| Média | 120. Carga e manutenção | Operações para montar e alterar as estruturas | bulk_load, insert, erase, reorganize e occupancy |
| Média | 121. Assinatura da junção | Interface da primitiva medida | join(algoritmo, externa, interna, m, forma do resultado, parâmetros) |
| Média | 123. Parâmetros derivados de M | Chunk, runs e partições precisam vir de um lugar só | Resolvidos no plano, com o orçamento de molduras |
| Média | 126. Destino do resultado | As fórmulas não contam a gravação da saída | ResultSink conta linhas e calcula o checksum; a saída não entra no custo |
| Média | 127. Conteúdo do OpResult | O que a bancada recebe de cada operação | Linhas, checksum, delta, flush_writes, estimativa, fases e tempo |
| Média | 131. Carga de manutenção | Necessária para a ocupação "depois" (§6.1.1) | Proporção de inserções e remoções, distribuição das chaves, volume e semente |
| Média | 134. Repasses | Mantém as camadas estritas | A camada 3 repassa o controle da 4; a camada 2 repassa definição, carga e manutenção; repasses não têm lógica |
| Média | 135. Tipos que atravessam camadas | Limita o acoplamento entre as camadas | Da camada 4 até a 2 só chegam o PageGuard e a Reservation (opaca); valores ficam em common/ |
| Média | 137. Intent | Informa à política e ao prefetch como a página será usada | Duas flags: reuse e sequential |
| Média | 139. Selagem de temporários | Define quanto custa gravar e reler runs e partições | O seal grava as páginas sujas (contadas como temp) e as retira do pool; a releitura é sempre física |
| Média | 146. Seleção sem índice | O caminho sem índice precisa sempre existir | O Scan sempre executa; na seleção pontual em campo UNIQUE, para no primeiro registro (emendada) |
| Baixa | 7. Blocos por página | Pedido pelo §2.2.1 | 1 bloco por página |
| Baixa | 12. Toss-immediate | Pedido pelo §2.2.5; ligado, aproxima o LRU do MRU | Desligado; não é exercitado pela bancada |
| Baixa | 16. Prefetch | Pedido pelo §2.2.9; ligado, favorece a varredura | Desligado; não é exercitado (o do protótipo segue o número do bloco, não o percurso) |
| Baixa | 22. Campos multivalorados | Pedido pelo §3.1.2 | Tabela externa (e-mails, telefones); fora das tabelas de medição |
| Baixa | 31. Compactação do heap | Pedido pelo §3.2.2 | Reorganize quando o espaço livre passa de 30%; a manutenção da bancada não atinge esse ponto |
| Baixa | 32. Chave de ordenação | Pedido pelo §3.4.1 | Campo único (DRE, CPF ou curso) |
| Baixa | 44. Desbalanceamento do hash | Pedido pelo §3.5.4 | Cardinalidade mínima do campo, garantida pelo desenho da base; hash em sexo é proibido |

---

## Decisões de implementação (46, em 44 linhas)

| Importância | Título | Explicação da decisão | O que foi decidido |
|---|---|---|---|
| Alta | 13. Fixação de blocos | Pin sem unpin é um dos erros listados no §7.1 | Pin e unpin por PageGuard (RAII); a camada 2 controla o que fica fixado pelos cursores e pela reserva |
| Alta | 159. Reserva sempre explícita | Um cursor que esquecesse a reserva furaria M sem erro | fetch e allocate sem valor padrão; o contador data_area_requests deve ficar em zero nas operações medidas |
| Alta | 158. Formato do registro e leitura verificada | Acesso fora da faixa em campo variável é um erro do §7.1 | [null bitmap][campos fixos][fins dos VarChar][VarChar]; leitura verificada sempre, também em release |
| Alta | 162. Heap não libera blocos | Liberar um bloco no meio do heap faria a varredura devolver registros apagados | O erase só marca o slot; blocos vazios saem no reorganize |
| Alta | 141. Manutenção dos índices quando o RID muda | Índices desatualizados devolveriam registros errados | Toda movimentação é avisada a um MoveListener; o reorganize reconstrói os índices |
| Alta | 142. Checksum canônico | Permite conferir o resultado com o SQLite | Campos na ordem dos operandos, com marca de NULL; FNV-1a de 64 bits por tupla; soma módulo 2⁶⁴ |
| Alta | 27. Registros removidos | Manter o RID estável na página evita atualizar índices a cada remoção | Marcados no slot; slots nunca são renumerados |
| Alta | 37. Ocupação mínima da B+ | Não manter a ocupação mínima é um dos erros listados no §7.1 | ⌈n/2⌉ registros por folha (a divisão considera o registro novo) |
| Média | 4. Gerência de blocos livres | Pedido pelo §2.1.3 | Bitmap no bloco 0, em memória, gravado no flush e no fechamento |
| Média | 18. Fator de blocagem | Pedido pelo §3.1.1; o valor sai do layout | Derivado pelo código: 31 para Aluno (R = 127) e 59 para Inscrição, com entrada de slot de 4 B (emendada) |
| Média | 26 e 28. SlottedPage | Pedido pelo §3.1.3 e §3.1.4 | Uma única implementação de página com slots para todas as tabelas |
| Média | 33. Nó interno e fan-out | Pedido pelo §3.4.2; o valor sai do layout | Chave de 4 B, ponteiro de 4 B, cabeçalho de 24 B: 509 chaves, fan-out de 510 (emendada) |
| Média | 35. Divisão da B+ | Pedido pelo §3.4.3 | Divide a 100% |
| Média | 36. Redistribuição na B+ | Pedido pelo §3.4.3 | Redistribui antes de fundir, se o irmão puder ceder |
| Média | 42. Funções de hash | Valores diferentes entre bibliotecas tornariam o experimento irreprodutível | Multiplicativa de Knuth para inteiros, FNV-1a para textos; nunca std::hash |
| Média | 104. Página nova | Alocar não deve custar uma leitura | allocate não lê o disco: moldura zerada, suja e fixada |
| Média | 111. RID | Formato do identificador de registro | {bloco u32, slot u16, reservado u16}, 8 B |
| Média | 113. RecordView | Ler campos sem copiar o registro | Visão sem posse, válida enquanto o guard existir |
| Média | 133. Saída | Formato dos resultados para análise | CSV longo de resultados, CSV de ocupação, fases e manifesto |
| Média | 138. Contagem de allocate | Mantém a identidade requisições = acertos + faltas | allocate soma em allocations, não em requisições |
| Média | 144. Catálogo fixado | O catálogo não pode ser expulso nem esvaziado | Página do catálogo fixada do início ao fim; invalidate_all não toca nela |
| Média | 151. Falta de espaço | O que fazer quando algo não cabe em M durante a execução | DoesNotFit vira inviavel; desde a 171, só no grupo de duplicatas do sort-merge |
| Média | 152. Papéis e ordem canônica | O checksum não pode mudar com a troca de papéis | Cada entrada diz se é o operando 0 ou 1 da consulta; o plano calcula a troca |
| Média | 153. Sementes de hash | Repetir a mesma função faria tudo cair na mesma partição | Sementes diferentes para particionamento e tabela em memória, e uma por passe |
| Média | 154. Cabeçalho e bitmap | Define o tamanho máximo de um arquivo | Um bloco de cabeçalho, que cobre até 32.576 blocos (cerca de 127 MiB) (emendada) |
| Média | 155. Ciclo de vida | Destrutores que lançam derrubam o programa | Destrutores, remove_file e release não lançam; página fixada nesse momento encerra o programa com diagnóstico |
| Média | 156. E/S POSIX | Leitura e gravação por posição, sem buffer do C++ | pread, pwrite e ftruncate; plataforma oficial Linux ou WSL2 |
| Média | 161. Criar × abrir | Um caminho errado não pode criar um arquivo vazio em silêncio | create (falha se existir) e open (falha se não existir e confere o cabeçalho) |
| Média | 165. Esquema com dono nos handles | Evita ponteiro pendente para um esquema temporário | Os handles guardam shared_ptr do esquema |
| Média | 21 e 25. Campos opcionais e null bitmap | Pedido pelo §3.1.2 | Null bitmap de ⌈n_anuláveis / 8⌉ bytes, sem espaço para o valor ausente |
| Baixa | 3. Cabeçalho do arquivo | Pedido pelo §2.1.2 | Bloco 0 com magic, versão, tamanhos, nº de blocos e bitmap |
| Baixa | 20. Campos comuns | Pedido pelo §3.1.2 | Prefixo fixo, na ordem do esquema |
| Baixa | 23. Campos estruturados | Pedido pelo §3.1.2 | Achatados em colunas (endereço → logradouro, nº, CEP) |
| Baixa | 24. Campos derivados | Pedido pelo §3.1.2 | Não materializados nas tabelas de medição |
| Baixa | 40. Estrutura do bucket | Pedido pelo §3.5.2 | Um bloco com cabeçalho, registros e ponteiro de overflow |
| Baixa | 41. Tamanho do bucket | Pedido pelo §3.5.2 | Um bloco: 31 registros de Aluno (emendada) |
| Baixa | 45. Fator de carga | Ajuda a explicar o custo do hash | α real registrado depois da carga, com o comprimento médio e máximo das cadeias |
| Baixa | 114. Tipos de chave | Tipos que as estruturas aceitam como chave | int32, int64 e texto de tamanho fixo; CR em int32 × 100 |
| Baixa | 129. Especificação do experimento | Onde fica a descrição do experimento | No código da bancada (spec_padrao), com hash no manifesto; TOML opcional (emendada) |
| Baixa | 150. Migração e prefetch | Página pedida pela reserva que já está em outra região | É copiada para o subpool e conta como acerto; o prefetch só expulsa páginas não fixadas |
| Baixa | 166. Diagnóstico do checksum | Achar qual tupla diverge do SQLite | Opção de gravar as tuplas e comparar com diff (--tuplas) |
| Baixa | 167. RID inválido e ordem dos bytes | Deixar escrito o que o formato em disco supõe | kInvalidRid = {0, 0}; formatos little-endian do host |
| Baixa | 168. Liberar página fixada | Comportamento de um erro de uso | free_block de página fixada ou do bloco 0 lança logic_error |
| Baixa | 169. Sentido de "RID estável" | Corrige a redação da 111 | O RID é estável enquanto o registro fica na mesma página |

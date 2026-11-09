# Decisões pedidas pelo enunciado (1 a 45)

Estado em 08/11/2026. Lista só as decisões que o PDF da Avaliação 2 manda o grupo tomar (§2.1, §2.2, §3.1, §3.2, §3.4 e §3.5), na ordem do enunciado e com a numeração do `HANDOFF_ARQUITETURA_2026-11-08.md`. Os valores já incluem as emendas de 08/11, e os números derivados (bfr, fan-out, buckets) são os medidos pelo protótipo.

A coluna "Explicação do item" diz, em termos gerais, o que é o conceito e por que ele importa. As colunas Categoria e Importância vêm de `DECISOES_POR_CATEGORIA_2026-11-08.md`. "Não exercitada" indica uma decisão tomada que a bancada não varia nem mede.

Esta lista também preenche a coluna "valor decidido" das decisões 1 a 45, que era uma pendência da §13 do handoff.

## §2.1 Gerente de arquivos

| Nº | Item do PDF | O que o enunciado pede decidir | Explicação do item | O que foi decidido | Categoria | Importância |
|---|---|---|---|---|---|---|
| 1 | §2.1.1 e §3.1.1 | Tamanho do bloco (2k, 4k ou 8k) | O bloco é a unidade de transferência entre o disco e a memória. O tamanho dele define quantos registros cabem por bloco, o fan-out da B+ e quantos bytes o buffer tem | 4.096 B, fixo; não é fator do experimento | Arquitetura | Alta |
| 2 | §2.1.1 | Técnica de alocação dos blocos (contígua, encadeada ou por extents) | Como os blocos de um arquivo ficam no disco: seguidos (contígua), cada um apontando o próximo (encadeada) ou em grupos contíguos (extents). Isso decide se ler o arquivo em ordem é acesso sequencial | Por extents de 8 blocos: cada arquivo lógico é um arquivo do SO; o número do bloco é a posição no arquivo | Arquitetura | Média |
| 3 | §2.1.2 | Estrutura do cabeçalho do arquivo (descritor) | Bloco de controle que descreve o resto do arquivo: formato, tamanho do bloco, quantos blocos existem e quais estão livres | Bloco 0 com magic, versão, tamanho do bloco e do extent, nº de blocos e bitmap de blocos livres; um bloco cobre até 32.576 blocos | Implementação | Baixa |
| 4 | §2.1.3 | Gerência de blocos livres | Como o sistema sabe quais blocos podem ser reaproveitados: por uma lista de livres ou por um mapa de bits | Bitmap no bloco 0, mantido em memória e gravado no flush e no fechamento; blocos de dados a partir do 1 | Implementação | Média |
| 5 | §2.1.4 | Contadores de leituras e gravações, por arquivo e globais, separando sequenciais e aleatórias | São as medidas do trabalho. O acesso sequencial (o bloco seguinte ao anterior) custa menos que o aleatório, que exige deslocar a cabeça do disco (seek) | Por arquivo, por tipo de arquivo e globais; há seek quando muda o arquivo ou quando o bloco não é o anterior + 1 (cabeça de disco global) | Arquitetura | Crítica |

## §2.2 Gerente de buffer pool

| Nº | Item do PDF | O que o enunciado pede decidir | Explicação do item | O que foi decidido | Categoria | Importância |
|---|---|---|---|---|---|---|
| 6 | §2.2.1 | Nº total de páginas | Tamanho da memória do buffer, em páginas (molduras), onde os blocos lidos ficam enquanto são usados | 84 molduras (336 KiB) | Arquitetura | Alta |
| 7 | §2.2.1 | Blocos por página | Se cada página do buffer guarda um bloco do disco ou vários | 1 | Arquitetura | Baixa |
| 8 | §2.2.2 | Repartição do pool por tipo de página ou por operação | Dividir o buffer em áreas por tipo de conteúdo (dados, índices, catálogo) ou dar a cada operação a sua memória, para que um uso não expulse as páginas de outro | As duas: por tipo de página nos acessos sem reserva e um subpool exclusivo por operador nos acessos com reserva | Arquitetura | Crítica |
| 9 | §2.2.2 | Quais áreas (dados, índices, catálogo, trabalho) e tamanhos | Quanto de memória cada área recebe. A área de trabalho é a usada por ordenação e junção, e define o M dos algoritmos | Dados 32, índices 16, catálogo 4 (fixado), trabalho 32; M ≤ 32; a área de dados só é usada fora das medidas | Arquitetura | Crítica |
| 10 | §2.2.3 | Áreas fixas, autoajustáveis ou compartilhadas | Se o tamanho de cada área é fixo, se pode crescer até um limite ou se todas disputam a mesma memória | Fixas; os subpools são criados e devolvidos a cada operação | Arquitetura | Média |
| 11 | §2.2.4 | Duas políticas de substituição a comparar (LRU, MRU, FIFO, Clock) | Regra para escolher a página a expulsar quando falta espaço: a usada há mais tempo (LRU), a usada mais recentemente (MRU), a que entrou primeiro (FIFO) ou a segunda chance (Clock) | LRU × MRU, com uma instância da política por região | Arquitetura | Crítica |
| 12 | §2.2.5 | Uso de toss-immediate | Descartar a página logo depois do uso quando se sabe que ela não será reusada, como numa varredura | Desligado (não exercitada) | Arquitetura | Baixa |
| 13 | §2.2.6 | Como se dá a fixação de blocos (pinned blocks) | Marcar uma página como em uso (pin) para que ela não seja expulsa enquanto alguém a lê ou altera; depois é preciso soltá-la (unpin) | Pin e unpin por PageGuard (RAII); a camada 2 controla o que fica fixado pelos cursores e pela reserva | Implementação | Alta |
| 14 | §2.2.7 | Gravação forçada (forced output) | Gravar as páginas modificadas no disco num momento definido, como o fim de uma operação, em vez de esperar que sejam expulsas | Não há gravação forçada de dados; páginas sujas são gravadas na expulsão; o flush depois da carga não é medido; os flushes de verificação devem gravar zero | Arquitetura | Alta |
| 15 | §2.2.8 | Substituição com prioridade ou retenção preferencial de índice e catálogo | Dar preferência a manter no buffer certas páginas, como as de índice e de catálogo, por serem mais reusadas | Sem prioridade; a retenção vem das áreas separadas de índices e catálogo | Arquitetura | Média |
| 16 | §2.2.9 | Leitura antecipada (prefetching) | Ler os blocos seguintes antes de serem pedidos, apostando que o acesso continuará sequencial | Desligado (não exercitada; o prefetch do protótipo segue o número do bloco, não o percurso) | Arquitetura | Baixa |
| 17 | §2.2 (métricas) | Métricas do buffer a registrar | Indicadores do buffer: requisições, acertos (o bloco já estava na memória), faltas (foi preciso ler do disco), taxa de acerto e gravações de páginas modificadas | Requisições, acertos, faltas, taxa de acerto, alocações, leituras e gravações sequenciais e aleatórias, gravações sujas na expulsão e gravações forçadas | Arquitetura | Alta |

## §3.1 Formato dos registros e layout dos blocos

| Nº | Item do PDF | O que o enunciado pede decidir | Explicação do item | O que foi decidido | Categoria | Importância |
|---|---|---|---|---|---|---|
| 18 | §3.1.1 | Tamanho do bloco de registros e fator de blocagem | O fator de blocagem (bfr) é quantos registros cabem num bloco. Junto com o nº de registros, ele define o nº de blocos b de cada tabela | bfr derivado do layout pelo código: 31 para Aluno (R = 127) e 59 para Inscrição (R = 64), com entrada de slot de 4 B | Implementação | Média |
| 19 | §3.1.2 | Organização dos campos de tamanho fixo e variável | Campos de tamanho constante (inteiros, códigos) e de tamanho variável (textos livres). Os variáveis exigem guardar comprimento ou deslocamento | Tabelas de medição só com campos fixos; campos variáveis só em Detalhes_do_Aluno | Arquitetura | Média |
| 20 | §3.1.2 | Tratamento dos campos comuns | Os campos obrigatórios e simples do registro, e a posição deles | Prefixo fixo, na ordem do esquema | Implementação | Baixa |
| 21 | §3.1.2 | Tratamento dos campos opcionais | Campos que podem ficar sem valor (NULL) | Null bitmap, sem reservar espaço para o valor ausente | Implementação | Média |
| 22 | §3.1.2 | Tratamento dos campos multivalorados (repeating field) | Campos com vários valores no mesmo registro, como uma lista de e-mails. Podem ficar dentro do registro ou numa tabela à parte | Tabela externa, como e-mails e telefones (não exercitada) | Arquitetura | Baixa |
| 23 | §3.1.2 | Tratamento dos campos estruturados | Campos compostos de partes, como um endereço com rua, número e CEP | Achatados em colunas (endereço → logradouro, nº, CEP) | Implementação | Baixa |
| 24 | §3.1.2 | Tratamento dos campos derivados | Campos calculados a partir de outros, como a carga horária total de um curso. Podem ser guardados ou recalculados | Não materializados nas tabelas de medição (não exercitada) | Implementação | Baixa |
| 25 | §3.1.2 | Uso de null bitmap | Um bit por campo opcional indicando se ele é NULL, sem gastar espaço com o valor ausente | ⌈n_anuláveis / 8⌉ bytes no cabeçalho do registro | Implementação | Média |
| 26 | §3.1.3 | Organização dos registros fixos e variáveis dentro do bloco | Como os registros ficam arrumados dentro de um bloco, sejam de tamanho fixo ou variável | Uma única implementação de página com slots para todas as tabelas | Implementação | Média |
| 27 | §3.1.4 | Tratamento dos registros removidos no bloco | O que acontece com o espaço de um registro apagado: marcar, compactar ou reaproveitar | Marcados no slot; slots nunca são renumerados, o que mantém o RID estável na página | Implementação | Alta |
| 28 | §3.1.4 | Uso de página com slots (slotted page) | Página com um diretório de slots que apontam para os registros. Permite mover registros dentro da página sem mudar o identificador deles | Sim, em todas as tabelas | Implementação | Média |
| 29 | §3.1.5 | Blocagem não estendida (unspanned) ou estendida (spanned) | Se um registro pode ficar dividido entre dois blocos (estendida) ou precisa caber inteiro em um (não estendida) | Não estendida; ainda falta confirmar com o professor se a estendida é obrigatória | Arquitetura | Média |

## §3.2 Heap (arquivo não ordenado)

| Nº | Item do PDF | O que o enunciado pede decidir | Explicação do item | O que foi decidido | Categoria | Importância |
|---|---|---|---|---|---|---|
| 30 | §3.2.1 | Gerência dos registros removidos: mapa de espaço livre ou lista de livres | Como achar espaço para novas inserções depois das remoções: um mapa com o espaço livre de cada bloco ou uma lista dos espaços livres | Mapa de espaço livre, sem lista de livres; inserção sempre no fim; o erase nunca libera blocos | Arquitetura | Média |
| 31 | §3.2.2 | Critério para compactar os espaços livres | Quando reorganizar o arquivo para eliminar os buracos deixados pelas remoções | Reorganize quando o espaço livre passa de 30% do arquivo (não exercitada: a manutenção da bancada não chega a esse ponto) | Arquitetura | Baixa |

## §3.4 Árvore B+

| Nº | Item do PDF | O que o enunciado pede decidir | Explicação do item | O que foi decidido | Categoria | Importância |
|---|---|---|---|---|---|---|
| 32 | §3.4.1 | Chave de ordenação por campo único ou grupo de campos | O campo, ou campos, pelo qual a B+ ordena os registros | Campo único (DRE, CPF ou curso) | Arquitetura | Baixa |
| 33 | §3.4.2 | Estrutura do nó interno, tamanho do bloco e fan-out | Os nós internos guardam chaves e ponteiros para os filhos. O fan-out (nº de filhos por nó) define a altura da árvore e, com ela, o custo de uma busca | Chave de 4 B, ponteiro de 4 B e cabeçalho de 24 B: 509 chaves, fan-out de 510 | Implementação | Média |
| 34 | §3.4.2 | Estrutura do nó folha | O que as folhas guardam (os próprios registros, ou chaves com ponteiros) e se são encadeadas para permitir percorrer em ordem | B+ primária com os registros nas folhas encadeadas, no arquivo de dados; B+ secundária com (chave, lista de RIDs), sem repetição de chave | Arquitetura | Alta |
| 35 | §3.4.3 | Taxa de ocupação para divisão | Quando um nó cheio se divide em dois | Divide a 100% | Implementação | Média |
| 36 | §3.4.3 | Redistribuição | Passar registros de um nó irmão para outro, em vez de fundir, quando um nó fica vazio demais | Redistribui antes de fundir, se o irmão puder ceder | Implementação | Média |
| 37 | §3.4.3 | Taxa de ocupação mínima para fusão | Quando dois nós com poucos registros se juntam. A ocupação mínima impede que a árvore fique esparsa e alta | ⌈n/2⌉ | Implementação | Alta |
| 38 | §3.4.4 | Carga em massa (bulk loading) ou inserção registro a registro | Montar a árvore de uma vez a partir dos dados ordenados, com folhas contíguas no disco, ou inserir um registro de cada vez, o que espalha as folhas | Bulk load com 70% de ocupação na carga; inserção registro a registro na manutenção; carga a 100% não exercitada | Arquitetura | Alta |

## §3.5 Hash externo

| Nº | Item do PDF | O que o enunciado pede decidir | Explicação do item | O que foi decidido | Categoria | Importância |
|---|---|---|---|---|---|---|
| 39 | §3.5.1 | Tamanho da tabela de hash (hashing estático) | Nº de buckets do hash, fixo depois de criado. Poucos buckets geram cadeias longas de overflow; buckets demais desperdiçam blocos | ⌈N / (α × capacidade do bucket)⌉ com α = 0,8, levado ao primo seguinte; em campo NOT UNIQUE, o primo ≥ min(d, esse valor). Ex.: Aluno por DRE, 809 buckets; por curso, 211 | Arquitetura | Alta |
| 40 | §3.5.2 | Estrutura dos buckets | Como é organizado o bloco de um bucket e como ele aponta para os blocos de overflow | Um bloco com cabeçalho, registros e ponteiro de overflow | Implementação | Baixa |
| 41 | §3.5.2 | Tamanho dos buckets | Quantos blocos, e quantos registros, um bucket ocupa | Um bloco: 31 registros de Aluno | Implementação | Baixa |
| 42 | §3.5.3 | Função de hash, segundo o domínio e a distribuição do campo | A função que transforma a chave no número do bucket. Ela deve espalhar bem os valores do domínio para evitar colisões | Multiplicativa de Knuth para inteiros e FNV-1a para textos; nunca std::hash | Implementação | Média |
| 43 | §3.5.4 | Tratamento de colisão (bucket overflow) | O que fazer quando um bucket enche: encadear páginas de overflow ou procurar outro bucket (endereçamento aberto) | Encadeamento de páginas de overflow por bucket | Arquitetura | Média |
| 44 | §3.5.4 | Tratamento do desbalanceamento (skew) | Muitos registros com a mesma chave, ou caindo no mesmo bucket, geram cadeias longas e buscas caras | Cardinalidade mínima do campo, garantida pelo desenho da base; hash em sexo é proibido (não exercitada como verificação) | Arquitetura | Baixa |
| 45 | §3.5.4 | Controle do fator de carga | O fator de carga é a ocupação média dos buckets (registros ÷ capacidade). Alto demais aumenta o overflow; baixo demais desperdiça espaço | α real registrado depois da carga, com o comprimento médio e máximo das cadeias | Implementação | Baixa |

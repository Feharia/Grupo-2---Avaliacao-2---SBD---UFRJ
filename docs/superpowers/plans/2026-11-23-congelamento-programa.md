# Congelamento do programa — plano geral

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking. A execução depende de solicitação do usuário; este plano não autoriza merge, push ou novas funcionalidades.

**Goal:** Fixar uma versão reproduzível do programa que cumpra o núcleo obrigatório e passe pelos critérios técnicos de entrega.

**Architecture:** Preservar o protótipo e suas quatro camadas. Tomar o código do PR #19 como candidato, corrigir lacunas remanescentes em mudanças isoladas e aceitar a versão integrada por testes e uma execução integral.

**Tech Stack:** C++20, Linux/WSL2, CMake, GoogleTest real, CTest, ASan/UBSan e ferramentas Python com SQLite.

**Spec:** `PrototipoIA/Avaliacao_2_v2_Indexacao_e_Processamento_de_Consultas.pdf`, especialmente §§4–6 e 7.1; solicitação do usuário de avaliar somente o programa, excluindo relatório, gráficos e documentação da entrega.

## Global Constraints

- Diagnóstico de 23/11/2026: candidato `4339845c74815bf51a29fe85425d68af30095c42`; main local `4091762`. Não foi feita nova consulta remota nesta avaliação.
- As fontes do motor não foram modificadas nesta avaliação. Registros locais ainda não commitados não integram o candidato publicado.
- Blocos de 4096 B, extent padrão de 8 blocos; M=32 nas junções e M=2 nas seleções da configuração padrão.
- Preservar os contratos de M das decisões 172/173 já adotados. Overhead permitido não equivale a RSS total do processo.
- Não introduzir políticas, estruturas, algoritmos opcionais, transações ou persistência entre execuções sem requisito concreto.
- Requisitos experimentais são capacidades do programa: não confundir lacuna de indicador gerado com pendência de redação do relatório.
- Qualquer novo defeito deve ter reprodução e teste anterior ao patch. Rever consumidores se mudar `Occupancy` ou outro header público.
- Commits, pushes, merge e marcação da versão exigem autorização; a execução futura deve respeitar a autorização vigente.

## Estado reconciliado

| Área | Evidência atual | Decisão para o congelamento |
|---|---|---|
| B+, slots, blocos livres, joins e memória | Regressões presentes; 107 casos C++ aprovados novamente nesta avaliação | Preservar e executar no candidato final; não reabrir o backlog antigo por numeração |
| Ferramentas de bancada | 63 testes Python aprovados novamente, com integração da CLI habilitada | Preservar cobertura negativa e oráculo |
| Matriz e seleções obrigatórias | Rodada integral preservada: 1.159 linhas, 168 combinações, 108 células executadas e 12 não aplicáveis, sem ausências/divergências | Aceitar de novo no SHA final integrado |
| I/O | 29 alertas contra a main reproduzidos e explicados; zero alteração no controle entre motores da última etapa | Aumentos não são novos bugs por si só; manter o mesmo protocolo nas comparações |
| Ocupação/extents (CBD-011 do contexto) | Reprodução nova: Curso heap usa 8 blocos, arquivo tem 9; Disciplina usa 13, arquivo tem 17 | Corrigir a saída do programa para separar uso, folga reservada e comprimento lógico |
| Validação da manutenção (resíduo de CBD-009) | `bench.cpp:1014` imprime erro de `verify()` e continua até `return 0`; não há advertência desse tipo nos logs examinados | Fechar a lacuna de propagação de erro; não afirmar corrupção de dados sem reprodução |
| Manutenção e índices | Existem testes de RIDs, splits, exclusão, reorganização e clustering/hash com colisões | Aproveitar esses testes; ampliar somente onde faltar comparação de conteúdo após manutenção |
| #18 | Pendências relatadas de gráficos e tamanho de bloco no manifesto | Gráficos ficam fora deste plano; ampliar metadados do manifesto é melhoria útil, não bloqueador funcional do motor |

## Review Focus

1. Extent parcialmente livre: distinguir blocos usados de blocos reservados no arquivo — tarefa 2.
2. Falha estrutural após manutenção: a execução não pode terminar como sucesso — tarefa 3.
3. RIDs e duplicatas após inserções, remoções e reorganização: conteúdo dos caminhos deve concordar — tarefa 3.
4. M pequeno, dados variáveis e grupos grandes: resultado correto ou inviabilidade explícita, sem truncamento — tarefa 4, reaproveitando regressões existentes.
5. Célula obrigatória ausente, porte errado ou contagem/checksum divergente: aceitação deve falhar — tarefa 4, reaproveitando testes negativos e verificadores existentes.

## Tarefa 1: Fixar a base de integração

**Arquivos:** sem alteração funcional; referências Git e workflow `.github/workflows/correcao.yml`.

- [x] Usar `4339845` como base conhecida, contendo as correções do PR #19.
- [x] Concluir a integração autorizada do PR antes de declarar a main congelada. Preservar a #18 aberta com a estratégia já registrada em `docs/INTEGRACAO_PR_19.md`.
- [x] Confirmar que o conteúdo funcional integrado é o esperado; registrar o SHA a ser usado nas correções seguintes.

**Aceite:** existe uma base única de trabalho contendo as correções revisadas; nenhuma alteração funcional ficou apenas em um worktree.

## Tarefa 2: Completar a contabilização de ocupação

**Arquivos a revisar/alterar:** `src/common/common.hpp` (`Occupancy`), `src/layer3/structures.*`, repasses em `storage_manager.*` e `monitor.*`, metadados em `src/layer4/file_manager.*`/`buffer_pool.*`, `src/layer1/bench.cpp` e consumidores da ocupação em `tools/tabelas.py`.
Todos os caminhos acima são relativos a `PrototipoIA/Implementacao/`.

**Interfaces:** preservar a função de `StorageManager::occupancy(StructureId)` e do seu repasse no Monitor. Se o retorno `Occupancy` for ampliado, revisar todos os consumidores e o driver comum do CI, que compila contra headers antigos.

- [x] Adicionar regressões em `tests/test_layer3.cpp`/`test_layer4.cpp`: arquivo recém-criado; primeira página alocada com extent=8; crescimento; liberação/reuso; organização com dois arquivos; situação antes/depois da manutenção.
- [x] Conferir soma entre blocos usados, livres reservados e total lógico do arquivo, incluindo cabeçalho. Não usar ocupação física do SSD como oráculo.
- [x] Executar os casos contra a base para mostrar a ausência/subcontagem da métrica exigida.
- [x] Expor e exportar os valores distintos, preservando os componentes de dados/overflow/índice. Consultar metadados já mantidos pelo gerenciador, sem varredura de dados dentro das operações medidas.
- [x] Preservar `TableStats.b`, os custos dos operadores e os contadores de leituras/escritas. O número de blocos que um operador percorre não deve virar o comprimento reservado do arquivo.
- [ ] Reexecutar a suíte e o comparador de I/O contra o pai. A expectativa para os contadores de consultas é nenhuma mudança; investigar qualquer diferença.

**Aceite:** no caso reproduzido de Curso há 8 usados + 1 livre reservado = 9 totais; em Disciplina, 13 + 4 = 17. A API/CSV distinguem essas quantidades também nas demais organizações.

## Tarefa 3: Encerrar a validação da manutenção

**Arquivos:** `src/layer1/bench.cpp`, consumidores de `Monitor::verify()`; testes existentes em `tests/test_layer3.cpp`, `tests/test_bplus_bulk_load_integration.cpp` e integração da CLI em `tests/python/`.

**Interfaces:** preservar `std::string StorageManager::verify(TableId) const` e o repasse existente no Monitor; mensagem vazia representa sucesso. Não substituir esse verificador por uma auditoria completa nova de todas as estruturas.

- [x] Criar teste negativo da propagação de falha de verificação até o encerramento da bancada. A falha precisa produzir saída não bem-sucedida, sem mensagem final que anuncie conclusão normal. Injeção de falha deve ficar no teste, sem nova opção de produção.
- [x] Corrigir o caminho que hoje apenas escreve no stderr depois da manutenção; preservar contexto de tabela/estrutura no erro.
- [x] Mapear os casos de manutenção já cobertos e completar apenas lacunas: mesmos registros antes/depois via scan, hash/B+ secundária e clustering, em Heap, Hash e B+ conforme aplicável, após inserção, exclusão e reorganização. Usar oráculo independente de multiconjunto, com duplicatas e registros variáveis em casos dirigidos.
- [x] Conferir as invariantes de medição já declaradas nos testes existentes: `requests = hits + misses`, temporários dentro da janela, preparo separado e flush final esperado. Se uma falha puder passar pelo aceite como sucesso, reproduzir e fechar somente esse caminho.

**Aceite:** falha de manutenção não gera sucesso falso; os casos dirigidos preservam conteúdo e acessibilidade dos registros. Essa tarefa fecha validação, não presume defeito atual nas estruturas.

## Tarefa 4: Aceitação do candidato integrado e congelamento

**Arquivos:** testes/CLI/scripts existentes; nenhuma implementação nova se a aceitação passar.

- [ ] Criar checkout limpo do SHA final autorizado e compilar pelo CMake oficial com GoogleTest obrigatório, em Linux/WSL2.
- [ ] Executar CTest Release, ASan/UBSan, testes Python com o binário habilitado e os casos dirigidos das tarefas 2/3. A suíte cresce a partir dos 107 C++ e 63 Python atuais.
- [ ] Executar smoke com tuplas completas e SQLite, cobertura do smoke e testes negativos existentes.
- [ ] Executar bancada integral em saída nova: escala 1, LRU/MRU, M=32/2, seed conhecida, pelo menos três repetições. Rodar `reference.py`, `compare.py` e `verificar_matriz.py`; conferir seleções em torno de 1%/50%, presença das 168 combinações e portes reais.
- [ ] Exigir ausência de divergência/combinação obrigatória ausente, de inviabilidade obrigatória não resolvida e de falha de manutenção. O verificador atual pode emitir aviso para inviabilidade; aviso não equivale a aprovação do congelamento.
- [ ] Avaliar estabilidade de leituras/escritas. Casos encerrados pelo teto só por ruído de tempo não justificam alteração arbitrária do motor; a contagem e o motivo precisam estar corretos.
- [ ] Conferir comparação de I/O válida e explicar cada novo aumento. Preservar os aumentos já aceitos da mudança do contrato de M, sem ajustar fórmulas artificialmente.
- [ ] Se a aceitação passar, fixar o SHA de entrega e restringir mudanças posteriores a correções reproduzidas, com revalidação proporcional. Tag/release e alterações remotas somente se autorizadas.

**Aceite:** a versão identificada compila do zero, executa o obrigatório, produz resultados corretos e indicadores coerentes e não oculta falhas como sucesso.

## Fora dos bloqueadores do programa

Relatório, slides, gráficos, organização estética do repositório e atualização de textos históricos não condicionam este congelamento. Também não é necessário ativar prefetch/toss-immediate, acrescentar opcionais ou aproximar artificialmente estimativas das medições. LSan permanece sem validação; não foi identificado vazamento atual por isso, e sua ausência não cria sozinha uma tarefa de implementação. Melhorias adicionais ficam para depois da versão aceita.

## Limite da avaliação e próxima ação

Esta avaliação reconciliou o backlog com fontes, testes e evidências atuais;
não é uma nova auditoria exaustiva de cada função. A ocupação foi reproduzida
novamente; o problema de propagação da verificação foi constatado no fonte,
e ainda precisa do teste negativo da tarefa 3. Nenhum outro novo defeito de
tuplas foi reproduzido nesta etapa.

Recomendação: integrar a base revisada, começar pela tarefa 2 e, em seguida,
fechar a tarefa 3. As duas são independentes de relatório/documentação.
A tarefa 4 é o critério de término: se passar, congelar, sem abrir uma
sequência indefinida de melhorias.

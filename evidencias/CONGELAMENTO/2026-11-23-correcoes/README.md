# Correções para congelamento — ocupação e manutenção

Base a59f3336f23f4899667108447c57a8a8e9808544 (squash do PR #19).
O script do usuário confirmou CI da main aprovado e #18 aberta; a árvore
integrada coincide com a fonte 4339845 anteriormente revisada.

## Ocupação

Seis regressões de integração C++ falharam antes da implementação e passaram
depois. Cinco verificações Python cobrem CSV, bitmap/tamanho real, tabela e
compatibilidade histórica; o controle histórico já passava na base. O RED
Python contém subtestes por estrutura, por isso conta múltiplas falhas.

Occupancy ganhou file_total e reserved_free. A camada4 conta livres no
bitmap em memória; as cinco estruturas somam seus próprios arquivos. O CSV
preserva total histórico e acrescenta usados, livres_reservados e total_arquivo.
O consumidor de tabelas usa o total lógico quando disponível, mantendo
leitura de arquivos históricos. O driver comum compila contra headers antigos
por template/requires; as métricas indisponíveis ficam vazias nessa referência.
TableStats.b, os planos, M e contadores de operações não foram alterados.
A ABI de Occupancy requer recompilar biblioteca e consumidores juntos.

Revisão independente sem bloqueadores; conferiu bitmap, consumidores,
compilação do driver contra headers antigos e os testes de tabela.

## Manutenção

Teste da CLI usa bancada_verify_failure: inclui o fonte real da bancada,
substituindo só o tipo do Monitor no executável de teste. Chama o verify
real antes de retornar o erro injetado. Não há flag de falha no programa
normal nem alteração da API pública para os testes.

RED: processo retornava zero apesar do erro injetado, depois das operações
normais da bancada. GREEN: a CLI retorna 1 e não anuncia pronto. O patch de
produção acrescenta o retorno de erro no caminho de verificação já existente.

Novo oráculo C++ de multiconjunto com VarChar, chaves de grupo repetidas,
Heap/Hash/B+, scan e secundários hash/B+ passa antes e depois da manutenção e
da reorganização. Este é fechamento de cobertura, sem novo defeito de tuplas.

Revisão independente encontrou falta do injetor no Makefile; corrigida com
alvo próprio no all, fora dos wildcards de testes, com dependências .d.
Teste CLI reexecutado pelo revisor e aprovado; sem bloqueadores restantes.

## Validação local anterior ao commit

CMake Release, GoogleTest real: 103 testes principais + 11 de memória, todos
aprovados. Python: 69 aprovados. Make com shim: build, duas suítes e teste CLI
aprovados. Há avisos preexistentes de inicialização/else em testes antigos.
A aceitação em checkout limpo, ASan/UBSan, matriz integral e I/O pertence à
etapa seguinte. Logs brutos estão preservados nesta pasta.

# Início da execução do plano de congelamento — 23/11/2026

O usuário autorizou seguir a sequência proposta, incluindo integrar o PR #19.
A API permaneceu inacessível ao agente. O script integrar-pr19.sh destina-se
ao terminal do usuário: confere PR/head/base, ausência de vínculo de fechamento
com a #18, CI do HEAD, squash com mensagem explícita e CI da main. Preserva
branches e alterações locais. Não usa force nem privilégios administrativos.
Os fechamentos de outras issues da mensagem foram preservados do histórico;
a #18 fica apenas como referência pendente.

Seis fluxos simulados aprovados: sucesso, vínculo #18, CI reprovado, HEAD
alterado durante a espera, base alterada e falha posterior no CI da main.
Nenhuma operação real de rede foi executada pelos mocks. O script preserva
resultado do merge mesmo quando o CI posterior ainda está pendente.

Preparadas seis regressões de integração para ocupação, em arquivo temporário,
sem modificar o motor nem os testes ativos do projeto. Compiladas com os
headers e biblioteca Release de 4339845 e GoogleTest real. Todas falharam
na ausência das métricas de total lógico e folga reservada (requires permite
compilar contra a API anterior e produzir falha de asserção). Os demais
valores observados de dados/overflow passaram. Os valores esperados derivam
de blocos de 4096 B, extents de oito, um cabeçalho por arquivo e fixtures
com capacidade manual conhecida; não vêm das funções de ocupação testadas.

São testes de integração de organizações + BufferPool + FileManager, não
novos testes unitários de algoritmo. O arquivo de evidência não é descoberto
pelo CMake: será incorporado à suíte na etapa 2, após confirmar a integração.
Nenhuma correção de implementação foi aplicada antes do RED.

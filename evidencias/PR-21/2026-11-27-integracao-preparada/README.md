# Integração preparada do PR #21

Autorização: após a lista merge squash → CI da main → rodada oficial → tag,
o usuário respondeu “pode prosseguir”. A revisão e o CI do head a502a5f
já estão preservados. O ambiente continua sem acesso a api.github.com;
a operação remota depende da execução do script no terminal do usuário.

`integrar-pr21.sh` integra exclusivamente o PR #21 por squash, exige HEAD
revisado e base f3d1231, CI aprovado e ausência de vínculo que feche #18.
Usa mensagem explícita sem fechamento automático da #18. Preserva branches,
HEAD e working tree, busca origin/main, exige árvore igual à revisada,
aguarda e verifica CI do SHA integrado. Registra resultado e POST_MORTEM.
Não faz commits de registros locais nem cria tag nesta etapa.

Se o PR já estiver integrado, retoma a verificação; não repete o merge.
O checkpoint /tmp/pr21-integrado.json é escrito logo após a confirmação
do merge, antes da espera pelo Actions. Nenhum merge real foi executado
na preparação nem nos testes locais.

Verificações: bash -n e quatro cenários simulados. Caminho aberto integra
uma vez; retomada de merge existente não integra; SHA diferente e CI
reprovado abortam antes da API de merge. Ver validacao-simulada.json.
Os testes substituíram git e gh por respostas locais e escreveram somente
em diretórios temporários. Logs em /tmp/pr21-integracao-validacao-158_cy9y.

Após a execução real, validar a confirmação e executar a rodada oficial
em checkout limpo do SHA integrado, com CBD_COMMIT correto, comparações
SQLite e matriz. A tag de entrega fica condicionada a essa aceitação.

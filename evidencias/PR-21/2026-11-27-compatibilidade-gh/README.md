# Compatibilidade do script com GitHub CLI 2.45.0

O usuário executou /tmp/verificar-ci-pr21.sh e informou seis checks
aprovados, incluindo push 46019750259 e pull_request 46021541767.
O script falhou depois disso: `gh pr checks --json` não existe no gh 2.45.0
instalado neste ambiente. Reproduzido localmente antes de alterar o script.

Correção restrita: obter check-runs por `gh api --paginate`, emitindo cada
objeto como JSON em uma linha via `--jq '.check_runs[] | @json'`. Mantidos
watch e validação do workflow, SHA, base e estado aberto do PR.
Não é preciso atualizar gh nem repetir testes do programa.

`bash -n` aprovado. Validação local com respostas simuladas: script antigo
falha com código 2; corrigido aceita sucesso; recusa check com falha e SHA
incorreto, sem escrever confirmação nesses dois casos. São simulações,
não nova confirmação real do CI. Logs em /tmp/pr21-script-compat-gm2n13rn.
A consulta real permanece pendente no terminal do usuário, pois o ambiente
não acessa api.github.com. Saída anterior preservada em /tmp/pr21-ci-jk5hQi.
Nenhuma mudança no código C++ ou operação de merge/push/tag.

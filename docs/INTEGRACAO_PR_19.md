# Preparação da integração do PR #19

As correções e a matriz foram publicadas em `a6b3d48`. O usuário forneceu
os resultados de três execuções Actions aprovadas, preservados em
`evidencias/PR-19/2026-11-22-io-main/publicacao-informada.json`.
Os registros novos consolidam a comparação contra a main: 168 cenários,
29 alertas explicados pela cobrança de índices e auxiliares em M, sem
divergências de tuplas SQLite. Não há mudança de implementação nesta etapa.

## Issue #18 permanece pendente

A revisão identificou pendências de gráficos e tamanho de bloco no
manifesto. O commit histórico `dc1b1ea` contém uma palavra-chave que fecha
essa issue quando o commit entra na branch padrão. Um commit posterior
de documentação ou a edição da descrição do PR não apagam a mensagem
histórica. Não foi feita reescrita de histórico nem force push.

Para preservar o histórico publicado e manter a issue aberta, a integração
prevista é **Squash and merge**, com mensagem final explicitamente revisada.
Não usar a mensagem automática que concatena os commits, pois ela pode
copiar a palavra-chave histórica. A mensagem final pode mencionar a #18
como pendência, sem instrução de fechamento. Revisar também o título,
a descrição e eventuais vínculos manuais do PR com essa issue.

Essa preparação não constitui autorização de merge. O PR continua aberto
até autorização do usuário; a main não deve ser modificada nesta etapa.
Os commits e as evidências da revisão permanecem na branch publicada.

Referências oficiais: [palavras-chave em PRs e commits](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/linking-a-pull-request-to-an-issue)
e [métodos de merge](https://docs.github.com/en/pull-requests/reference/pull-request-merges).

## Atualização remota

O script `/tmp/atualizar-registros-pr19.sh` prepara uma cópia da descrição
existente, remove a instrução explícita de fechamento da #18, se houver,
e acrescenta a orientação de integração por squash. Mantém o restante da
descrição. Também confere que squash é permitido no repositório e que a
issue ainda está aberta. Descrições ambíguas com múltiplas referências
na mesma instrução exigem revisão do arquivo preparado antes do envio.
Antes da edição remota, reconfere título e descrição para interromper se
detectar alteração feita por outra pessoa durante a espera do CI.

O envio usa push normal para a branch de correção e para a origem do PR,
recusando divergência. O script acompanha Actions para o SHA enviado e
confere os checks do PR. Não executa merge, não habilita auto-merge e não
altera configurações do repositório.

O acesso à API estava indisponível no ambiente do agente durante a
preparação. A edição da descrição e o CI da atualização só podem ser
confirmados depois da execução do script em um terminal com acesso.

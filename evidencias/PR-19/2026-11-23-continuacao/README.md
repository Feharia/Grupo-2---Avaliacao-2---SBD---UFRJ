# Continuação da publicação interrompida em 23/11/2026

O usuário confirmou push das duas branches para 4339845 e sucesso do CI da
branch de correção (run 45107904847). Informou que os jobs seguintes ainda
estavam em execução. O helper interrompeu antes do PATCH da descrição.

Os dois JSONs reais preservados mostram PR aberto, mesmo texto e HEAD antigo
a6b3d48. A checagem anterior, reproduzida em red.log, rejeitava imediatamente
essa situação. Não foi confirmado remotamente o motivo da resposta antiga.

O helper de continuação espera até 30 consultas (29 intervalos de dois
segundos) pela atualização para 4339845. Aceita esperar apenas pelo SHA
anterior conhecido; outro SHA, PR fechado ou mudança de destino interrompem.
Reconfere o texto antes do PATCH e acompanha os checks existentes desse SHA.
Não contém push, merge ou reescrita de histórico. Os arquivos locais de
registro podem estar sem commit porque esta continuação só altera metadados
remotos do PR e acompanha os jobs do commit já publicado.

Validação: sintaxe Bash e quatro casos do classificador, incluindo o JSON
real. Seis fluxos simulados aprovados: atraso seguido de atualização,
terceiro SHA, PR fechado, timeout, edição concorrente e falha do CI. Somente
o fluxo de atraso e o de falha posterior no CI fizeram PATCH simulado;
nenhum realizou mutação Git. As esperas dos mocks não dormiram realmente.
Os logs dos fluxos e o helper estão preservados nesta pasta.

O comando /tmp/atualizar-registros-pr19.sh foi atualizado para executar esta
continuação; a versão anterior está em /tmp/atualizar-registros-pr19-antes-espera.sh.
Nenhuma operação remota foi executada pelo agente nesta correção do helper.

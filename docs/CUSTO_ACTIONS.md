# Consumo do GitHub Actions — avaliação em 13/11/2026

O usuário confirmou que este repositório é **público**. O workflow usa apenas
`ubuntu-24.04`, runner padrão: sua computação não desconta a franquia mensal,
independentemente de ultrapassar 1.000 minutos de duração somada. A regra
publicada é uso gratuito e ilimitado desses runners em repositórios públicos.
[GitHub: escolha de runners](https://docs.github.com/en/actions/how-tos/write-workflows/choose-where-workflows-run/choose-the-runner-for-a-job).

Não há consulta autenticada ao faturamento do proprietário nesta avaliação.
A visibilidade foi informada pelo usuário; não foi inferida de uma falha de
acesso ao site. A primeira execução hospedada do workflow ainda está pendente.

## Se o repositório passar a ser privado

A documentação atual expressa o GitHub Free como **US$12 de computação mensal
incluída**, compartilhada com outros usos elegíveis. Ao preço publicado de
US$0,006 por minuto do Linux padrão de 2 cores, equivale a cerca de **2.000
minutos**, se usados apenas nesse SKU, e não a uma franquia geral de 1.000.
[Franquias](https://docs.github.com/en/billing/reference/product-usage-included),
[preços de runners](https://docs.github.com/en/billing/reference/actions-runner-pricing).

O consumo é somado por job; paralelismo reduz o tempo de espera, não essa soma.
Há dois jobs da matriz e o agregador. Minutos parciais são arredondados por
job. Reexecuções e dois eventos do mesmo commit (push e PR) consomem novamente.
[GitHub: tempo de execução](https://docs.github.com/en/actions/how-tos/monitor-workflows/view-job-execution-time).

Sem histórico hosted, uma reserva conservadora para planejamento é **8–12
minutos Linux somados por execução** para build/testes/apt/checkouts/uploads
e suíte de I/O. É estimativa, não medição do runner. Assim, 100 execuções/mês
podem somar 800–1.200 minutos; 200 podem somar 1.600–2.400. Sob um orçamento
hipotético de 1.000 minutos, haveria espaço para aproximadamente 83–125
execuções. As primeiras execuções reais devem substituir essa aproximação.

Evidência local: o CTest anterior levou 0,77 s em Release e 65,18 s em
ASan/UBSan. A medição final com bibliotecas prontas levou cerca de 13 s por
lado, incluindo recompilar o driver e conferir SQLite; um build completo de
referência acrescenta dezenas de segundos. Esses tempos não incluem apt,
checkout, upload, inicialização e arredondamento de jobs do GitHub.

Os limites configurados são 20 minutos para cada job da matriz e 2 para o
agregador: não representam a duração esperada. O cancelamento de execuções
antigas economiza trabalho do mesmo ref; push e PR usam refs distintos e
podem executar ambos.

## Artifacts

Armazenamento tem contabilização própria; a franquia publicada do Free é
500 MB, compartilhada com Packages, com apropriação por tempo de retenção.
[GitHub: faturamento e armazenamento](https://docs.github.com/en/billing/concepts/product-billing/github-actions).

Uma aproximação local do artifact normal, incluindo dumps usuais e novos
CSV/metadados de I/O, resultou em **15.297.798 bytes comprimidos**, cerca de
14,6 MiB, para 182 arquivos. A compressão foi ZIP/deflate nível 6. Esse valor
é estimado; o tamanho publicado pode variar. O grosso vem dos dumps de
correção já existentes; a suíte de I/O não envia novos dumps ou builds.

Com retenção de 14 dias, cerca de 34 artifacts desse tamanho já representam
500 MiB simultâneos, desconsiderando outras evidências e Packages. Isso não
é uma conta completa de faturamento: a cobrança considera armazenamento
acumulado por hora. Acompanhar a página de uso após ativar o workflow. Se
necessário, reduzir retenção dos dumps ou guardá-los somente em falhas,
preservando os relatórios pequenos por mais tempo. A retenção não foi mudada
nesta tarefa, e nenhuma configuração de cobrança foi alterada.

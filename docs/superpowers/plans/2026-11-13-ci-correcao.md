# Gate de correção — plano autorizado em 13/11/2026

Escopo: implementar a primeira melhoria aprovada pelo usuário. Execução direta
desta autorização; sem commit, push, merge ou mudanças nas regras remotas.

1. Criar CI para push, pull request e acionamento manual, com GoogleTest
   obrigatório, builds Release e Debug com ASan/UBSan e toda a suíte CTest.
2. No Release, executar a bancada reduzida com tuplas e conferir com SQLite.
   Contrato mínimo deste smoke: LRU e MRU; oito seleções (`sel_pt_dre`,
   `sel_pt_cpf`, faixas baixa/alta de dre, cr e curso); os workloads
   `j_mestre_detalhe`, `j_pequenas` e `j_grande_pequena`, cada um com BNL,
   hash e sort_merge. Cada cenário deve ter ao menos uma execução ok por
   política; cada execução ok deve ter seu dump de tuplas. Este inventário
   é um contrato de cobertura do CI, não certificação da matriz integral.
3. Antes de implementar o verificador de cobertura, escrever testes Python
   independentes com fixtures explícitas: aprovação, cenários ausentes,
   apenas casos não executados, dump ausente e entradas inválidas.
4. Publicar logs, XML dos testes e CSVs/tuplas como artifacts; agregar os jobs
   em um check `gate-correcao` que também rejeite cancelamento/falha.
5. Validar localmente os comandos, os casos negativos e a sintaxe do workflow;
   pedir revisão independente e registrar evidências no POST_MORTEM.md.
6. Documentar a ativação posterior: enviar o workflow mediante autorização e
   exigir `gate-correcao` nas regras da main. A criação do YAML não configura
   essa exigência automaticamente.

Fora do escopo: reorganização do repositório, comparação de desempenho de I/O,
alterações no motor e execução integral da matriz experimental.

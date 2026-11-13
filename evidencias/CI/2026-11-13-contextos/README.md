# Correção da validação do workflow no GitHub

Execução inicial: [43456714396](https://github.com/Feharia/Grupo-2---Avaliacao-2---SBD---UFRJ/actions/runs/43456714396),
commit `50d427830653b85080b8d815c73c4783216fe749`. O usuário forneceu o
resultado do `gh run list`, o resumo do `gh run view` e as anotações da página
do Actions. Metadados em `execucao-original.json`; mensagem em `anotacoes.txt`.

O GitHub rejeitou o workflow antes de iniciar jobs, por cinco usos de
`runner.temp` em `jobs.verificar.env`. Por isso não existiam logs de testes.
A [tabela oficial de contextos](https://docs.github.com/en/actions/reference/workflows-and-actions/contexts)
permite `runner` em steps, mas não no env do job.

Correção: os cinco caminhos são definidos em um step após checkout, usando
`RUNNER_TEMP` e escritos em `GITHUB_ENV`. Valores preservados para normal e
sanitizers. O upload exige sucesso desse step, evitando expandir caminhos
vazios se checkout/inicialização falhar. Falhas posteriores ainda permitem
preservar artifacts. Motor e protocolo de medição não foram alterados.

- `contexto-red.log`: regressão dirigida falhou nas cinco referências antes
  da correção.
- `artefato-red.log`: teste da pré-condição de upload falhou antes da proteção.
- `testes-python-green.log`: 30 testes Python aprovados após as correções.
- `validacao-local.log`: executou o step de inicialização nos dois modos,
  verificou os cinco caminhos e conferiu sintaxe de todos os scripts/YAML.

Comandos, a partir de `PrototipoIA/Implementacao/`:

```bash
python3 -I -m unittest discover -s tests/python -p test_workflow_ci.py -v
python3 -I -m unittest discover -s tests/python -v
```

As regressões de contexto/upload são dirigidas ao formato atual do workflow;
não constituem parser YAML geral nem substituem actionlint ou validação
hospedada. A primeira validação local de CI anterior a esta correção verificou
YAML e Bash, mas não detectou a restrição de contextos do GitHub; essa lacuna
foi confirmada pela execução real.

Revisão independente identificou o risco de upload sem inicialização; foi
corrigido e revisto, sem novo achado concreto. A consulta direta do agente
ao GitHub continua indisponível por rede. Uma nova execução hospedada ainda
precisa ser confirmada; não foi alegado que o CI remoto passou.

Os snapshots de workflow/docs anteriores foram preservados em
`evidencias/IO/2026-11-13/fontes/`, com hashes originais intactos. O manifesto
`FONTES.sha256` deste diretório identifica a versão corrigida e seus testes;
verificar da raiz do repositório com `sha256sum -c`.

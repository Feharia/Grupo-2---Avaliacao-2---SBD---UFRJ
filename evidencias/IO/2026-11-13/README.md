# Validação da suíte io-rapido-v1

13/11/2026, branch `ci/gate-correcao`, HEAD
`edb9c84502bcfbb79b5fb974ee866b46374214f3`; ferramentas de CI/I/O ainda sem
commit. Referência do motor: `b61b87b4c3363e84eb199a21afaaee063a74e775`.
O mesmo driver do candidato foi compilado com os headers e biblioteca de
cada revisão. Os hashes de ferramentas, fontes, binário, dados e protocolo
estão nos metadados de cada medição.

| Métrica — soma das 108 consultas | Referência | Candidato | Delta |
|---|---:|---:|---:|
| Leituras | 45.480 | 45.456 | -24 |
| Escritas antes do flush | 5.432 | 5.432 | 0 |
| Total | 50.912 | 50.888 | -24 |
| Escritas no flush final | 0 | 0 | 0 |

Houve redução de uma leitura em 24 cenários e nenhum aumento. A soma descreve
somente este conjunto de consultas. Não é uma previsão universal nem mede
carga/manutenção. A comparação por cenário está em `comparacao/`; cada lado
passou pelo SQLite e comparação de tuplas, com 108 execuções corretas e 132
linhas não aplicáveis. `controle-determinismo/` compara duas execuções do
mesmo candidato: todos os deltas de contadores são zero.

`workflow-io.log` registra a execução local do bloco **atual** de I/O extraído
diretamente do YAML, em bash `-e -o pipefail`. Incluiu build fresco apenas da
biblioteca de referência e reutilizou a biblioteca Release candidata. Tempos
totais desse bloco: 30,245 s para a referência e 12,912 s para o candidato.
Medições separadas com bibliotecas prontas e repetição estão nos três arquivos
`final-*-medicao.json` (cerca de 13 s cada). Tempos incluem compilação do
driver e validação SQLite, não representam latência da consulta.

Comandos do bloco, a partir de `PrototipoIA/Implementacao/`:

```bash
python3 -I tools/medir_io.py --source "$referencia_source" \
  --commit "$referencia_sha" --saida "$IO_DIR/referencia"
python3 -I tools/medir_io.py --source "$PWD" --build-dir "$BUILD_DIR" \
  --commit "$GITHUB_SHA" --saida "$IO_DIR/candidato"
python3 -I tools/comparar_io.py "$IO_DIR/referencia" "$IO_DIR/candidato" \
  "$IO_DIR/comparacao"
```

Temporários usados: `/tmp/cbd-io-00fhvO/workflow-final/`. Referência em clone
local isolado `/tmp/cbd-io-00fhvO/_io_referencia/`; biblioteca candidata em
`/tmp/cbd-ci-kzV5kR/cbd-normal/build/`. O clone local não alterou o checkout
do usuário. Ver `docs/IO.md` para reproduzir sem depender desses temporários.

Verificações adicionais preservadas:

- `testes-python-final.log`: 28 testes aprovados. Expectativas sintéticas;
  inclui precisão >2^53, checksum de 64 bits, zero, aumentos/queda, status,
  duplicatas/ausências, Config/flags/dados incompatíveis, CSV alterado e CLI
  que alerta sem falhar e não sobrescreve evidências.
- `red-protocolo.log`: os testes dos dois achados da primeira revisão
  falharam antes de implementar inventário estrito e flags reais. A primeira
  execução do comparador também falhou quando o módulo ainda não existia,
  registrada na sessão, sem log local preservado.
- `ctest-release.log` e `gtest-release.xml`: 59 casos GoogleTest aprovados,
  CTest 0,77 s. Motor não foi modificado nesta tarefa. ASan/UBSan do mesmo
  motor já está em `evidencias/CI/2026-11-13/`; não foi repetido sem necessidade.
- `workflow-validacao.log`: YAML, sintaxe bash, seleção de referência para
  push/PR/manual/branch nova e agregador success/failure/cancelled/skipped.
- `tamanho-artifact-final.json`: aproximação ZIP nível 6, 182 arquivos,
  15.297.798 bytes comprimidos; inclui os dumps de correção já existentes,
  sem incluir dumps adicionais da suíte de I/O.

Revisão independente encontrou inicialmente cobertura insuficiente de
variantes e flags declaradas incompletas. Ambos foram corrigidos e revistos;
nenhum novo defeito concreto foi encontrado na revisão final. GitHub hosted,
checkout via rede e upload não foram executados localmente. `actionlint` não
está instalado; YAML/bash não substituem execução no serviço.

O workflow e `docs/CI.md` foram corrigidos depois desta medição. Suas versões
exatas desta etapa foram preservadas em `fontes/` e conferidas contra os
hashes originais, sem alterar `FONTES.sha256`. Conferir o conjunto histórico,
da raiz do repositório:

```bash
sha256sum -c evidencias/IO/2026-11-13/FONTES_PRESERVADAS.sha256
```

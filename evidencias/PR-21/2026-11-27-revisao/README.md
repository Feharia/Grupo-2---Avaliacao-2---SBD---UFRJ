# Revisão local do PR #21 — 27/11/2026

Base: `f3d12311bfece737fa2f2c3b6decedcccfa5696f`.
Head recebido por fetch do usuário: `a502a5f573626623b6fd2b71195902aeceeadb24`.
Escopo: manifesto/configuração física, consumidor tabelas.py, diagrama e
textos alterados. Sem nova auditoria geral. Não houve correção de código,
commit, push, merge, tag ou alteração da main.

## Resultado

Nenhum bloqueador funcional encontrado no diff. A bancada usa uma Config
base comum; somente política e diretório variam. Os valores registrados
são os usados pelo Monitor/StorageManager/BufferPool/FileManager.

- Build CMake Release do zero com GCC 13.3 e GoogleTest real, exit 0.
- CTest: 2 executáveis, 103 + 11 = 114 casos, nenhum erro ou caso desativado.
- Python: 69 testes, nenhum skip ou falha, inclusive os testes de CLI.
- io-rapido-v2: 168 cenários; leituras, gravações e flush idênticos.
  Ambos os lados passaram no SQLite com tuplas completas: zero ausências
  e divergências; cobertura mínima 168 execuções com dumps presentes.
- Validação adicional com a bancada nativa: manifesto com bloco 4096,
  extent 8, áreas 32/16/4/32, flags desligadas e SHA correto. Cabeçalhos
  dos 37 arquivos de cada política LRU/MRU confirmaram bloco e extent.
- tabelas.py: 16 tabelas geradas tanto para a saída nova quanto para os
  CSVs integrais legados do congelamento; a ausência de bloco no manifesto
  antigo aparece como “não registrado”. Não relabelamos dados históricos.
- Diagrama: conferência visual PNG/SVG e revisão independente das interfaces;
  on_moved/set_on_move corresponde ao código e substitui classes inexistentes.

## Método e limites

A referência usa biblioteca Release já compilada em checkout limpo do commit
8569e0e. Sua árvore `PrototipoIA/Implementacao` é idêntica à de f3d1231
(`6592ddbf73aeb73adacab23f678359c3e290e790`), confirmado via Git. Não misturamos
headers e bibliotecas entre versões. As duas medições usam o mesmo driver
candidato, como previsto no protocolo; a bancada nativa também foi validada
separadamente para o manifesto. As assinaturas de configuração, dados,
compilação e ferramentas foram comparadas pelo verificador.

Não houve comparação de desempenho temporal: os testes rodaram em paralelo.
Não repetimos integral escala 1 nem sanitizers nesta revisão localizada;
nenhuma alteração de layout, ownership, algoritmo ou orçamento foi feita.
A aceitação anterior do congelamento continua sendo evidência histórica,
não resultado executado para este novo SHA.

Build apresentou avisos preexistentes em testes não alterados (dangling-else
e inicializadores de KeyRange); não houve erro de compilação. O diagrama
mantém termos herdados como “físico” e ext4. O README explica a contagem
arquivo↔buffer; uniformizar a terminologia seria ajuste documental opcional.
Não foi identificada inexatidão nova que bloqueie este PR.

O acesso ao GitHub falhou no ambiente: pr-remote-error.log. O fetch confirma
somente o objeto local recebido. CI/estado atual do PR devem ser confirmados
no terminal do usuário antes da integração. Congelar a entrega exige escolher
o SHA integrado e gerar a rodada oficial com o novo manifesto; criar tag
somente com autorização. Esta revisão não fecha issues.

## Comandos reproduzíveis

`workdir.txt` identifica a cópia isolada e suas saídas. Variáveis abaixo devem
apontar para diretórios novos ao repetir medições.

```bash
src=/tmp/cbd-pr21-review-_lq6ai_g/source/PrototipoIA/Implementacao
review=/tmp/cbd-pr21-review-_lq6ai_g
cmake -S "$src" -B "$review/release" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/home/carlos/.local/cbd-deps/googletest-03597a01ee50ed33e9dfd640b249b4be3799d395
cmake --build "$review/release" --parallel 2
CBD_TEST_DIR="$review/test-data" GTEST_OUTPUT="xml:$review/gtest/" \
  ctest --test-dir "$review/release" --output-on-failure --no-tests=error
cd "$src"
BUILD_DIR="$review/release" python3 -I -m unittest discover -s tests/python -v
python3 -I tools/medir_io.py \
  --source /tmp/cbd-freeze-validacao-10reharx/candidate/PrototipoIA/Implementacao \
  --build-dir /tmp/cbd-freeze-validacao-10reharx/release \
  --commit f3d12311bfece737fa2f2c3b6decedcccfa5696f --saida "$review/io-base"
python3 -I tools/medir_io.py --source "$src" --build-dir "$review/release" \
  --commit a502a5f573626623b6fd2b71195902aeceeadb24 --saida "$review/io-pr21"
python3 -I tools/comparar_io.py "$review/io-base" "$review/io-pr21" "$review/comparacao"
python3 -I "$review/validate_manifest.py"
```

As saídas existentes devem ser preservadas; não repetir os comandos de medição
acima sem escolher diretórios novos. Dados/arquivos do motor/dumps/binários
permanecem em /tmp; CSVs, metadados, logs e teste adicional foram copiados aqui.
Nenhuma asserção permanente foi acrescentada à suíte do PR.

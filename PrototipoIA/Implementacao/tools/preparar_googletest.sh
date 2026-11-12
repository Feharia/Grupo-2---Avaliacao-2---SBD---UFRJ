#!/usr/bin/env bash
# Instala GoogleTest no usuário e valida o projeto, sem alterar o build existente.
# Revisão fixa do exemplo oficial: https://google.github.io/googletest/quickstart-cmake.html
set -euo pipefail

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  cat <<'EOF'
Uso: bash preparar_googletest.sh

Requer Git, compilador C++20, CMake >= 3.22, CTest e acesso ao GitHub.
Instala GoogleTest em ~/.local/cbd-deps/, sem sudo.
Executa os testes normais e com ASan/UBSan (LSan desativado).
Preserva builds e logs em um diretório temporário novo e registra o resultado
no POST_MORTEM.md da raiz do checkout. Não executa commits, push ou PR.
EOF
  exit 0
fi
if (( $# != 0 )); then
  printf 'Argumentos inválidos. Use --help.\n' >&2
  exit 2
fi

for cbd_ferramenta in git cmake ctest c++ tee mktemp; do
  if ! command -v "$cbd_ferramenta" >/dev/null 2>&1; then
    printf 'Ferramenta necessária ausente: %s\n' "$cbd_ferramenta" >&2
    exit 1
  fi
done

cbd_script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cbd_projeto="$(cd -- "$cbd_script_dir/.." && pwd)"
cbd_repo="$(git -C "$cbd_projeto" rev-parse --show-toplevel)"
cbd_revisao="03597a01ee50ed33e9dfd640b249b4be3799d395"
cbd_prefixo="$HOME/.local/cbd-deps/googletest-$cbd_revisao"
cbd_trabalho="$(mktemp -d "${TMPDIR:-/tmp}/cbd-googletest.XXXXXX")"
cbd_etapa="preparação"

cbd_registrar_resultado() {
  local cbd_status=$?
  trap - EXIT
  {
    printf '\n## %s — Preparação do GoogleTest pelo usuário\n' "$(date -Iseconds)"
    printf -- '- SHA do projeto: %s\n' "$(git -C "$cbd_repo" rev-parse HEAD)"
    printf -- '- Script: PrototipoIA/Implementacao/tools/preparar_googletest.sh\n'
    printf -- '- Revisão do GoogleTest: %s\n' "$cbd_revisao"
    printf -- '- Instalação local: %s\n' "$cbd_prefixo"
    printf -- '- Última etapa: %s; código de saída: %s.\n' "$cbd_etapa" "$cbd_status"
    printf -- '- Builds e logs: %s (diretório temporário; conservar se necessário).\n' "$cbd_trabalho"
    printf -- '- LSan desativado na rodada com ASan/UBSan.\n'
  } >> "$cbd_repo/POST_MORTEM.md"
  printf '\nArquivos da execução: %s\n' "$cbd_trabalho"
  exit "$cbd_status"
}
trap cbd_registrar_resultado EXIT

# CMAKE_REQUIRE_FIND_PACKAGE_GTest existe a partir do CMake 3.22.
cbd_etapa="verificação da versão do CMake"
cmake --version | tee "$cbd_trabalho/cmake-version.log"
read -r _ _ cbd_cmake_versao < "$cbd_trabalho/cmake-version.log"
if [[ ! "$cbd_cmake_versao" =~ ^([0-9]+)\.([0-9]+) ]] ||
   (( BASH_REMATCH[1] < 3 || (BASH_REMATCH[1] == 3 && BASH_REMATCH[2] < 22) )); then
  printf 'Este script requer CMake >= 3.22.\n' >&2
  exit 1
fi

cbd_etapa="obtenção do GoogleTest"
git clone https://github.com/google/googletest.git "$cbd_trabalho/googletest" \
  2>&1 | tee "$cbd_trabalho/download.log"
git -C "$cbd_trabalho/googletest" checkout --detach "$cbd_revisao" \
  2>&1 | tee "$cbd_trabalho/revisao.log"

cbd_etapa="compilação e instalação do GoogleTest"
cmake -S "$cbd_trabalho/googletest" -B "$cbd_trabalho/build-dependencia" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$cbd_prefixo" \
  -DBUILD_GMOCK=OFF -DINSTALL_GTEST=ON \
  2>&1 | tee "$cbd_trabalho/configuracao-dependencia.log"
cmake --build "$cbd_trabalho/build-dependencia" --parallel 2 \
  2>&1 | tee "$cbd_trabalho/compilacao-dependencia.log"
cmake --install "$cbd_trabalho/build-dependencia" \
  2>&1 | tee "$cbd_trabalho/instalacao.log"

for cbd_modo in normal sanitizers; do
  cbd_sanitizar=OFF
  if [[ "$cbd_modo" == sanitizers ]]; then
    cbd_sanitizar=ON
  fi
  cbd_build="$cbd_trabalho/build-$cbd_modo"
  cbd_etapa="configuração do projeto: $cbd_modo"

  # Exige GoogleTest real, impedindo fallback para o substituto local.
  cmake -S "$cbd_projeto" -B "$cbd_build" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="$cbd_prefixo" \
    -DCMAKE_REQUIRE_FIND_PACKAGE_GTest=TRUE \
    -DCBD_SANITIZE="$cbd_sanitizar" \
    2>&1 | tee "$cbd_trabalho/configuracao-$cbd_modo.log"

  cbd_etapa="compilação do projeto: $cbd_modo"
  cmake --build "$cbd_build" --parallel 2 \
    2>&1 | tee "$cbd_trabalho/compilacao-$cbd_modo.log"

  cbd_etapa="testes: $cbd_modo"
  CBD_TEST_DIR="$cbd_trabalho/dados-$cbd_modo" \
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    ctest --test-dir "$cbd_build" --output-on-failure \
    2>&1 | tee "$cbd_trabalho/testes-$cbd_modo.log"
done

cbd_etapa="concluído: testes normais e ASan/UBSan aprovados"
printf '\n%s\n' "$cbd_etapa"

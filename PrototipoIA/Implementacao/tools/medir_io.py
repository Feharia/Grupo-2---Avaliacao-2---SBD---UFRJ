#!/usr/bin/env python3
"""Executa e valida o perfil io-rapido-v1 em diretório novo; usa Python padrão."""
import argparse
import csv
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

TOOLS = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("comparar_io", TOOLS / "comparar_io.py")
comparison = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(comparison)
PARAMETERS = ("compilador", "flags", "escala", "semente", "reps", "tolerancia",
              "m_juncao", "m_selecao", "lote", "hash_especificacao")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def measure(source, output, commit, build_dir=None):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("--commit deve identificar o SHA completo medido")
    output.mkdir(parents=True, exist_ok=False)
    logs = output / "logs"
    logs.mkdir()
    started = time.monotonic()
    compiler = os.environ.get("CXX", "g++")
    env = os.environ | {"CBD_COMMIT": commit}

    def run(name, command):
        print(f"I/O: {name}", flush=True)
        with (logs / f"{name}.log").open("w") as log:
            log.write(shlex.join(map(str, command)) + "\n")
            log.flush()
            subprocess.run(list(map(str, command)), cwd=source, env=env,
                           stdout=log, stderr=subprocess.STDOUT, check=True)

    if build_dir is None:
        build_dir = output / "build"
        run("configure", ["cmake", "-S", source, "-B", build_dir,
                          "-DCMAKE_BUILD_TYPE=Release", "-DCBD_SANITIZE=OFF",
                          f"-DCMAKE_CXX_COMPILER={compiler}"])
        run("build", ["cmake", "--build", build_dir, "--target", "cbd_motor", "--parallel", "2"])
    flags = comparison.compiler_options(json.loads((build_dir / "compile_commands.json").read_text()), source)
    binary = output / "bancada_io"
    driver = TOOLS.parent / "src/layer1/bench.cpp"
    # Mesmo driver nos dois lados: fixa fill, variantes e preparação do cache.
    # Headers e biblioteca são da revisão medida, evitando misturar ABIs.
    run("driver-build", [compiler, "-std=c++20", "-O3", "-DNDEBUG", '-DCBD_FLAGS="io-rapido-v1 -O3 -DNDEBUG"',
                         "-I", source / "src", driver, build_dir / "libcbd_motor.a", "-o", binary])
    run("config-probe-build", [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Wpedantic",
                               "-I", source / "src", TOOLS / "config_io.cpp", "-o", output / "config_io"])
    config = json.loads(subprocess.check_output([str(output / "config_io")], text=True))
    smoke = output / "smoke"
    run("bancada", [binary, "--rapido", "--tuplas", "--politicas", "lru,mru",
                    "--escala", "0.1", "--reps-min", "1", "--reps-max", "1",
                    "--m-juncao", "32", "--m-selecao", "2", "--lote", "100", "--saida", smoke])
    # Ferramentas do candidato verificam ambos os lados, inclusive a referência.
    run("sqlite", [sys.executable, "-I", TOOLS / "reference.py", smoke, "--tuplas"])
    run("cobertura", [sys.executable, "-I", TOOLS / "validar_smoke_ci.py", smoke])
    run("comparacao-sqlite", [sys.executable, "-I", TOOLS / "compare.py", smoke, "--tuplas"])
    inventory = TOOLS / "io_cenarios_v1.json"
    with (smoke / "resultados.csv").open(newline="") as file:
        rows = list(csv.DictReader(file))
    comparison.index_rows(rows)
    comparison.ensure_inventory(rows, {tuple(key) for key in json.loads(inventory.read_text())})
    manifest = dict(line.split(": ", 1) for line in (smoke / "manifesto.txt").read_text().splitlines())
    expected = {"escala": "0.1", "semente": "20261122", "reps": "1..1",
                "m_juncao": "32", "m_selecao": "2", "lote": "100"}
    if any(manifest.get(key) != value for key, value in expected.items()):
        raise ValueError("a bancada não respeitou o perfil io-rapido-v1")
    inputs = [smoke / name for name in ("esquema.csv", "constantes.csv", "operacoes.csv")]
    inputs += sorted((smoke / "dados").glob("*.csv")) + sorted((smoke / "lotes").glob("*.csv"))
    if not list((smoke / "dados").glob("*.csv")) or not list((smoke / "lotes").glob("*.csv")):
        raise ValueError("dados ou lotes ausentes")
    hashes = {name: sha(TOOLS / name) for name in
              ("medir_io.py", "comparar_io.py", "config_io.cpp", "io_cenarios_v1.json", "validar_smoke_ci.py", "reference.py", "compare.py")}
    hashes["driver_bancada.cpp"] = sha(driver)
    metadata = {
        "protocolo": "io-rapido-v1", "commit": commit, "validado_sqlite_tuplas": True,
        "assinatura": {"manifesto": {key: manifest[key] for key in PARAMETERS}, "config": config,
                       "opcoes_compilacao_motor": flags,
                       "dados": {str(path.relative_to(smoke)): sha(path) for path in inputs}},
        "tool_hash": hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest(),
        "ferramentas_sha256": hashes, "resultados_sha256": sha(smoke / "resultados.csv"),
        "inventario_sha256": sha(inventory),
        "binario_sha256": sha(binary), "duracao_total_s": round(time.monotonic() - started, 3),
        "fontes_sha256": {str(path.relative_to(source)): sha(path)
                          for path in sorted((source / "src").rglob("*"))
                          if path.suffix in (".cpp", ".hpp")},
    }
    (output / "medicao.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")
    print(f"Medição validada: {commit} em {metadata['duracao_total_s']} s", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="diretório Implementacao da revisão")
    parser.add_argument("--saida", required=True, type=Path, help="diretório novo")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--build-dir", type=Path, help="reutilizar biblioteca Release e compile_commands da mesma revisão")
    args = parser.parse_args()
    try:
        measure(args.source.resolve(), args.saida.resolve(), args.commit,
                args.build_dir.resolve() if args.build_dir else None)
        return 0
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"Medição de I/O falhou: {error}; consultar logs na saída", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

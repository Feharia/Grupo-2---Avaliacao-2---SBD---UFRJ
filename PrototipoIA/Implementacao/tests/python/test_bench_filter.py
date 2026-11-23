"""Integração da CLI da bancada com o SQLite e o filtro de operações."""
import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[2]
BINARY = Path(os.environ.get("BUILD_DIR", SOURCE / "build")) / "bancada"


@unittest.skipUnless(BINARY.is_file(), "defina BUILD_DIR para o build CMake da bancada")
class BenchFilterTest(unittest.TestCase):
    def test_consulta_filtrada_confere_com_sqlite(self):
        with tempfile.TemporaryDirectory() as temporary:
            out = Path(temporary) / "saida"
            commands = [
                [str(BINARY), "--rapido", "--tuplas", "--so", "j_pequenas", "--saida", str(out)],
                [sys.executable, "-I", str(SOURCE / "tools/reference.py"), str(out), "--tuplas"],
                [sys.executable, "-I", str(SOURCE / "tools/compare.py"), str(out), "--tuplas"],
            ]
            for command in commands:
                result = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            with (out / "operacoes.csv").open(newline="") as f:
                self.assertEqual({r["operacao"] for r in csv.DictReader(f)}, {"j_pequenas"})

    def test_filtro_sem_correspondencia_reprova(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run([str(BINARY), "--rapido", "--so", "consulta_inexistente",
                                     "--saida", str(Path(temporary) / "saida")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)

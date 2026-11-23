"""Erro de verificação deve reprovar a execução da bancada, sem anunciar sucesso."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[2]
BUILD = Path(os.environ.get("BUILD_DIR", SOURCE / "build"))


@unittest.skipUnless((BUILD / "bancada").is_file(), "defina BUILD_DIR para o build CMake")
class MaintenanceFailureTest(unittest.TestCase):
    def test_falha_de_verificacao_reprova_a_bancada(self):
        binary = BUILD / "bancada_verify_failure"
        self.assertTrue(binary.is_file(), "compile todos os alvos de teste do CMake")
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run([str(binary), "--rapido", "--so", "j_pequenas", "--politicas", "lru",
                                     "--saida", str(Path(temporary) / "saida")], capture_output=True, text=True)
            self.assertIn("falha_injetada_no_verificador", result.stderr)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotIn("pronto em", result.stdout)

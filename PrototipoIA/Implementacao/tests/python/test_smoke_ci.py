"""Contrato do smoke de CI, com inventário explícito independente do motor."""
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[2] / "tools" / "validar_smoke_ci.py"
spec = importlib.util.spec_from_file_location("smoke_ci", MODULE)
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)


class SmokeCoverageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name)
        (self.out / "tuplas_motor").mkdir()
        self.rows = []
        # Inventário escolhido para o CI; não importar constantes do verificador.
        for policy in ("LRU", "MRU"):
            for operation in (
                "sel_pt_dre", "sel_pt_cpf", "sel_rg_dre_baixa",
                "sel_rg_cr_baixa", "sel_rg_curso_baixa", "sel_rg_dre_alta",
                "sel_rg_cr_alta", "sel_rg_curso_alta",
            ):
                self.add_row(policy, operation, "")
            for operation in ("j_mestre_detalhe", "j_pequenas", "j_grande_pequena"):
                for algorithm in ("bnl", "hash", "sort_merge"):
                    self.add_row(policy, operation, algorithm)

    def add_row(self, policy, operation, algorithm):
        self.rows.append(dict(politica=policy, operacao=operation,
                              variante="heap:scan", algoritmo=algorithm,
                              rep="1", status="ok"))
        # O CSV deixa algoritmo vazio nas seleções; o dump usa o sufixo 'sel'.
        dump_algorithm = algorithm or "sel"
        filename = f"{policy}__{operation}__heap_scan_{dump_algorithm}__1.txt"
        (self.out / "tuplas_motor" / filename).write_text("42\n")

    def write_results(self):
        with (self.out / "resultados.csv").open("w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=[
                "politica", "operacao", "variante", "algoritmo", "rep", "status"])
            writer.writeheader()
            writer.writerows(self.rows)

    def validate(self):
        self.write_results()
        return smoke.validate(self.out)

    def test_complete_profile_passes(self):
        self.assertEqual(self.validate(), 34)

    def test_optional_unexecuted_rows_are_allowed(self):
        self.rows.append(dict(politica="LRU", operacao="optional", variante="x",
                              algoritmo="hash", rep="1", status="inviavel"))
        self.assertEqual(self.validate(), 34)

    def test_every_required_scenario_is_required(self):
        original = self.rows.copy()
        for row in original:
            with self.subTest(row=row):
                self.rows = [other for other in original if other is not row]
                with self.assertRaises(ValueError):
                    self.validate()

    def test_all_skipped_is_rejected(self):
        for row in self.rows:
            row["status"] = "nao_aplicavel"
        with self.assertRaises(ValueError):
            self.validate()

    def test_empty_results_are_rejected(self):
        self.rows = []
        with self.assertRaises(ValueError):
            self.validate()

    def test_missing_dump_is_rejected(self):
        next((self.out / "tuplas_motor").iterdir()).unlink()
        with self.assertRaises(ValueError):
            self.validate()

    def test_duplicate_execution_is_rejected(self):
        self.rows.append(self.rows[0].copy())
        with self.assertRaises(ValueError):
            self.validate()

    def test_unknown_status_is_rejected(self):
        self.rows[0]["status"] = "erro"
        with self.assertRaises(ValueError):
            self.validate()

    def test_missing_fields_are_rejected(self):
        (self.out / "resultados.csv").write_text("status\nok\n")
        with self.assertRaises(ValueError):
            smoke.validate(self.out)

    def test_bad_repetition_is_rejected(self):
        self.rows[0]["rep"] = "../escape"
        with self.assertRaises(ValueError):
            self.validate()


if __name__ == "__main__":
    unittest.main()

"""BENCH-006: a conferência com o SQLite conta as combinações verificadas e as ausentes, e um
resultado ausente nunca passa como conferido."""
import csv
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "compare.py"
CAMPOS = ["politica", "operacao", "variante", "algoritmo", "rep", "status", "linhas", "checksum"]


class CompareTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name)
        (self.out / "tuplas_motor").mkdir()
        (self.out / "tuplas_ref").mkdir()
        self.rows = [
            {"politica": p, "operacao": "j_x", "variante": "a_heap:scan x b_heap:scan", "algoritmo": alg, "rep": "1",
             "status": "ok", "linhas": "2", "checksum": "10"}
            for p in ("LRU", "MRU") for alg in ("bnl", "hash")]
        self.rows.append({"politica": "LRU", "operacao": "sel_x", "variante": "a_hash:primary_hash", "algoritmo": "",
                          "rep": "1", "status": "ok", "linhas": "1", "checksum": "7"})
        self.rows.append({"politica": "LRU", "operacao": "sel_x", "variante": "a_hash:primary_hash (faixa)",
                          "algoritmo": "", "rep": "1", "status": "nao_aplicavel", "linhas": "", "checksum": ""})
        self.ref = [("j_x", "*", "2", "10"), ("sel_x", "1", "1", "7")]
        self.ops = ["j_x", "sel_x"]
        self.dumps = True
        self.plano = None

    def nome(self, r):
        v = "".join(c if c.isalnum() else "_" for c in r["variante"])
        return "%s__%s__%s_%s__%s.txt" % (r["politica"], r["operacao"], v, r["algoritmo"] or "sel", r["rep"])

    def roda(self, *args):
        with open(self.out / "resultados.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=CAMPOS)
            w.writeheader()
            w.writerows(self.rows)
        with open(self.out / "referencia.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["operacao", "rep", "linhas", "checksum"])
            w.writerows(self.ref)
        with open(self.out / "operacoes.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["operacao", "tipo", "sql", "lote", "tipo_lote"])
            w.writerows([[op, "x", "", "", ""] for op in self.ops])
        if self.dumps:
            for r in self.rows:
                if r["status"] == "ok":
                    (self.out / "tuplas_motor" / self.nome(r)).write_text("aa\nbb\n")
            (self.out / "tuplas_ref" / "j_x_todas.txt").write_text("bb\naa\n")
            (self.out / "tuplas_ref" / "sel_x_1.txt").write_text("aa\nbb\n")
        if self.plano is not None:
            with (self.out / "configuracoes.csv").open("w", newline="") as f:
                fields = ["politica", "operacao", "variante", "algoritmo"]
                w = csv.DictWriter(f, fieldnames=fields)
                w.writeheader()
                w.writerows({k: r[k] for k in fields} for r in self.plano)
        p = subprocess.run([sys.executable, "-I", str(SCRIPT), str(self.out), *args], capture_output=True, text=True)
        return p.returncode, p.stdout

    def test_tudo_conferido(self):
        code, out = self.roda("--tuplas")
        self.assertEqual(code, 0, out)
        self.assertIn("conferidas: 5 linhas medidas em 5 combinações", out)
        self.assertIn("nao_aplicavel: 1; ausentes: 0; divergências: 0", out)

    def test_linha_sem_referencia_e_ausente(self):
        self.ref = [("j_x", "*", "2", "10")]
        self.ops = ["j_x"]
        code, out = self.roda()
        self.assertEqual(code, 1)
        self.assertIn("ausentes: 1", out)
        self.assertIn("sem referência do SQLite", out)

    def test_operacao_declarada_sem_linha_e_ausente(self):
        self.ops.append("j_y")
        code, out = self.roda()
        self.assertEqual(code, 1)
        self.assertIn("AUSENTE  j_y", out)

    def test_dump_ausente(self):
        self.dumps = False
        code, out = self.roda("--tuplas")
        self.assertEqual(code, 1)
        self.assertIn("ausentes: 5", out)
        self.assertIn("sem o dump", out)
        self.assertEqual(self.roda()[0], 0)   # sem --tuplas, os dumps não são exigidos

    def test_dump_sem_referencia(self):
        code, _ = self.roda("--tuplas")
        self.assertEqual(code, 0)
        (self.out / "tuplas_ref" / "sel_x_1.txt").unlink()
        self.dumps = False   # mantém os arquivos já gravados, sem regravar a referência
        code, out = self.roda("--tuplas")
        self.assertEqual(code, 1)
        self.assertIn("dump do motor sem a referência", out)

    def test_divergencias(self):
        self.rows[0]["checksum"] = "11"
        code, out = self.roda()
        self.assertEqual(code, 1)
        self.assertIn("em 4 combinações", out)
        self.assertIn("divergências: 1", out)
        self.rows[0]["checksum"] = "10"
        code, _ = self.roda("--tuplas")
        self.assertEqual(code, 0)
        (self.out / "tuplas_ref" / "j_x_todas.txt").write_text("aa\ncc\n")
        self.dumps = False
        code, out = self.roda("--tuplas")
        self.assertEqual(code, 1)
        self.assertIn("tuplas diferem", out)
        self.assertIn("em 1 combinações", out)

    def test_diretorio_vazio_reprova(self):
        result = subprocess.run([sys.executable, "-I", str(SCRIPT), str(self.out)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)

    def test_nenhuma_medicao_reprova(self):
        for r in self.rows:
            r["status"] = "inviavel"
        self.assertEqual(self.roda()[0], 1)

    def test_combinacao_declarada_ausente_reprova(self):
        self.plano = list(self.rows)
        self.rows = self.rows[1:]
        code, out = self.roda()
        self.assertEqual(code, 1)
        self.assertIn("combinação declarada sem execução", out)


if __name__ == "__main__":
    unittest.main()

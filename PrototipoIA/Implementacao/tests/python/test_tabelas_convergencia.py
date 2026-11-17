"""BENCH-007: a tabela de convergência mostra os grupos que pararam no teto sem convergir."""
import importlib.util
from pathlib import Path
import unittest

MODULE = Path(__file__).resolve().parents[2] / "tools" / "tabelas.py"
spec = importlib.util.spec_from_file_location("tabelas", MODULE)
tabelas = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tabelas)


class Base:
    """O mínimo da Base do tabelas.py que t_convergencia usa."""

    def __init__(self, linhas):
        self.convergencia = linhas
        self.manifesto = {"tolerancia": "0.05"}
        self.combos = [{"operacao": "j_x", "tipo": "juncao"}]

    def rotulo_op(self, operacao, tipo):
        return f"{operacao} ({tipo})"


def linha(parada, io="sim", tempo="sim", reps="3", alg="bnl"):
    return {"politica": "LRU", "operacao": "j_x", "variante": "a x b", "algoritmo": alg, "n_repeticoes": reps,
            "dispersao_io": "0.0100", "dispersao_tempo": "0.2000", "convergiu_io": io, "convergiu_tempo": tempo,
            "parada": parada}


class ConvergenciaTest(unittest.TestCase):
    def test_teto_aparece_e_e_contado(self):
        b = Base([linha("convergiu"), linha("teto", io="sim", tempo="nao", reps="10"),
                  linha("teto", io="nao", tempo="nao", reps="10", alg="hash"), linha("nao_executada")])
        t = tabelas.t_convergencia(b)
        self.assertEqual(len(t.linhas), 2)
        self.assertTrue(all(row[4] == "10" for row in t.linhas))
        self.assertIn("3 grupos executados: 1 convergiram", t.texto)
        self.assertIn("2 pararam no teto", t.texto)
        self.assertIn("1 só no tempo", t.texto)

    def test_serie_estavel_nao_aparece(self):
        t = tabelas.t_convergencia(Base([linha("convergiu"), linha("convergiu", alg="hash")]))
        self.assertEqual(t.linhas, [])
        self.assertIn("0 pararam no teto", t.texto)


if __name__ == "__main__":
    unittest.main()

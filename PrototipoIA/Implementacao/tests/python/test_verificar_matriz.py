"""BENCH-005: o verificador da matriz obrigatória aprova a matriz completa e reprova as lacunas.

As rodadas são sintéticas: um resultados.csv com todas as células obrigatórias, nas duas políticas,
mais o estatisticas.csv e o manifesto. Cada teste negativo estraga uma coisa.
"""
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[2] / "tools"


def carregar(nome):
    spec = importlib.util.spec_from_file_location(nome, TOOLS / f"{nome}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


verificador = carregar("verificar_matriz")

CAMPOS = ["politica", "operacao", "tipo", "tamanho", "config", "seletividade", "variante", "algoritmo",
          "externa", "interna", "m", "rep", "status", "motivo"]
# blocos de cada tabela com m = 32: "menor" se b <= 30
BLOCOS = {"curso": 16, "disciplina": 9, "aluno": 952, "inscricao": 1695}
PARES = {"menor_menor": ("curso", "disciplina"), "menor_maior": ("curso", "aluno"),
         "maior_maior": ("aluno", "inscricao")}


def linha(pol, tipo, tamanho, config, selet, alg, status="ok", motivo="", externa="", interna="", m="32"):
    return {"politica": pol, "operacao": f"op_{tipo}_{tamanho}_{selet}", "tipo": tipo, "tamanho": tamanho,
            "config": config, "seletividade": selet, "variante": f"v_{config}", "algoritmo": alg,
            "externa": externa, "interna": interna, "m": m, "rep": "1", "status": status, "motivo": motivo}


def matriz_completa():
    rows = []
    for pol in ("LRU", "MRU"):
        for tam, (a, b) in PARES.items():
            for cfg in "abcde":
                for alg in ("bnl", "sort_merge", "hash"):
                    rows.append(linha(pol, "natural_join", tam, cfg, "", alg, externa=a + "_" + tam + "_" + cfg, interna=b + "_" + tam + "_" + cfg))
        for cfg in "abc":
            rows.append(linha(pol, "igualdade_unique", "", cfg, "pontual", "", externa="aluno_sel_" + cfg, m="2"))
            for s in ("1%", "50%"):
                rows.append(linha(pol, "igualdade_unique", "", cfg, s, "", status="nao_aplicavel",
                                  motivo="caso 3: igualdade em UNIQUE devolve 1 registro", m="2"))
                rows.append(linha(pol, "faixa_not_unique", "", cfg, s, "", externa="aluno_sel_" + cfg, m="2"))
    return [dict(r, rep=str(rep)) for r in rows for rep in (1, 2, 3)]


class VerificadorTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name)
        self.rows = matriz_completa()
        self.blocos = dict(BLOCOS)
        self.manifesto = {"escala": "1", "reps": "3..10"}
        self.configs, self.metodos = {}, {}
        roles = {"a": "NN", "b": "PN", "c": "PP", "d": "PS", "e": "SS"}
        for r in self.rows:
            if r["status"] != "ok":
                continue
            key = (r["politica"], r["operacao"], r["variante"], r["algoritmo"])
            self.configs[key] = dict(zip(
                ("politica", "operacao", "variante", "algoritmo"), key),
                externa=r["externa"], campo_externa="k", interna=r["interna"],
                campo_interna="k" if r["interna"] else "")
            kinds = roles[r["config"]] if r["algoritmo"] else {"a": "N", "b": "P", "c": "S"}[r["config"]]
            for table, kind in zip((r["externa"], r["interna"]), kinds):
                self.metodos[(r["politica"], table, "k", "scan")] = True
                if kind != "N":
                    method = "primary_btree" if kind == "P" else "secondary_btree"
                    self.metodos[(r["politica"], table, "k", method)] = True

    def roda(self, aceitar_reduzida=False):
        with open(self.out / "resultados.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=CAMPOS)
            w.writeheader()
            w.writerows(self.rows)
        with open(self.out / "estatisticas.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["politica", "tabela", "estrutura", "b"])
            for pol in ("LRU", "MRU"):
                tables = {c[t] for c in self.configs.values() for t in ("externa", "interna") if c[t]}
                for t in sorted(tables):
                    base = next(name for name in BLOCOS if t.startswith(name + "_"))
                    w.writerow([pol, t, t + ":heap", self.blocos[base]])
        with (self.out / "configuracoes.csv").open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["politica", "operacao", "variante", "algoritmo",
                                            "externa", "campo_externa", "interna", "campo_interna"])
            w.writeheader()
            w.writerows(self.configs.values())
        with (self.out / "metodos.csv").open("w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["politica", "tabela", "campo", "metodo"])
            w.writerows(self.metodos)
        (self.out / "manifesto.txt").write_text("".join(f"{k}: {v}\n" for k, v in self.manifesto.items()),
                                               encoding="utf-8")
        return verificador.verificar(str(self.out), aceitar_reduzida)

    def test_matriz_completa_aprova(self):
        falhas, avisos, resumo = self.roda()
        self.assertEqual(falhas, [])
        self.assertIn("60 células por política", resumo)
        self.assertIn("0 ausentes", resumo)

    def test_omitir_uma_celula_reprova(self):
        self.rows = [r for r in self.rows
                     if not (r["politica"] == "MRU" and r["tipo"] == "natural_join" and r["tamanho"] == "maior_maior"
                             and r["config"] == "e" and r["algoritmo"] == "hash")]
        falhas, _, _ = self.roda()
        self.assertEqual(len(falhas), 1)
        self.assertIn("ausente", falhas[0])
        self.assertIn("MRU natural_join maior_maior config. e hash", falhas[0])

    def test_juncao_obrigatoria_nao_aplicavel_reprova(self):
        for r in self.rows:
            if r["tipo"] == "natural_join" and r["tamanho"] == "menor_menor" and r["config"] == "b":
                r["status"], r["motivo"] = "nao_aplicavel", "caso 1: o tipo exige campos que não ditam"
        falhas, _, _ = self.roda()
        self.assertTrue(falhas)
        self.assertTrue(all("sem fundamento" in f for f in falhas))

    def test_selecao_nao_aplicavel_fora_do_caso_3_reprova(self):
        for r in self.rows:
            if r["tipo"] == "faixa_not_unique" and r["config"] == "c" and r["seletividade"] == "1%":
                r["status"], r["motivo"] = "nao_aplicavel", "caso 4: faixa no hash primário"
        falhas, _, _ = self.roda()
        self.assertEqual(len(falhas), 2)   # LRU e MRU
        self.assertTrue(all("faixa_not_unique" in f for f in falhas))

    def test_variante_nao_aplicavel_com_outra_executada_aprova(self):
        extra = [dict(r, variante="v_hash_primario", status="nao_aplicavel", motivo="caso 4: faixa no hash primário")
                 for r in self.rows if r["tipo"] == "faixa_not_unique" and r["config"] == "c"]
        self.rows += extra
        self.assertEqual(self.roda()[0], [])

    def test_inviavel_aparece_mas_nao_reprova(self):
        for r in self.rows:
            if r["tipo"] == "natural_join" and r["tamanho"] == "maior_maior" and r["algoritmo"] == "sort_merge" \
                    and r["config"] == "a":
                r["status"], r["motivo"] = "inviavel", "não cabe em M: grupo de duplicatas"
        falhas, avisos, _ = self.roda()
        self.assertEqual(falhas, [])
        self.assertEqual(len(avisos), 2)

    def test_porte_falso_reprova(self):
        self.blocos["aluno"] = 20   # Aluno caberia no chunk: "maior" não confere
        falhas, _, _ = self.roda()
        self.assertTrue(falhas)
        self.assertTrue(all("porte não confere" in f for f in falhas))

    def test_rodada_reduzida_nao_prova_a_matriz(self):
        self.manifesto = {"escala": "0.1", "reps": "1..1"}
        falhas, _, _ = self.roda()
        self.assertEqual(len(falhas), 2)
        self.assertEqual(self.roda(aceitar_reduzida=True)[0], [])

    def test_politica_ausente_reprova(self):
        self.rows = [r for r in self.rows if r["politica"] == "LRU"]
        falhas, _, _ = self.roda()
        self.assertEqual(falhas, ["política MRU ausente do resultados.csv"])

    def test_scan_em_tabela_indexada_nao_prova_config_a(self):
        self.metodos[("LRU", "curso_menor_menor_a", "k", "secondary_hash")] = True
        self.assertTrue(any("configuração física" in f for f in self.roda()[0]))

    def test_indice_em_outro_campo_nao_muda_config_a(self):
        self.metodos[("LRU", "curso_menor_menor_a", "outro", "secondary_hash")] = True
        self.assertEqual(self.roda()[0], [])

    def test_config_b_com_secundario_na_outra_tabela_reprova(self):
        self.metodos[("LRU", "disciplina_menor_menor_b", "k", "secondary_btree")] = True
        self.assertTrue(any("configuração física" in f for f in self.roda()[0]))

    def test_config_c_sem_primarias_reprova(self):
        self.metodos.pop(("LRU", "curso_menor_menor_c", "k", "primary_btree"))
        self.assertTrue(any("configuração física" in f for f in self.roda()[0]))

    def test_selecao_a_indexada_reprova(self):
        self.metodos[("LRU", "aluno_sel_a", "k", "secondary_hash")] = True
        self.assertTrue(any("configuração física" in f for f in self.roda()[0]))

    def test_sem_metadados_fisicos_reprova(self):
        self.configs.clear()
        self.assertTrue(any("metadados" in f for f in self.roda()[0]))

    def test_repeticao_unica_reprova_ainda_com_manifesto_tres(self):
        self.rows = [r for r in self.rows if r["rep"] == "1"]
        self.assertTrue(any("repetições executadas" in f for f in self.roda()[0]))

    def test_tudo_inviavel_sem_motivo_reprova(self):
        for r in self.rows:
            if r["status"] == "ok":
                r["status"], r["motivo"] = "inviavel", ""
        self.assertTrue(self.roda()[0])

    def test_inventario(self):
        matriz = carregar("matriz")
        cel = matriz.obrigatorias()
        self.assertEqual(len(cel), 45 + 9 + 6)
        self.assertEqual(len(set(cel)), len(cel))


if __name__ == "__main__":
    unittest.main()

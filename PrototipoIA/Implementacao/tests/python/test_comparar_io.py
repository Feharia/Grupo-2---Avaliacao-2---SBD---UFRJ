"""Expectativas do relatório de I/O, independentes dos algoritmos do motor."""
import copy
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

path = Path(__file__).resolve().parents[2] / "tools/comparar_io.py"
spec = importlib.util.spec_from_file_location("comparar_io", path)
io = importlib.util.module_from_spec(spec)
spec.loader.exec_module(io)


def sample():
    return {
        "politica": "LRU", "operacao": "sel_pt_dre", "tipo": "pontual",
        "tamanho": "", "config": "a", "seletividade": "",
        "variante": "heap:scan", "algoritmo": "", "externa": "heap",
        "interna": "", "m": "2", "rep": "1", "status": "ok",
        "linhas": "100", "checksum": "18446744073709551615",
        "leituras": "10", "gravacoes": "0", "flush_writes": "0",
    }


def metadata():
    return {"protocolo": "io-rapido-v1", "commit": "a" * 40,
            "validado_sqlite_tuplas": True,
            "assinatura": {"dados": {"dados/a.csv": "abc"}, "config": {"block_size": 4096},
                           "manifesto": {"m_juncao": "32"}}, "tool_hash": "abc"}


class IoComparisonTest(unittest.TestCase):
    def test_inventory_rejects_same_variant_missing_on_both_sides(self):
        required = {("LRU", "sel_pt_dre", "heap:scan", ""),
                    ("LRU", "sel_pt_dre", "bt:primary", "")}
        rows = [sample(), sample() | {"variante": "bt:primary"}]
        io.ensure_inventory(rows, required)
        with self.assertRaises(ValueError):
            io.ensure_inventory(rows[:1], required)

    def test_real_compile_options_normalize_paths_but_preserve_flags(self):
        def entries(root, optimization):
            return [{"file": root + "/src/common/common.cpp",
                     "command": f"/usr/bin/g++ -I{root}/src {optimization} -DNDEBUG -o CMakeFiles/cbd_motor.dir/src/common/common.cpp.o -c {root}/src/common/common.cpp"}]
        a = io.compiler_options(entries("/tmp/antes", "-O3"), Path("/tmp/antes"))
        b = io.compiler_options(entries("/tmp/depois", "-O3"), Path("/tmp/depois"))
        self.assertEqual(a, b)
        self.assertNotEqual(a, io.compiler_options(entries("/tmp/depois", "-O0"), Path("/tmp/depois")))

    def compare(self, before=None, after=None, left=None, right=None):
        return io.compare_rows(before if before is not None else [sample()],
                               after if after is not None else [sample()],
                               left if left is not None else metadata(),
                               right if right is not None else metadata())

    def test_identical_has_zero_deltas(self):
        result = self.compare()
        self.assertEqual(result["cenarios"], 1)
        self.assertEqual(result["aumentos"], 0)
        self.assertEqual(result["linhas"][0]["leituras"],
                         {"antes": 10, "depois": 10, "delta": 0, "percentual": 0.0})

    def test_increase_warns_and_decrease_remains_visible(self):
        row = sample() | {"leituras": "8", "gravacoes": "3", "flush_writes": "2"}
        result = self.compare(after=[row])
        self.assertEqual(result["aumentos"], 1)
        metrics = result["linhas"][0]
        self.assertEqual(metrics["leituras"]["percentual"], -20.0)
        self.assertEqual(metrics["gravacoes"]["delta"], 3)
        self.assertIsNone(metrics["gravacoes"]["percentual"])
        self.assertEqual(metrics["total"]["depois"], 11)
        self.assertEqual(metrics["total_com_flush"]["depois"], 13)

    def test_large_integers_are_exact(self):
        before = sample() | {"leituras": str(2**60)}
        after = before | {"leituras": str(2**60 + 1)}
        self.assertEqual(self.compare([before], [after])["linhas"][0]["leituras"]["delta"], 1)
        with self.assertRaises(ValueError):
            self.compare(after=[sample() | {"checksum": "18446744073709551614"}])

    def test_missing_or_extra_scenario_rejected(self):
        with self.assertRaises(ValueError):
            self.compare(after=[])
        with self.assertRaises(ValueError):
            self.compare(after=[sample(), sample() | {"variante": "bt:primary"}])

    def test_duplicates_rejected_on_either_side(self):
        with self.assertRaises(ValueError):
            self.compare(before=[sample(), sample()])
        with self.assertRaises(ValueError):
            self.compare(after=[sample(), sample()])

    def test_skipped_scenario_and_status_changes_rejected(self):
        for status in ("inviavel", "nao_aplicavel", "erro"):
            with self.subTest(status=status), self.assertRaises(ValueError):
                self.compare(after=[sample() | {"status": status}])
        skipped = sample() | {"operacao": "optional", "status": "nao_aplicavel"}
        self.assertEqual(self.compare([sample(), skipped], [sample(), skipped])["cenarios"], 1)

    def test_empty_success_set_rejected(self):
        row = sample() | {"status": "inviavel"}
        with self.assertRaises(ValueError):
            self.compare([row], [row])

    def test_wrong_results_or_m_rejected(self):
        for field, value in (("linhas", "99"), ("checksum", "1"), ("m", "3")):
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.compare(after=[sample() | {field: value}])

    def test_negative_or_noninteger_counter_rejected(self):
        for value in ("-1", "1.5", "", "abc"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.compare(after=[sample() | {"gravacoes": value}])

    def test_missing_column_rejected(self):
        row = sample()
        del row["flush_writes"]
        with self.assertRaises(ValueError):
            self.compare(after=[row])

    def test_different_input_configuration_or_tool_rejected(self):
        for name in ("dados", "config", "manifesto"):
            right = copy.deepcopy(metadata())
            right["assinatura"][name] = {"diferente": 1}
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.compare(right=right)
        with self.assertRaises(ValueError):
            self.compare(right=metadata() | {"tool_hash": "outro"})

    def test_unvalidated_or_unknown_protocol_rejected(self):
        for change in ({"validado_sqlite_tuplas": False}, {"protocolo": "v2"}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.compare(left=metadata() | change)

    def test_different_commit_is_expected(self):
        self.assertEqual(self.compare(right=metadata() | {"commit": "b" * 40})["cenarios"], 1)

    def measurement(self, root, name, row):
        directory = root / name
        (directory / "smoke").mkdir(parents=True)
        csv_path = directory / "smoke/resultados.csv"
        with csv_path.open("w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(row))
            writer.writeheader()
            inventory_path = path.with_name("io_cenarios_v1.json")
            for n, key in enumerate(json.loads(inventory_path.read_text())):
                fixture = (row if n == 0 else sample()) | dict(zip(
                    ("politica", "operacao", "variante", "algoritmo"), key))
                writer.writerow(fixture)
        meta = metadata() | {"resultados_sha256": hashlib.sha256(csv_path.read_bytes()).hexdigest(),
                             "inventario_sha256": hashlib.sha256(inventory_path.read_bytes()).hexdigest()}
        (directory / "medicao.json").write_text(json.dumps(meta))
        return directory

    def test_changed_csv_after_validation_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = self.measurement(Path(temporary), "antes", sample())
            (directory / "smoke/resultados.csv").write_text("alterado")
            with self.assertRaises(ValueError):
                io.read_measurement(directory)

    def test_versioned_inventory_rejects_same_omission_on_both_sides(self):
        with tempfile.TemporaryDirectory() as temporary:
            for name in ("antes", "depois"):
                directory = self.measurement(Path(temporary), name, sample())
                csv_path = directory / "smoke/resultados.csv"
                lines = csv_path.read_text().splitlines(keepends=True)
                csv_path.write_text("".join(lines[:1] + lines[2:]))
                meta_path = directory / "medicao.json"
                meta = json.loads(meta_path.read_text())
                meta["resultados_sha256"] = hashlib.sha256(csv_path.read_bytes()).hexdigest()
                meta_path.write_text(json.dumps(meta))
                with self.subTest(name=name), self.assertRaises(ValueError):
                    io.read_measurement(directory)

    def test_cli_warns_without_failing_and_never_overwrites(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            before = self.measurement(root, "antes", sample())
            after = self.measurement(root, "depois", sample() | {"gravacoes": "1"})
            output = root / "relatorio"
            command = [sys.executable, "-I", str(path), str(before), str(after), str(output)]
            result = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads((output / "comparacao.json").read_text())["aumentos"], 1)
            original = (output / "resumo.md").read_bytes()
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 1)
            self.assertEqual((output / "resumo.md").read_bytes(), original)


if __name__ == "__main__":
    unittest.main()

"""Ocupação exportada deve distinguir uso, folga reservada e comprimento lógico."""
import csv
import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
from types import SimpleNamespace
import unittest

SOURCE = Path(__file__).resolve().parents[2]
BINARY = Path(os.environ.get("BUILD_DIR", SOURCE / "build")) / "bancada"
spec = importlib.util.spec_from_file_location("tabelas_ocupacao", SOURCE / "tools/tabelas.py")
tabelas = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tabelas)


@unittest.skipUnless(BINARY.is_file(), "defina BUILD_DIR para o build CMake da bancada")
class OccupancyCliTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(temporary.cleanup)
        cls.out = Path(temporary.name) / "saida"
        result = subprocess.run([str(BINARY), "--rapido", "--so", "j_pequenas", "--politicas", "lru",
                                 "--saida", str(cls.out)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        with (cls.out / "ocupacao.csv").open(newline="") as file:
            cls.rows = list(csv.DictReader(file))

    def test_csv_reconcilia_arquivo_heap_com_bitmap(self):
        for table in ("curso_heap_sem_indices", "disciplina_heap_sem_indices"):
            with self.subTest(table=table):
                row = next(r for r in self.rows if r["tabela"] == table and r["momento"] == "depois")
                self.assertTrue({"usados", "livres_reservados", "total_arquivo"} <= row.keys())
                files = [p for p in (self.out / "motor_LRU").iterdir()
                         if re.fullmatch(re.escape(table) + r"_\d+\.heap", p.name)]
                self.assertEqual(len(files), 1)
                with files[0].open("rb") as file:
                    header = file.read(4096)
                # Formato de arquivo declarado: tamanho aos 8 B, bitmap a partir dos 24 B.
                block_bytes = struct.unpack_from("<I", header, 8)[0]
                total = files[0].stat().st_size // block_bytes
                used = sum(bool(header[24 + b // 8] & (1 << (b % 8))) for b in range(total))
                self.assertEqual(int(row["total_arquivo"]), total)
                self.assertEqual(int(row["usados"]), used)
                self.assertEqual(int(row["livres_reservados"]), total - used)
                self.assertGreater(total, used)

    def test_todas_as_estruturas_exportam_as_tres_quantidades(self):
        for row in self.rows:
            with self.subTest(structure=row["estrutura"], phase=row["momento"]):
                self.assertTrue({"usados", "livres_reservados", "total_arquivo"} <= row.keys())
                used, free, total = (int(row[k]) for k in ("usados", "livres_reservados", "total_arquivo"))
                self.assertGreater(used, 0)
                self.assertGreaterEqual(free, 0)
                self.assertEqual(used + free, total)


class OccupancyTableTest(unittest.TestCase):
    def table(self, before, after):
        rows = []
        for phase, data in (("antes", before), ("depois", after)):
            rows.append({"politica": "LRU", "momento": phase, "estrutura": "t:heap", "overflow": "0", **data})
        return tabelas.t_ocupacao(SimpleNamespace(ocup=rows, politica="LRU"))

    def test_total_novo_inclui_folga_do_extent(self):
        t = self.table({"dados": "1", "total": "2", "usados": "2", "livres_reservados": "7", "total_arquivo": "9"},
                       {"dados": "9", "total": "10", "usados": "10", "livres_reservados": "7", "total_arquivo": "17"})
        self.assertEqual(t.brutas[0][5:7], [9, 17])

    def test_ocupacao_muda_dentro_do_mesmo_extent(self):
        t = self.table({"dados": "1", "total": "2", "usados": "2", "livres_reservados": "7", "total_arquivo": "9"},
                       {"dados": "2", "total": "3", "usados": "3", "livres_reservados": "6", "total_arquivo": "9"})
        self.assertEqual(len(t.brutas), 1)
        self.assertEqual(t.brutas[0][5:7], [9, 9])

    def test_csv_historico_continua_legivel(self):
        t = self.table({"dados": "1", "total": "2"}, {"dados": "9", "total": "10"})
        self.assertEqual(t.brutas[0][5:7], [2, 10])

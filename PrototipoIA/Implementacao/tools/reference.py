#!/usr/bin/env python3
"""Reproduz cada operação da bancada no SQLite e grava referencia.csv (decisão 147).

Uso: python3 -I tools/reference.py SAIDA [--tuplas]

Lê SAIDA/esquema.csv, SAIDA/dados/*.csv, SAIDA/operacoes.csv e SAIDA/lotes/*.csv.
Para cada operação (e cada repetição, nas pontuais) executa o SQL equivalente e calcula
linhas e checksum canônico, idêntico ao ResultSink (decisões 142 e 158):
  campo NULL  -> byte 0
  inteiro     -> byte 1 + int64 little-endian
  texto       -> byte 1 + u32 little-endian do tamanho + bytes UTF-8
  FNV-1a 64 por tupla; checksum = soma módulo 2^64.
Com --tuplas, grava também SAIDA/tuplas_ref/<operacao>_<rep>.txt (uma tupla em hexadecimal por linha).
Só usa a biblioteca padrão.
"""
import csv
import glob
import os
import sqlite3
import struct
import sys

FNV_BASIS = 14695981039346656037
FNV_PRIME = 1099511628211
MASK = (1 << 64) - 1


def fnv1a(data):
    h = FNV_BASIS
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & MASK
    return h


def encode(row):
    parts = []
    for v in row:
        if v is None:
            parts.append(b"\x00")
        elif isinstance(v, int):
            parts.append(b"\x01" + struct.pack("<q", v))
        else:
            s = v.encode("utf-8")
            parts.append(b"\x01" + struct.pack("<I", len(s)) + s)
    return b"".join(parts)


def convert(value, kind):
    if value == "\\N":
        return None
    return int(value) if kind == "int" else value


def load(conn, outdir):
    schema = {}
    with open(os.path.join(outdir, "esquema.csv"), newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            schema.setdefault(r["tabela"], []).append((r["campo"], r["tipo"]))
    for table, fields in schema.items():
        cols = ", ".join("%s %s" % (n, "INTEGER" if t == "int" else "TEXT") for n, t in fields)
        conn.execute("CREATE TABLE %s (%s)" % (table, cols))
        with open(os.path.join(outdir, "dados", table + ".csv"), newline="", encoding="utf-8") as f:
            reader = csv.reader(f)
            next(reader)
            rows = [[convert(v, fields[i][1]) for i, v in enumerate(r)] for r in reader]
        conn.executemany("INSERT INTO %s VALUES (%s)" % (table, ",".join("?" * len(fields))), rows)
    return schema


def run(conn, sql):
    rows = 0
    total = 0
    tuples = []
    for row in conn.execute(sql):
        enc = encode(row)
        total = (total + fnv1a(enc)) & MASK
        rows += 1
        tuples.append(enc.hex())
    return rows, total, tuples


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    outdir = sys.argv[1]
    want_tuples = "--tuplas" in sys.argv[2:]
    conn = sqlite3.connect(":memory:")
    load(conn, outdir)
    tdir = os.path.join(outdir, "tuplas_ref")
    if want_tuples:
        os.makedirs(tdir, exist_ok=True)
    with open(os.path.join(outdir, "operacoes.csv"), newline="", encoding="utf-8") as f:
        ops = list(csv.DictReader(f))
    with open(os.path.join(outdir, "referencia.csv"), "w", newline="", encoding="utf-8") as out:
        w = csv.writer(out)
        w.writerow(["operacao", "rep", "linhas", "checksum"])
        for op in ops:
            jobs = []
            if op["lote"]:
                for path in sorted(glob.glob(os.path.join(outdir, op["lote"] + "_*.csv"))):
                    rep = path.rsplit("_", 1)[1][:-4]
                    jobs.append((rep, path))
            else:
                jobs.append(("*", None))
            for rep, path in jobs:
                conn.execute("DROP TABLE IF EXISTS lote")
                if path:
                    kind = op["tipo_lote"]
                    conn.execute("CREATE TABLE lote (k %s)" % ("INTEGER" if kind == "int" else "TEXT"))
                    with open(path, newline="", encoding="utf-8") as f:
                        reader = csv.reader(f)
                        next(reader)
                        conn.executemany("INSERT INTO lote VALUES (?)", [(convert(r[0], kind),) for r in reader])
                rows, total, tuples = run(conn, op["sql"])
                w.writerow([op["operacao"], rep, rows, total])
                if want_tuples:
                    with open(os.path.join(tdir, "%s_%s.txt" % (op["operacao"], rep.replace("*", "todas"))), "w") as t:
                        t.write("\n".join(sorted(tuples)) + ("\n" if tuples else ""))
            print("referência:", op["operacao"])
    return 0


if __name__ == "__main__":
    sys.exit(main())

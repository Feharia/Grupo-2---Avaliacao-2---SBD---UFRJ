#!/usr/bin/env python3
"""Cruza resultados.csv com referencia.csv por operação e repetição (decisão 147).

Uso: python3 -I tools/compare.py SAIDA [--tuplas]

Termina com código 1 se alguma linha medida divergir da referência do SQLite.
Com --tuplas, compara também cada dump do motor (SAIDA/tuplas_motor/*.txt) com a
referência correspondente (SAIDA/tuplas_ref), como multiconjuntos ordenados (decisão 166).
"""
import csv
import glob
import os
import sys


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    outdir = sys.argv[1]
    ref = {}
    with open(os.path.join(outdir, "referencia.csv"), newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            ref[(r["operacao"], r["rep"])] = (int(r["linhas"]), int(r["checksum"]))
    checked = 0
    bad = []
    skipped = {}
    with open(os.path.join(outdir, "resultados.csv"), newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            if r["status"] != "ok":   # nao_aplicavel (casos 1 a 4) ou inviavel (não cabe em M), decisão 170
                skipped[r["status"]] = skipped.get(r["status"], 0) + 1
                continue
            key = (r["operacao"], r["rep"])
            if key not in ref:
                key = (r["operacao"], "*")
            if key not in ref:
                bad.append((r, "sem referência"))
                continue
            got = (int(r["linhas"]), int(r["checksum"]))
            checked += 1
            if got != ref[key]:
                bad.append((r, "motor %s x SQLite %s" % (got, ref[key])))
    if "--tuplas" in sys.argv[2:]:
        for path in sorted(glob.glob(os.path.join(outdir, "tuplas_motor", "*.txt"))):
            name = os.path.basename(path)[:-4]
            op, rep = name.split("__")[1], name.rsplit("__", 1)[1]
            refpath = os.path.join(outdir, "tuplas_ref", "%s_%s.txt" % (op, rep))
            if not os.path.exists(refpath):
                refpath = os.path.join(outdir, "tuplas_ref", "%s_todas.txt" % op)
            with open(path) as a, open(refpath) as b:
                mine, theirs = sorted(a.read().split()), sorted(b.read().split())
            if mine != theirs:
                only_m = len(set(mine) - set(theirs))
                only_r = len(set(theirs) - set(mine))
                bad.append(({"politica": "", "operacao": op, "variante": name, "algoritmo": "", "rep": rep},
                            "tuplas diferem: %d só no motor, %d só no SQLite" % (only_m, only_r)))
    others = ", ".join("%s: %d" % kv for kv in sorted(skipped.items())) or "nenhuma"
    print("conferidas: %d linhas medidas; sem execução: %s; divergências: %d" % (checked, others, len(bad)))
    for r, why in bad[:50]:
        print("  DIVERGE %s %s %s %s rep=%s: %s" % (r["politica"], r["operacao"], r["variante"], r["algoritmo"], r["rep"], why))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

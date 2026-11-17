#!/usr/bin/env python3
"""Cruza resultados.csv com referencia.csv por operação e repetição (decisão 147).

Uso: python3 -I tools/compare.py SAIDA [--tuplas]

Termina com código 1 se alguma linha medida divergir da referência do SQLite ou se faltar
alguma coisa para conferir (BENCH-006). Conta como ausente:
- uma operação declarada (operacoes.csv ou referencia.csv) sem nenhuma linha no resultados.csv;
- uma linha medida (status ok) sem referência do SQLite;
- com --tuplas, uma linha medida sem o dump das tuplas do motor, ou um dump sem a referência.
Resultado ausente nunca conta como conferência bem-sucedida. A completude da matriz do enunciado é
papel do tools/verificar_matriz.py; aqui a conta é relativa ao que a rodada declarou, e por isso
vale também para o smoke reduzido do CI.

Com --tuplas, compara também cada dump do motor (SAIDA/tuplas_motor/*.txt) com a referência
correspondente (SAIDA/tuplas_ref), como multiconjuntos ordenados (decisão 166).

Colunas: o motor emite, para cada par, os campos das duas relações, e o SQL de referência é
SELECT x.*, y.* ... ON x.a = y.b, com as duas colunas da junção. A conferência por checksum e por
multiconjunto vale para essa forma; o NATURAL JOIN do enunciado mostraria a coluna comum uma vez,
sem mudar o número de linhas.
"""
import csv
import glob
import os
import sys


def ler_csv(caminho):
    if not os.path.exists(caminho):
        return []
    with open(caminho, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def nome_dump(r):
    """O nome que a bancada dá ao dump (shape_for no bench.cpp): bytes não alfanuméricos viram '_'."""
    v = "".join(chr(b) if b < 128 and chr(b).isalnum() else "_" for b in r["variante"].encode("utf-8"))
    return "%s__%s__%s_%s__%s.txt" % (r["politica"], r["operacao"], v, r["algoritmo"] or "sel", r["rep"])


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    outdir = sys.argv[1]
    tuplas = "--tuplas" in sys.argv[2:]
    ref = {}
    for r in ler_csv(os.path.join(outdir, "referencia.csv")):
        ref[(r["operacao"], r["rep"])] = (int(r["linhas"]), int(r["checksum"]))
    rows = ler_csv(os.path.join(outdir, "resultados.csv"))
    declaradas = {r["operacao"] for r in ler_csv(os.path.join(outdir, "operacoes.csv"))} | {op for op, _ in ref}

    checked = 0
    bad, ausentes = [], []
    skipped = {}
    combos_ok, combos_falha = set(), set()
    for r in rows:
        if r["status"] != "ok":   # nao_aplicavel (casos 1 a 4) ou inviavel (não cabe em M), decisão 170
            skipped[r["status"]] = skipped.get(r["status"], 0) + 1
            continue
        combo = (r["politica"], r["operacao"], r["variante"], r["algoritmo"])
        key = (r["operacao"], r["rep"])
        if key not in ref:
            key = (r["operacao"], "*")
        if key not in ref:
            ausentes.append((r, "linha medida sem referência do SQLite"))
            combos_falha.add(combo)
            continue
        got = (int(r["linhas"]), int(r["checksum"]))
        checked += 1
        if got != ref[key]:
            bad.append((r, "motor %s x SQLite %s" % (got, ref[key])))
            combos_falha.add(combo)
        else:
            combos_ok.add(combo)
    vazio = {"politica": "", "variante": "", "algoritmo": "", "rep": ""}
    for op in sorted(declaradas - {r["operacao"] for r in rows}):
        ausentes.append((dict(vazio, operacao=op), "operação declarada sem nenhuma linha no resultados.csv"))

    if tuplas:
        tdir, rdir = os.path.join(outdir, "tuplas_motor"), os.path.join(outdir, "tuplas_ref")
        existentes = {os.path.basename(p) for p in glob.glob(os.path.join(tdir, "*.txt"))}
        for r in rows:
            if r["status"] == "ok" and nome_dump(r) not in existentes:
                ausentes.append((r, "linha medida sem o dump das tuplas do motor"))
                combos_falha.add((r["politica"], r["operacao"], r["variante"], r["algoritmo"]))
        for name in sorted(existentes):
            op, rep = name[:-4].split("__")[1], name[:-4].rsplit("__", 1)[1]
            info = {"politica": name.split("__")[0], "operacao": op, "variante": name[:-4], "algoritmo": "", "rep": rep}
            refpath = os.path.join(rdir, "%s_%s.txt" % (op, rep))
            if not os.path.exists(refpath):
                refpath = os.path.join(rdir, "%s_todas.txt" % op)
            if not os.path.exists(refpath):
                ausentes.append((info, "dump do motor sem a referência do SQLite"))
                continue
            with open(os.path.join(tdir, name)) as a, open(refpath) as b:
                mine, theirs = sorted(a.read().split()), sorted(b.read().split())
            if mine != theirs:
                only_m = len(set(mine) - set(theirs))
                only_r = len(set(theirs) - set(mine))
                bad.append((info, "tuplas diferem: %d só no motor, %d só no SQLite" % (only_m, only_r)))

    others = ", ".join("%s: %d" % kv for kv in sorted(skipped.items())) or "nenhuma"
    print("conferidas: %d linhas medidas em %d combinações; sem execução: %s; ausentes: %d; divergências: %d"
          % (checked, len(combos_ok - combos_falha), others, len(ausentes), len(bad)))
    for r, why in ausentes[:50]:
        print("  AUSENTE %s %s %s %s rep=%s: %s" % (r["politica"], r["operacao"], r["variante"], r["algoritmo"], r["rep"], why))
    for r, why in bad[:50]:
        print("  DIVERGE %s %s %s %s rep=%s: %s" % (r["politica"], r["operacao"], r["variante"], r["algoritmo"], r["rep"], why))
    return 1 if bad or ausentes else 0


if __name__ == "__main__":
    sys.exit(main())

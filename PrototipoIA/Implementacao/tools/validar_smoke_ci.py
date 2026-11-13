#!/usr/bin/env python3
"""Exige cobertura mínima e dumps no smoke de CI; compare.py confere os valores.

Uso: python3 -I tools/validar_smoke_ci.py SAIDA
O contrato do perfil está documentado em docs/CI.md, na raiz do repositório.
"""
import csv
from pathlib import Path
import re
import sys

SELECTIONS = (
    "sel_pt_dre", "sel_pt_cpf", "sel_rg_dre_baixa", "sel_rg_cr_baixa",
    "sel_rg_curso_baixa", "sel_rg_dre_alta", "sel_rg_cr_alta", "sel_rg_curso_alta",
)
JOINS = ("j_mestre_detalhe", "j_pequenas", "j_grande_pequena")
ALGORITHMS = ("bnl", "hash", "sort_merge")
POLICIES = ("LRU", "MRU")


def validate(outdir):
    outdir = Path(outdir)
    required = {
        (policy, operation, algorithm)
        for policy in POLICIES
        for operation, algorithms in (
            [(op, ("",)) for op in SELECTIONS]
            + [(op, ALGORITHMS) for op in JOINS]
        )
        for algorithm in algorithms
    }
    covered, executions = set(), set()
    with (outdir / "resultados.csv").open(newline="", encoding="utf-8") as file:
        reader = csv.DictReader(file)
        fields = {"politica", "operacao", "variante", "algoritmo", "rep", "status"}
        if not fields.issubset(reader.fieldnames or []):
            raise ValueError("resultados.csv sem as colunas obrigatórias")
        for number, row in enumerate(reader, start=2):
            if any(row.get(field) is None for field in fields):
                raise ValueError(f"linha {number}: CSV incompleto")
            status = row["status"]
            if status not in ("ok", "nao_aplicavel", "inviavel"):
                raise ValueError(f"linha {number}: status inesperado {status!r}")
            if status != "ok":
                continue
            policy, operation = row["politica"], row["operacao"]
            variant, algorithm, rep = row["variante"], row["algoritmo"], row["rep"]
            if (policy not in POLICIES or not re.fullmatch(r"[A-Za-z0-9_]+", operation)
                    or algorithm not in ("", *ALGORITHMS) or not variant
                    or not re.fullmatch(r"[1-9][0-9]*", rep)):
                raise ValueError(f"linha {number}: identificação da execução inválida")
            key = (policy, operation, variant, algorithm, rep)
            if key in executions:
                raise ValueError(f"execução duplicada: {key}")
            executions.add(key)
            # Convenção pública dos dumps da bancada: variante não alfanumérica vira '_'.
            safe_variant = re.sub(r"[^A-Za-z0-9]", "_", variant)
            dump_algorithm = algorithm or "sel"
            dump = f"{policy}__{operation}__{safe_variant}_{dump_algorithm}__{rep}.txt"
            if not (outdir / "tuplas_motor" / dump).is_file():
                raise ValueError(f"dump de tuplas ausente: {dump}")
            covered.add((policy, operation, algorithm))
    missing = sorted(required - covered)
    if missing:
        raise ValueError(f"cenários obrigatórios sem execução ok: {missing}")
    return len(executions)


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    try:
        count = validate(sys.argv[1])
    except (OSError, ValueError, csv.Error) as error:
        print(f"Smoke inválido: {error}", file=sys.stderr)
        return 1
    print(f"Smoke: cobertura mínima completa; {count} execuções com dumps presentes")
    return 0


if __name__ == "__main__":
    sys.exit(main())

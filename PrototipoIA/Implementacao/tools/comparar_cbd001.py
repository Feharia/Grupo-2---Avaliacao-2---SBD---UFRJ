#!/usr/bin/env python3
"""Executa duas versões do mesmo driver, em ordem alternada, preservando CSV."""
import argparse
import csv
import json
import statistics
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("correcao", type=Path)
    parser.add_argument("saida", type=Path)
    parser.add_argument("--reps", type=int, default=10)
    args = parser.parse_args()
    if args.reps < 3:
        parser.error("são necessárias pelo menos três repetições")
    if args.saida.exists():
        parser.error("a pasta de saída deve ser nova, para preservar evidências")
    args.saida.mkdir(parents=True)
    metrics = ["rows", "valid", "height", "leaves", "internal_blocks"]
    for phase in ("load", "points", "scan"):
        metrics.extend(f"{phase}_{metric}" for metric in ("us", "requests", "reads", "writes"))
    samples = []
    cases = ((22, "fixed"), (32, "fixed"), (7518, "fixed"), (20000, "fixed"),
             (64, "variable"), (5, "variable"), (20000, "variable"))
    executables = {"baseline": args.baseline.resolve(), "correcao": args.correcao.resolve()}
    with (args.saida / "amostras.csv").open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=["variant", "rep", "layout", *metrics])
        writer.writeheader()
        for rep in range(args.reps):
            order = ("baseline", "correcao") if rep % 2 == 0 else ("correcao", "baseline")
            for rows, layout in cases:
                for variant in order:
                    with tempfile.TemporaryDirectory(prefix="cbd001-perf-") as directory:
                        result = subprocess.run(
                            [str(executables[variant]), directory, str(rows), layout],
                            text=True, capture_output=True, check=True,
                        )
                    values = next(csv.reader([result.stdout.strip()]))
                    if len(values) != len(metrics):
                        raise RuntimeError(f"CSV inválido: {result.stdout!r}")
                    sample = dict(zip(metrics, map(float, values)))
                    sample.update(variant=variant, rep=rep + 1, layout=layout)
                    if variant == "correcao" and sample["valid"] != 1:
                        raise RuntimeError("correção produziu árvore inválida")
                    writer.writerow(sample)
                    output.flush()
                    samples.append(sample)

    summary = {}
    lines = [
        "# CBD-001 — comparação de desempenho", "",
        f"{args.reps} repetições pareadas por tamanho; ordem alternada. Tempos em microssegundos.",
        "Contadores representam transferências arquivo↔buffer, não I/O físico do SSD.", "",
        "| Registros/layout | Fase | Mediana antes (µs) | Mediana depois (µs) | Delta | Leituras antes/depois | Escritas antes/depois |",
        "|---:|---|---:|---:|---:|---:|---:|",
    ]
    for rows, layout in cases:
        case = f"{rows}-{layout}"
        groups = {v: [s for s in samples if s["rows"] == rows and s["layout"] == layout and s["variant"] == v]
                  for v in executables}
        summary[case] = {}
        for variant, group in groups.items():
            data = {}
            for metric in metrics[1:]:
                values = [s[metric] for s in group]
                median = statistics.median(values)
                data[metric] = dict(median=median, min=min(values), max=max(values),
                                    cv_percent=100 * statistics.pstdev(values) / statistics.mean(values)
                                    if any(values) else 0)
            summary[case][variant] = data
        for phase in ("load", "points", "scan"):
            before = summary[case]["baseline"]
            after = summary[case]["correcao"]
            old = before[f"{phase}_us"]["median"]
            new = after[f"{phase}_us"]["median"]
            reads = f'{before[f"{phase}_reads"]["median"]:.0f}/{after[f"{phase}_reads"]["median"]:.0f}'
            writes = f'{before[f"{phase}_writes"]["median"]:.0f}/{after[f"{phase}_writes"]["median"]:.0f}'
            lines.append(f"| {case} | {phase} | {old:.2f} | {new:.2f} | {100 * (new / old - 1):+.1f}% | {reads} | {writes} |")
    lines.extend(["", "Variabilidade, mínimos e máximos: `resumo.json`; valores brutos: `amostras.csv`.",
                  "A baseline pode violar ocupação mínima; `valid` inclui checagem independente da última folha.",
                  "Resultados de pontos e varredura são conferidos em ambas as versões."])
    (args.saida / "resumo.md").write_text("\n".join(lines) + "\n")
    (args.saida / "resumo.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()

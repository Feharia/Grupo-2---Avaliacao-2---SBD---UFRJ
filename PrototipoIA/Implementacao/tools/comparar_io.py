#!/usr/bin/env python3
"""Compara medições produzidas por medir_io.py; aumentos são alertas, não falhas."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import shlex
import sys

KEY = ("politica", "operacao", "tipo", "tamanho", "config", "seletividade",
       "variante", "algoritmo", "externa", "interna", "m", "rep")
METRICS = ("leituras", "gravacoes", "total", "flush_writes", "total_com_flush")


def ensure_inventory(rows, required):
    actual = {tuple(row[field] for field in ("politica", "operacao", "variante", "algoritmo"))
              for row in rows if row["status"] == "ok"}
    if actual != required:
        raise ValueError(f"inventário io-rapido-v1 incompatível: {len(required - actual)} ausentes; {len(actual - required)} extras")


def compiler_options(entries, source):
    options = set()
    for entry in entries:
        command = entry.get("arguments") or shlex.split(entry["command"])
        if not any("CMakeFiles/cbd_motor.dir/" in token for token in command):
            continue
        filtered, skip = [], False
        for token in command:
            if skip:
                skip = False
                continue
            if token == "-o":
                skip = True
            elif token != entry["file"] and token != "-c":
                filtered.append(token.replace(str(source), "{SOURCE}"))
        options.add(tuple(filtered))
    if not options:
        raise ValueError("flags reais de cbd_motor não encontradas em compile_commands.json")
    return [list(option) for option in sorted(options)]


def integer(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9]+", value):
        raise ValueError(f"inteiro não negativo esperado: {value!r}")
    return int(value)


def index_rows(rows):
    indexed = {}
    for row in rows:
        if any(field not in row or row[field] is None for field in (*KEY, "status")):
            raise ValueError("CSV incompleto")
        key = tuple(row[field] for field in KEY)
        if key in indexed:
            raise ValueError(f"cenário duplicado: {key}")
        if row["status"] not in ("ok", "inviavel", "nao_aplicavel"):
            raise ValueError(f"status inesperado: {row['status']}")
        indexed[key] = row
    return indexed


def delta(before, after):
    return {"antes": before, "depois": after, "delta": after - before,
            "percentual": ((after - before) * 100 / before if before
                           else (0.0 if after == 0 else None))}


def compare_rows(before, after, left, right):
    for metadata in (left, right):
        if (metadata.get("protocolo") != "io-rapido-v1"
                or metadata.get("validado_sqlite_tuplas") is not True
                or not metadata.get("assinatura") or not metadata.get("tool_hash")):
            raise ValueError("metadados ausentes, protocolo desconhecido ou medição não validada")
    if left["assinatura"] != right["assinatura"] or left["tool_hash"] != right["tool_hash"]:
        raise ValueError("protocolo incompatível: dados, consultas, parâmetros, Config ou ferramentas diferentes")
    old, new = index_rows(before), index_rows(after)
    if old.keys() != new.keys():
        raise ValueError("inventário de cenários diferente entre referência e candidato")
    results = []
    for key in sorted(old):
        a, b = old[key], new[key]
        if a["status"] != b["status"]:
            raise ValueError(f"cenário mudou de status: {key}")
        if a["status"] != "ok":
            continue
        try:
            if any(integer(a[name]) != integer(b[name]) for name in ("linhas", "checksum")):
                raise ValueError(f"resultados diferentes: {key}")
            aa = {name: integer(a[name]) for name in ("leituras", "gravacoes", "flush_writes")}
            bb = {name: integer(b[name]) for name in ("leituras", "gravacoes", "flush_writes")}
        except KeyError as error:
            raise ValueError(f"coluna obrigatória ausente: {error}") from error
        for counts in (aa, bb):
            counts["total"] = counts["leituras"] + counts["gravacoes"]
            counts["total_com_flush"] = counts["total"] + counts["flush_writes"]
        row = dict(zip(KEY, key))
        row.update({name: delta(aa[name], bb[name]) for name in METRICS})
        row["aumento"] = any(row[name]["delta"] > 0 for name in METRICS)
        results.append(row)
    if not results:
        raise ValueError("nenhum cenário executado para comparar")
    return {"cenarios": len(results), "aumentos": sum(row["aumento"] for row in results),
            "referencia": left, "candidato": right, "linhas": results}


def read_measurement(directory):
    metadata = json.loads((directory / "medicao.json").read_text())
    csv_path = directory / "smoke/resultados.csv"
    if hashlib.sha256(csv_path.read_bytes()).hexdigest() != metadata.get("resultados_sha256"):
        raise ValueError("resultados.csv foi alterado depois da validação")
    with csv_path.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    profile = Path(__file__).with_name("io_cenarios_v1.json")
    if hashlib.sha256(profile.read_bytes()).hexdigest() != metadata.get("inventario_sha256"):
        raise ValueError("inventário versionado diferente do usado na medição")
    ensure_inventory(rows, {tuple(key) for key in json.loads(profile.read_text())})
    return rows, metadata


def markdown(report):
    lines = ["# Comparação de I/O", "",
             f"Referência: `{report['referencia']['commit']}`. Candidato: `{report['candidato']['commit']}`.",
             f"{report['cenarios']} cenários pareados; {report['aumentos']} com aumento em alguma métrica.",
             "Aumentos são alertas; contagens representam transferências arquivo↔buffer, não I/O físico do SSD.",
             "Total = leituras + escritas, antes do flush final. Carga e manutenção fora desta versão.", "",
             "| Política / operação / variante / algoritmo | Leituras antes→depois (Δ) | Escritas antes→depois (Δ) | Total Δ / % | Flush antes→depois |",
             "|---|---:|---:|---:|---:|"]
    for row in report["linhas"]:
        label = " / ".join(row[name] for name in ("politica", "operacao", "variante", "algoritmo")).replace("|", "\\|")
        reads, writes, total, flush = (row[name] for name in ("leituras", "gravacoes", "total", "flush_writes"))
        pct = "indefinido (base zero)" if total["percentual"] is None else f"{total['percentual']:+.2f}%"
        lines.append(f"| {label} | {reads['antes']}→{reads['depois']} ({reads['delta']:+d}) "
                     f"| {writes['antes']}→{writes['depois']} ({writes['delta']:+d}) "
                     f"| {total['delta']:+d} / {pct} | {flush['antes']}→{flush['depois']} |")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("referencia", type=Path)
    parser.add_argument("candidato", type=Path)
    parser.add_argument("saida", type=Path, help="diretório novo para CSV/JSON/Markdown")
    args = parser.parse_args()
    try:
        before, left = read_measurement(args.referencia)
        after, right = read_measurement(args.candidato)
        report = compare_rows(before, after, left, right)
        args.saida.mkdir(parents=True, exist_ok=False)
        (args.saida / "comparacao.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        (args.saida / "resumo.md").write_text(markdown(report))
        fields = [*KEY, "aumento", *(f"{name}_{part}" for name in METRICS
                                         for part in ("antes", "depois", "delta", "percentual"))]
        with (args.saida / "comparacao.csv").open("w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=fields, lineterminator="\n")
            writer.writeheader()
            for row in report["linhas"]:
                flat = {name: row[name] for name in (*KEY, "aumento")}
                flat.update({f"{name}_{part}": value for name in METRICS for part, value in row[name].items()})
                writer.writerow(flat)
        print(f"I/O: {report['cenarios']} cenários; {report['aumentos']} alertas de aumento")
        return 0
    except (OSError, ValueError, KeyError, csv.Error) as error:
        print(f"Comparação de I/O inválida: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Verifica se uma rodada cobre a matriz OBRIGATÓRIA do enunciado v2 (BENCH-005).

Uso: python3 -I tools/verificar_matriz.py SAIDA [--aceitar-reduzida]

Lê SAIDA/resultados.csv, SAIDA/estatisticas.csv e SAIDA/manifesto.txt. Termina com código 1 se:
- a rodada não for integral: escala menor que 1, menos de 3 repetições ou sem LRU e MRU (o smoke
  --rapido não prova a matriz; --aceitar-reduzida desliga só esta regra, para testes);
- faltar alguma célula obrigatória em alguma política (célula sem nenhuma linha no CSV);
- uma célula obrigatória vier não aplicável sem fundamento: junção obrigatória nunca é; seleção
  obrigatória só no caso 3 (igualdade em UNIQUE com 1% ou 50%);
- o porte de uma junção executada não conferir com os blocos reais das tabelas e o m da operação
  (menor: b ≤ m − 2; maior: b > m − 2).
Células inviáveis (não cabem em M) são resultado do experimento: aparecem no relatório, mas não
reprovam. O inventário e o estado das células vêm de tools/matriz.py, o mesmo da tabela de cobertura.
"""
import csv
import importlib.util
import os
import sys

_spec = importlib.util.spec_from_file_location("matriz", os.path.join(os.path.dirname(os.path.abspath(__file__)), "matriz.py"))
matriz = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(matriz)


def ler_csv(caminho):
    if not os.path.exists(caminho):
        return []
    with open(caminho, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def ler_manifesto(caminho):
    d = {}
    if os.path.exists(caminho):
        with open(caminho, encoding="utf-8") as f:
            for linha in f:
                if ":" in linha:
                    k, v = linha.split(":", 1)
                    d[k.strip()] = v.strip()
    return d


def verificar(saida, aceitar_reduzida=False):
    """Devolve (falhas, avisos, resumo)."""
    falhas, avisos = [], []
    brutos = ler_csv(os.path.join(saida, "resultados.csv"))
    if not brutos:
        return [f"sem {os.path.join(saida, 'resultados.csv')}"], [], ""
    man = ler_manifesto(os.path.join(saida, "manifesto.txt"))

    politicas = sorted({r["politica"] for r in brutos})
    if not aceitar_reduzida:
        try:
            escala = float(man.get("escala", "0").replace(",", "."))
        except ValueError:
            escala = 0.0
        if escala < 1:
            falhas.append(f"rodada reduzida (escala {man.get('escala', '?')}): não prova a matriz nem o porte")
        reps = man.get("reps", "")
        try:
            minimo = int(reps.split("..")[0])
        except ValueError:
            minimo = 0
        if minimo < 3:
            falhas.append(f"repetições mínimas {reps or '?'}: o enunciado pede pelo menos 3")
        for p in matriz.POLITICAS:
            if p not in politicas:
                falhas.append(f"política {p} ausente do resultados.csv")

    blocos = {}
    for r in ler_csv(os.path.join(saida, "estatisticas.csv")):
        blocos.setdefault((r["politica"], r["tabela"]), int(r["b"]))

    cont = {"ok": 0, "nao_aplicavel": 0, "inviavel": 0, "ausente": 0}
    portes = 0
    for pol in [p for p in matriz.POLITICAS if p in politicas]:
        for cel in matriz.obrigatorias():
            texto, est = matriz.estado_celula(brutos, pol, *cel)
            cont[est] += 1
            nome = f"{pol} {cel[0]} {cel[1] or '-'} config. {cel[2]} {cel[3] or cel[4]}"
            if est == "ausente":
                falhas.append(f"célula obrigatória ausente: {nome}")
            elif est == "nao_aplicavel":
                mot = matriz.motivos(brutos, pol, cel, "nao_aplicavel")
                if not matriz.nao_aplicavel_fundamentado(cel, mot):
                    falhas.append(f"célula obrigatória dada como não aplicável sem fundamento: {nome} ({'; '.join(mot)})")
            elif est == "inviavel":
                avisos.append(f"célula inviável em M: {nome} ({'; '.join(matriz.motivos(brutos, pol, cel, 'inviavel'))})")
            if cel[0] != "natural_join":
                continue
            for r in matriz.linhas_da_celula(brutos, pol, *cel):
                if r["status"] != "ok":
                    continue
                be, bi = blocos.get((pol, r["externa"])), blocos.get((pol, r["interna"]))
                if be is None or bi is None:
                    falhas.append(f"sem blocos de {r['externa']} ou {r['interna']} no estatisticas.csv ({nome})")
                    continue
                m = int(r["m"])
                visto = "_".join(sorted((matriz.porte(be, m), matriz.porte(bi, m)), key=("menor", "maior").index))
                portes += 1
                if visto != cel[1]:
                    falhas.append(f"porte não confere: {nome}, {r['variante']} tem {r['externa']} com {be} blocos e "
                                  f"{r['interna']} com {bi} com m = {m} ({visto})")
    resumo = (f"matriz obrigatória: {len(matriz.obrigatorias())} células por política, {', '.join(politicas)}; "
              f"{cont['ok']} executadas, {cont['nao_aplicavel']} não aplicáveis, {cont['inviavel']} inviáveis, "
              f"{cont['ausente']} ausentes; porte conferido em {portes} execuções de junção")
    return falhas, avisos, resumo


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 1:
        print(__doc__)
        return 2
    falhas, avisos, resumo = verificar(args[0], "--aceitar-reduzida" in sys.argv[1:])
    print(resumo)
    for a in avisos:
        print("  AVISO " + a)
    for f in falhas[:100]:
        print("  FALHA " + f)
    print("matriz: " + ("REPROVADA" if falhas else "aprovada") + f" ({len(falhas)} falhas)")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())

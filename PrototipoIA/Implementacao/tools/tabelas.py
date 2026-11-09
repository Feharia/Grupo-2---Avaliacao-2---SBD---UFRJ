#!/usr/bin/env python3
"""Gera as tabelas de análise a partir de uma saída da bancada.

Uso: python3 -I tools/tabelas.py SAIDA [--politica LRU] [--md ARQUIVO] [--csv PASTA] [--sem-csv]

Lê SAIDA/resultados.csv (obrigatório) e, quando existem, fases.csv, estatisticas.csv,
ocupacao.csv, operacoes.csv, constantes.csv, manifesto.txt e conferencia.txt. Grava:
  SAIDA/tabelas.md       todas as tabelas em Markdown, com números formatados;
  SAIDA/tabelas/*.csv    as mesmas tabelas em CSV, com números sem formatação.

Métrica central: blocos transferidos = leituras + gravações, sem o flush de verificação
(que deve ser 0). As contagens de cada combinação são a mediana das repetições (elas
coincidem entre si); o tempo é a mediana. --politica escolhe a política das tabelas
principais (padrão: a primeira do CSV); a tabela LRU × MRU usa as duas.
Só usa a biblioteca padrão.
"""
import argparse
import csv
import os
import re
import statistics
import sys
from collections import Counter, OrderedDict, defaultdict

KEY = ("politica", "operacao", "tipo", "tamanho", "config", "seletividade",
       "variante", "algoritmo", "externa", "interna", "m")
INT_COLS = ("linhas", "requisicoes", "acertos", "faltas", "leituras_seq", "leituras_aleat",
            "gravacoes_seq", "gravacoes_aleat", "leituras", "gravacoes", "expulsoes_sujas",
            "gravacoes_forcadas", "alocacoes", "leituras_indice", "leituras_temp",
            "gravacoes_temp", "flush_writes")
FLOAT_COLS = ("estimado_transf", "estimado_seeks", "tempo_ms", "taxa_acerto")

TABELA = {"aluno": "Aluno", "curso": "Curso", "disciplina": "Disciplina", "turma": "Turma",
          "inscricao": "Inscrição", "detalhes": "Detalhes"}
TIPO = {"igualdade_unique": "igualdade em UNIQUE", "faixa_unique": "faixa em UNIQUE",
        "faixa_not_unique": "faixa em NOT UNIQUE", "natural_join": "natural join",
        "equijoin_unique": "equijoin UNIQUE", "equijoin_not_unique": "equijoin NOT UNIQUE"}
TAMANHO = {"menor_menor": "menor ⋈ menor", "menor_maior": "menor ⋈ maior", "maior_maior": "maior ⋈ maior"}
ALG = {"bnl": "BNL", "sort_merge": "sort-merge", "hash": "hash"}
ALG_ORDEM = ("bnl", "sort_merge", "hash")
TIPO_ORDEM = ("igualdade_unique", "faixa_unique", "faixa_not_unique")
SELET_ORDEM = ("pontual", "0,1%", "50%")
# colunas da tabela "organização × caminho", na ordem em que aparecem
CATEGORIAS = OrderedDict([
    (("heap", "scan"), "Heap (varredura)"),
    (("bt", "primary_btree"), "B+ primária"),
    (("h", "primary_hash"), "Hash primário"),
    (("heap", "secondary_hash"), "Heap + hash sec."),
    (("heap", "secondary_btree"), "Heap + B+ sec."),
    (("bt", "clustering_btree"), "B+ prim. + clustering"),
    (("h", "clustering_btree"), "Hash prim. + clustering"),
])


# ----------------------------------------------------------------------------- formatação
def fmt_int(v):
    return "—" if v is None else f"{int(round(v)):,}".replace(",", ".")


def fmt_num(v, casas=1):
    if v is None:
        return "—"
    s = f"{v:,.{casas}f}"
    return s.replace(",", "§").replace(".", ",").replace("§", ".")


def fmt_pct(v, sinal=True):
    if v is None:
        return "—"
    return (f"{v:+.1f}%" if sinal else f"{v:.1f}%").replace(".", ",")


def fmt_vezes(v):
    if v is None:
        return "—"
    if abs(v - 1.0) < 1e-9:
        return "melhor"
    return fmt_num(v, 2 if v < 10 else 1) + "×"


def erro_pct(estimado, medido):
    return None if not medido else 100.0 * (estimado - medido) / medido


# ----------------------------------------------------------------------------- leitura
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


class Combo:
    """Uma combinação medida (política, operação, célula, variante, algoritmo), já agregada."""

    def __init__(self, chave, linhas):
        self.k = dict(zip(KEY, chave))
        self.reps = len(linhas)
        for c in INT_COLS:
            setattr(self, c, statistics.median(int(r[c] or 0) for r in linhas))
        for c in FLOAT_COLS:
            setattr(self, c, statistics.median(float(r[c] or 0) for r in linhas))
        totais = [int(r["leituras"]) + int(r["gravacoes"]) for r in linhas]
        self.variou = len(set(totais)) > 1
        meio = statistics.median(totais)
        self.dispersao = 100.0 * (max(totais) - min(totais)) / meio if meio else 0.0

    def __getitem__(self, c):
        return self.k[c]

    @property
    def total(self):
        return self.leituras + self.gravacoes

    @property
    def erro(self):
        return erro_pct(self.estimado_transf, self.total)

    @property
    def trocada(self):
        return self.k["variante"].endswith("(papeis trocados)")

    @property
    def variante_base(self):
        return self.k["variante"].replace(" (papeis trocados)", "")


class Base:
    def __init__(self, saida, politica):
        self.saida = saida
        brutos = ler_csv(os.path.join(saida, "resultados.csv"))
        if not brutos:
            raise SystemExit(f"não achei {os.path.join(saida, 'resultados.csv')}")
        self.brutos = brutos
        self.politicas = list(OrderedDict.fromkeys(r["politica"] for r in brutos))
        self.politica = politica or self.politicas[0]
        if self.politica not in self.politicas:
            raise SystemExit(f"política {self.politica} não está no CSV ({', '.join(self.politicas)})")
        grupos = defaultdict(list)
        for r in brutos:
            if r["status"] == "ok":
                grupos[tuple(r[c] for c in KEY)].append(r)
        self.combos = [Combo(k, v) for k, v in grupos.items()]
        self.nao_exec = [r for r in brutos if r["status"] != "ok"]
        self.ops = {r["operacao"]: r for r in ler_csv(os.path.join(saida, "operacoes.csv"))}
        self.consts = {r["operacao"]: r for r in ler_csv(os.path.join(saida, "constantes.csv"))}
        self.fases = ler_csv(os.path.join(saida, "fases.csv"))
        self.stats = ler_csv(os.path.join(saida, "estatisticas.csv"))
        self.ocup = ler_csv(os.path.join(saida, "ocupacao.csv"))
        self.manifesto = ler_manifesto(os.path.join(saida, "manifesto.txt"))
        conf = os.path.join(saida, "conferencia.txt")
        self.conferencia = open(conf, encoding="utf-8").read().strip() if os.path.exists(conf) else ""

    def da_politica(self, pred=lambda c: True, politica=None):
        p = politica or self.politica
        return [c for c in self.combos if c["politica"] == p and pred(c)]

    # ------------------------------------------------------------------ rótulos
    def campos_juncao(self, op):
        """{tabela: campo} da junção, lido do SQL equivalente."""
        sql = self.ops.get(op, {}).get("sql", "")
        m = re.search(r"FROM (\w+) x JOIN (\w+) y ON x\.(\w+) = y\.(\w+)", sql)
        return {m.group(1): m.group(3), m.group(2): m.group(4)} if m else {}

    def campo_selecao(self, op):
        sql = self.ops.get(op, {}).get("sql", "")
        m = re.search(r"a\.(\w+)\s*(?:>=|=)", sql)
        return m.group(1) if m else "?"

    def rotulo_op(self, op, tipo):
        if op.startswith("j_"):
            sql = self.ops.get(op, {}).get("sql", "")
            m = re.search(r"FROM (\w+) x JOIN (\w+) y ON x\.(\w+) = y\.(\w+)", sql)
            if m:
                a, b, fa, fb = m.groups()
                campo = fa if fa == fb else f"{fa} = {fb}"
                return f"{TABELA.get(a, a)} ⋈ {TABELA.get(b, b)} ({campo})"
            return op
        campo = self.campo_selecao(op)
        return f"{TIPO.get(tipo, tipo)}: {campo}"


def partes_estrutura(nome):
    m = re.fullmatch(r"([a-z]+)_(heap|bt|h)(?:_(\w+))?", nome)
    return (m.group(1), m.group(2), m.group(3)) if m else (nome, "?", None)


def caminho(estrutura_e_tipo, campo):
    """'aluno_bt_curso:clustering_btree' -> 'B+ primária (curso) + B+ clustering (curso)'."""
    estrutura, _, tipo = estrutura_e_tipo.partition(":")
    _, org, chave = partes_estrutura(estrutura)
    prim = {"bt": f"B+ primária ({chave})", "h": f"hash primário ({chave})", "heap": "heap"}.get(org, estrutura)
    if tipo == "scan":
        return f"{prim}, varredura"
    if tipo in ("primary_btree", "primary_hash"):
        return prim
    if tipo == "secondary_hash":
        return f"{prim} + hash secundário ({campo})"
    if tipo == "secondary_btree":
        return f"{prim} + B+ secundária ({campo})"
    if tipo == "clustering_btree":
        return f"{prim} + B+ clustering ({campo})"
    return estrutura_e_tipo


def categoria(estrutura_e_tipo):
    estrutura, _, tipo = estrutura_e_tipo.partition(":")
    return (partes_estrutura(estrutura)[1], tipo)


def operandos(base, c):
    """Externa → interna, com o caminho de cada uma."""
    campos = base.campos_juncao(c["operacao"])
    ext_int = []
    for nome in (c["externa"], c["interna"]):
        for parte in c.variante_base.split(" x "):
            if parte.split(":")[0] == nome:
                tab = partes_estrutura(nome)[0]
                ext_int.append(f"{TABELA.get(tab, tab)}: {caminho(parte, campos.get(tab, '?'))}")
                break
        else:
            ext_int.append(nome)
    return " → ".join(ext_int)


# ----------------------------------------------------------------------------- tabelas
class Tabela:
    def __init__(self, nome, titulo, texto, cab, linhas, brutas=None, cab_bruto=None, nota=""):
        self.nome, self.titulo, self.texto, self.nota = nome, titulo, texto, nota
        self.cab, self.linhas = cab, linhas
        self.cab_bruto = cab_bruto or cab
        self.brutas = brutas if brutas is not None else linhas

    def markdown(self):
        out = [f"## {self.titulo}", ""]
        if self.texto:
            out += [self.texto, ""]
        if not self.linhas:
            out += ["_Sem dados nesta saída._", ""]
            return "\n".join(out)
        alinha = ["---:" if self._numerica(i) else "---" for i in range(len(self.cab))]
        out.append("| " + " | ".join(h.replace("|", "\\|") for h in self.cab) + " |")
        out.append("|" + "|".join(alinha) + "|")
        for l in self.linhas:
            out.append("| " + " | ".join(str(x).replace("|", "\\|") for x in l) + " |")
        if self.nota:
            out += ["", self.nota]
        out.append("")
        return "\n".join(out)

    def _numerica(self, i):
        vals = [str(l[i]) for l in self.linhas if str(l[i]) not in ("—", "", "melhor")]
        return bool(vals) and all(re.fullmatch(r"[\*]*[+\-]?[\d.,]+[%×]?[\*]*", v) for v in vals)


def t_resumo(b):
    m = b.manifesto
    st = Counter((r["status"], r["motivo"].split(":")[0] if r["motivo"] else "") for r in b.nao_exec
                 if r["politica"] == b.politica)
    linhas = [
        ["Escala", m.get("escala", "?")],
        ["Políticas comparadas", ", ".join(b.politicas)],
        ["Repetições por combinação", f"{m.get('reps', '?')} (tolerância {m.get('tolerancia', '?')})"],
        ["M das junções / das seleções", f"{m.get('m_juncao', '?')} / {m.get('m_selecao', '?')} molduras"],
        ["Chaves por lote de seleção pontual", m.get("lote", "?")],
        ["Combinações medidas (por política)", fmt_int(len(b.da_politica()))],
        ["Linhas medidas no CSV (todas as repetições e políticas)",
         fmt_int(sum(1 for r in b.brutos if r["status"] == "ok"))],
    ]
    for (status, caso), n in sorted(st.items()):
        linhas.append([f"Células `{status}` ({caso}) por política", fmt_int(n)])
    if b.conferencia:
        linhas.append(["Conferência com o SQLite", b.conferencia])
    linhas.append(["Duração da rodada", f"{m.get('duracao_s', '?')} s"])
    return Tabela("01_resumo", "Resumo da rodada", "", ["Item", "Valor"], linhas)


def t_igualdade(b):
    lote = int(b.manifesto.get("lote", "0") or 0)
    combos = sorted(b.da_politica(lambda c: c["tipo"].startswith("igualdade")),
                    key=lambda c: (c["operacao"], c["config"], c.total))
    melhor = defaultdict(lambda: float("inf"))
    for c in combos:
        melhor[c["operacao"]] = min(melhor[c["operacao"]], c.total)
    linhas, brutas = [], []
    for c in combos:
        campo = b.campo_selecao(c["operacao"])
        por_chave = c.leituras / lote if lote else None
        vezes = c.total / melhor[c["operacao"]] if melhor[c["operacao"]] else None
        linhas.append([b.rotulo_op(c["operacao"], c["tipo"]), c["config"], caminho(c["variante"], campo),
                       fmt_int(c.linhas), fmt_int(c.leituras), fmt_num(por_chave, 2), fmt_int(c.leituras_aleat),
                       fmt_num(c.taxa_acerto, 2), fmt_int(c.estimado_transf), fmt_pct(c.erro),
                       fmt_num(c.tempo_ms, 1), fmt_pct(c.dispersao, False), fmt_vezes(vezes)])
        brutas.append([c["operacao"], c["config"], c["variante"], c.linhas, c.leituras, por_chave, c.leituras_aleat,
                       c.taxa_acerto, c.estimado_transf, c.erro, c.tempo_ms, c.dispersao, vezes])
    return Tabela(
        "02_selecao_igualdade", "Seleção por igualdade em campo UNIQUE (lote de chaves)",
        f"Cada execução busca um lote de {fmt_int(lote)} chaves sorteadas. O lote muda a cada repetição, então as "
        "contagens variam um pouco: a tabela mostra a mediana e, em “Var. reps”, a amplitude entre repetições. "
        "“Leituras/chave” divide as leituras pelo lote. “× melhor” compara com o caminho de menor custo na mesma "
        "consulta.",
        ["Consulta", "Config.", "Caminho", "Linhas", "Leituras", "Leituras/chave", "Aleatórias", "Taxa de acerto",
         "Estimado", "Erro est.", "Tempo (ms)", "Var. reps", "× melhor"], linhas, brutas,
        ["operacao", "config", "variante", "linhas", "leituras", "leituras_por_chave", "leituras_aleat",
         "taxa_acerto", "estimado_transf", "erro_pct", "tempo_ms", "amplitude_reps_pct", "vezes_melhor"])


def t_faixa(b):
    combos = sorted(b.da_politica(lambda c: c["tipo"].startswith("faixa")),
                    key=lambda c: (ordem(TIPO_ORDEM, c["tipo"]), b.campo_selecao(c["operacao"]),
                                   ordem(SELET_ORDEM, c["seletividade"]), c["config"], c.total))
    melhor = defaultdict(lambda: float("inf"))
    for c in combos:
        melhor[c["operacao"]] = min(melhor[c["operacao"]], c.total)
    linhas, brutas = [], []
    for c in combos:
        campo = b.campo_selecao(c["operacao"])
        vezes = c.total / melhor[c["operacao"]] if melhor[c["operacao"]] else None
        seq = 100.0 * c.leituras_seq / c.leituras if c.leituras else None
        linhas.append([b.rotulo_op(c["operacao"], c["tipo"]), c["seletividade"], c["config"],
                       caminho(c["variante"], campo), fmt_int(c.linhas), fmt_int(c.leituras), fmt_pct(seq, False),
                       fmt_int(c.estimado_transf), fmt_pct(c.erro), fmt_num(c.tempo_ms, 2), fmt_vezes(vezes)])
        brutas.append([c["operacao"], c["seletividade"], c["config"], c["variante"], c.linhas, c.leituras, seq,
                       c.estimado_transf, c.erro, c.tempo_ms, vezes])
    return Tabela(
        "03_selecao_faixa", "Seleção por faixa",
        "“% seq.” é a fração das leituras que foram sequenciais. “× melhor” compara com o caminho de menor custo "
        "na mesma consulta e seletividade.",
        ["Consulta", "Selet.", "Config.", "Caminho", "Linhas", "Leituras", "% seq.", "Estimado", "Erro est.",
         "Tempo (ms)", "× melhor"], linhas, brutas,
        ["operacao", "seletividade", "config", "variante", "linhas", "leituras", "pct_seq", "estimado_transf",
         "erro_pct", "tempo_ms", "vezes_melhor"])


def t_organizacoes(b):
    combos = b.da_politica(lambda c: not c["operacao"].startswith("j_"))
    na = {(r["operacao"], categoria(r["variante"])): r["motivo"].split(":")[0]
          for r in b.nao_exec if r["politica"] == b.politica and r["variante"]}
    por_op = OrderedDict()
    for c in sorted(combos, key=lambda c: (ordem(TIPO_ORDEM, c["tipo"]), b.campo_selecao(c["operacao"]),
                                           ordem(SELET_ORDEM, c["seletividade"]))):
        por_op.setdefault(c["operacao"], {})[categoria(c["variante"])] = c
    usadas = [k for k in CATEGORIAS if any(k in d for d in por_op.values()) or any(op_k[1] == k for op_k in na)]
    linhas, brutas = [], []
    for op, d in por_op.items():
        c0 = next(iter(d.values()))
        menor = min(c.total for c in d.values())
        rot = [b.rotulo_op(op, c0["tipo"]), c0["seletividade"]]
        bruta = [op, c0["seletividade"]]
        for k in usadas:
            if k in d:
                v = fmt_int(d[k].total)
                rot.append(f"**{v}**" if d[k].total == menor else v)
                bruta.append(d[k].total)
            elif (op, k) in na:
                rot.append(f"n/a ({na[(op, k)]})")
                bruta.append(na[(op, k)])
            else:
                rot.append("")
                bruta.append("")
        linhas.append(rot)
        brutas.append(bruta)
    return Tabela(
        "04_organizacao_x_caminho", "Mesma seleção, organizações e caminhos diferentes",
        f"Blocos transferidos ({b.politica}). Em negrito, o menor custo de cada linha. “n/a” é uma célula que não se "
        "aplica (decisão 170); vazio é uma combinação que a bancada não monta para essa consulta.",
        ["Consulta", "Selet."] + [CATEGORIAS[k] for k in usadas], linhas, brutas,
        ["operacao", "seletividade"] + [f"{k[0]}:{k[1]}" for k in usadas])


def t_juncoes(b):
    combos = b.da_politica(lambda c: c["operacao"].startswith("j_"))
    melhor = defaultdict(lambda: float("inf"))
    for c in combos:
        g = (c["operacao"], c["config"], c.variante_base)
        melhor[g] = min(melhor[g], c.total)
    ordem_op = list(OrderedDict.fromkeys(c["operacao"] for c in combos))
    combos.sort(key=lambda c: (ordem_op.index(c["operacao"]), c["config"], c.variante_base,
                               ALG_ORDEM.index(c["algoritmo"]), c.trocada))
    linhas, brutas = [], []
    for c in combos:
        g = (c["operacao"], c["config"], c.variante_base)
        vezes = c.total / melhor[g] if melhor[g] else None
        linhas.append([f"{b.rotulo_op(c['operacao'], c['tipo'])} — {TIPO.get(c['tipo'], c['tipo'])}",
                       TAMANHO.get(c["tamanho"], c["tamanho"]), c["config"], operandos(b, c),
                       ALG.get(c["algoritmo"], c["algoritmo"]), fmt_int(c.leituras), fmt_int(c.gravacoes),
                       fmt_int(c.total), fmt_int(c.estimado_transf), fmt_pct(c.erro), fmt_num(c.tempo_ms, 1),
                       fmt_vezes(vezes)])
        brutas.append([c["operacao"], c["tipo"], c["tamanho"], c["config"], c["variante"], c["externa"],
                       c["interna"], c["algoritmo"], c.leituras, c.gravacoes, c.total, c.estimado_transf, c.erro,
                       c.tempo_ms, vezes])
    return Tabela(
        "05_juncoes", "Junções: custo de cada algoritmo",
        f"M = {b.manifesto.get('m_juncao', '?')} molduras. A primeira tabela de “Externa → interna” é a externa no BNL, "
        "a de build no hash e a primeira entrada do merge. “× melhor” compara com a opção mais barata para as mesmas "
        "entradas (todos os algoritmos e as duas ordens).",
        ["Junção", "Tamanho", "Config.", "Externa → interna", "Algoritmo", "Leituras", "Gravações", "Total",
         "Estimado", "Erro est.", "Tempo (ms)", "× melhor"], linhas, brutas,
        ["operacao", "tipo", "tamanho", "config", "variante", "externa", "interna", "algoritmo", "leituras",
         "gravacoes", "total", "estimado_transf", "erro_pct", "tempo_ms", "vezes_melhor"])


def t_vencedor(b):
    grupos = OrderedDict()
    for c in b.da_politica(lambda c: c["operacao"].startswith("j_")):
        g = (c["operacao"], c["tipo"], c["tamanho"], c["config"], c.variante_base)
        atual = grupos.setdefault(g, {}).get(c["algoritmo"])
        if atual is None or c.total < atual.total:
            grupos[g][c["algoritmo"]] = c
    linhas, brutas = [], []
    for (op, tipo, tam, cfg, var), d in grupos.items():
        ordenado = sorted(d.values(), key=lambda c: (c.total, ALG_ORDEM.index(c["algoritmo"])))
        venc = ordenado[0]
        empatados = [c for c in ordenado if c.total == venc.total]
        resto = [c for c in ordenado if c.total != venc.total]
        vant = resto[0].total / venc.total if resto and venc.total else None
        exemplo = next(iter(d.values()))
        lin = [b.rotulo_op(op, tipo), TIPO.get(tipo, tipo), TAMANHO.get(tam, tam), cfg,
               operandos(b, exemplo).replace(" → ", " ⋈ ") if not exemplo.trocada else var]
        bru = [op, tipo, tam, cfg, var]
        for a in ALG_ORDEM:
            v = d.get(a)
            txt = fmt_int(v.total) if v else "—"
            lin.append(f"**{txt}**" if v in empatados else txt)
            bru.append(v.total if v else "")
        nomes = " = ".join(ALG.get(c["algoritmo"], c["algoritmo"]) for c in empatados)
        lin += [nomes + (" (empate)" if len(empatados) > 1 else ""), fmt_vezes(vant) if vant else "—"]
        bru += ["=".join(c["algoritmo"] for c in empatados), vant]
        linhas.append(lin)
        brutas.append(bru)
    return Tabela(
        "06_melhor_algoritmo", "Melhor algoritmo por situação",
        "Para cada célula (tipo, tamanho, configuração), o total de blocos de cada algoritmo na melhor ordem "
        "externa/interna. “Vantagem” é quantas vezes o próximo algoritmo mais barato custa em relação ao vencedor "
        "(empates contam juntos).",
        ["Junção", "Tipo", "Tamanho", "Config.", "Entradas", "BNL", "Sort-merge", "Hash", "Vencedor", "Vantagem"],
        linhas, brutas,
        ["operacao", "tipo", "tamanho", "config", "variante", "bnl", "sort_merge", "hash", "vencedor", "vantagem"])


def t_ordem(b):
    pares = defaultdict(dict)
    for c in b.da_politica(lambda c: c["operacao"].startswith("j_")):
        pares[(c["operacao"], c["config"], c.variante_base, c["algoritmo"])][c.trocada] = c
    linhas, brutas = [], []
    for (op, cfg, var, alg), d in pares.items():
        if len(d) < 2:
            continue
        p, t = d[False], d[True]
        dif = erro_pct(t.total, p.total)
        linhas.append([b.rotulo_op(op, p["tipo"]), cfg, ALG.get(alg, alg), operandos(b, p), fmt_int(p.total),
                       operandos(b, t), fmt_int(t.total), fmt_pct(dif)])
        brutas.append([op, cfg, var, alg, p["externa"], p.total, t["externa"], t.total, dif])
    return Tabela(
        "07_ordem_externa_interna", "Escolha da externa e da interna",
        "A ordem padrão segue a decisão 122: no BNL e no hash, a menor por fora (build); no sort-merge, a UNIQUE "
        "por dentro. “Δ” é o custo da ordem trocada em relação à padrão.",
        ["Junção", "Config.", "Algoritmo", "Ordem padrão", "Total", "Ordem trocada", "Total", "Δ"], linhas, brutas,
        ["operacao", "config", "variante", "algoritmo", "externa_padrao", "total_padrao", "externa_trocada",
         "total_trocada", "delta_pct"])


def t_politicas(b):
    if len(b.politicas) < 2:
        return Tabela("08_lru_x_mru", "Política de substituição", "Esta saída tem uma política só.", [], [])
    p1, p2 = b.politicas[:2]
    idx = defaultdict(dict)
    for c in b.combos:
        if c["politica"] in (p1, p2):
            chave = tuple(c[k] for k in KEY if k != "politica")
            idx[chave][c["politica"]] = c
    linhas, brutas, iguais = [], [], 0
    difs = []
    for chave, d in idx.items():
        if p1 not in d or p2 not in d:
            continue
        a, z = d[p1], d[p2]
        if a.total == z.total:
            iguais += 1
            continue
        difs.append((abs(erro_pct(z.total, a.total) or 0), a, z))
    for _, a, z in sorted(difs, key=lambda x: -x[0]):
        dif = erro_pct(z.total, a.total)
        if a["operacao"].startswith("j_"):
            onde = f"{ALG.get(a['algoritmo'])}, config. {a['config']}: {operandos(b, a)}"
        else:
            onde = f"config. {a['config']}: {caminho(a['variante'], b.campo_selecao(a['operacao']))}"
        linhas.append([b.rotulo_op(a["operacao"], a["tipo"]), a["seletividade"] or TAMANHO.get(a["tamanho"], ""),
                       onde, fmt_int(a.total), fmt_int(z.total), fmt_pct(dif), fmt_num(a.taxa_acerto, 2),
                       fmt_num(z.taxa_acerto, 2)])
        brutas.append([a["operacao"], a["config"], a["variante"], a["algoritmo"], a.total, z.total, dif,
                       a.taxa_acerto, z.taxa_acerto])
    return Tabela(
        "08_lru_x_mru", f"Política de substituição: {p1} × {p2}",
        f"Só as combinações em que o total de blocos muda. Nas outras {iguais}, as duas políticas transferem o mesmo "
        "número de blocos.",
        ["Consulta", "Selet./tamanho", "Onde", p1, p2, "Δ", f"Acerto {p1}", f"Acerto {p2}"], linhas, brutas,
        ["operacao", "config", "variante", "algoritmo", p1, p2, "delta_pct", f"acerto_{p1}", f"acerto_{p2}"])


def familia(c):
    if c["operacao"].startswith("j_"):
        return f"junção: {ALG.get(c['algoritmo'], c['algoritmo'])}"
    return "seleção: igualdade (lote)" if c["tipo"].startswith("igualdade") else "seleção: faixa"


def t_estimativa(b):
    fam = defaultdict(list)
    for c in b.da_politica():
        if c.erro is not None:
            fam[familia(c)].append(c)
    linhas, brutas = [], []
    for f in sorted(fam):
        cs = fam[f]
        abs_err = [abs(c.erro) for c in cs]
        pior = max(cs, key=lambda c: abs(c.erro))
        dentro = sum(1 for e in abs_err if e <= 10)
        if pior["operacao"].startswith("j_"):
            onde = f"config. {pior['config']}: {operandos(b, pior)}"
        else:
            onde = f"config. {pior['config']}: {caminho(pior['variante'], b.campo_selecao(pior['operacao']))}"
        linhas.append([f, fmt_int(len(cs)), fmt_pct(statistics.median(abs_err), False), f"{dentro} de {len(cs)}",
                       fmt_pct(pior.erro), f"{b.rotulo_op(pior['operacao'], pior['tipo'])} ({onde})"])
        brutas.append([f, len(cs), statistics.median(abs_err), dentro, pior.erro, pior["operacao"], pior["variante"]])
    return Tabela(
        "09_estimado_x_medido", "Custo estimado × medido",
        "Erro = (estimado − medido) / medido, em blocos transferidos. As fórmulas são as dos livros (Elmasri e "
        "Silberschatz), com os parâmetros medidos pela própria bancada.",
        ["Família", "Combinações", "Mediana do erro absoluto", "Erro absoluto ≤ 10%", "Maior erro", "Onde"],
        linhas, brutas,
        ["familia", "combinacoes", "mediana_abs_erro_pct", "ate_10pct", "maior_erro_pct", "operacao", "variante"])


def t_fases(b):
    fases = [r for r in b.fases if r["politica"] == b.politica and r["rep"] == "1" and r["algoritmo"]]
    if not fases:
        return Tabela("10_fases_juncoes", "Fases das junções", "Sem fases.csv nesta saída.", [], [])
    por = OrderedDict()
    for r in fases:
        por.setdefault((r["operacao"], r["variante"], r["algoritmo"]), []).append(r)
    combos = {(c["operacao"], c["variante"], c["algoritmo"]): c for c in b.da_politica()}
    linhas, brutas = [], []
    for (op, var, alg), rs in por.items():
        if alg == "bnl":
            continue
        c = combos.get((op, var, alg))
        partes, recursao = [], ""
        for r in rs:
            if r["fase"].startswith("resumo"):
                recursao = r["fase"].replace("resumo: ", "").replace("_", " ")
                continue
            nome = r["fase"].replace("_", " ").replace("intercalacao", "intercalação")
            partes.append(f"{nome} {fmt_int(int(r['leituras']))}/{fmt_int(int(r['gravacoes']))}")
            brutas.append([op, var, alg, r["fase"], r["leituras"], r["gravacoes"], r["requisicoes"]])
        linhas.append([b.rotulo_op(op, c["tipo"]) if c else op, c["config"] if c else "",
                       operandos(b, c) if c else var, ALG.get(alg, alg), " · ".join(partes), recursao or "—"])
    return Tabela(
        "10_fases_juncoes", "Fases do sort-merge e do hash join",
        "Leituras/gravações de cada fase (1ª repetição). No hash join, o resumo traz os passes de particionamento "
        "recursivo e os bailouts (decisão 171).",
        ["Junção", "Config.", "Externa → interna", "Algoritmo", "Fases (leituras/gravações)", "Recursão"],
        linhas, brutas, ["operacao", "variante", "algoritmo", "fase", "leituras", "gravacoes", "requisicoes"])


def t_estruturas(b):
    rs = [r for r in b.stats if r["politica"] == b.politica]
    if not rs:
        return Tabela("11_estruturas", "Estruturas construídas", "Sem estatisticas.csv nesta saída.", [], [])
    vistos, linhas, brutas = set(), [], []
    for r in rs:
        if r["estrutura"] in vistos:
            continue
        vistos.add(r["estrutura"])
        num = lambda c: fmt_int(int(r[c])) if r[c] else "—"
        flt = lambda c, d=2: fmt_num(float(r[c]), d) if r[c] else "—"
        linhas.append([r["estrutura"], num("n"), r["r"], r["bfr"], num("blocos"), num("altura"), num("folhas"),
                       num("distintos"), num("buckets"), num("overflow"), flt("fator_carga"), flt("cadeia_media"),
                       num("cadeia_max")])
        brutas.append([r[k] for k in ("estrutura", "n", "r", "bfr", "blocos", "altura", "folhas", "distintos",
                                      "buckets", "overflow", "fator_carga", "cadeia_media", "cadeia_max")])
    return Tabela(
        "11_estruturas", "Estruturas construídas",
        "Medidas pela própria bancada depois da carga. n = registros, R = bytes por registro, bfr = registros por "
        "bloco. Nos índices, “Blocos” conta só o índice.",
        ["Estrutura", "n", "R", "bfr", "Blocos", "Altura", "Folhas", "Distintos", "Buckets", "Overflow",
         "Fator de carga", "Cadeia média", "Cadeia máx."], linhas, brutas,
        ["estrutura", "n", "r", "bfr", "blocos", "altura", "folhas", "distintos", "buckets", "overflow",
         "fator_carga", "cadeia_media", "cadeia_max"])


def t_ocupacao(b):
    rs = [r for r in b.ocup if r["politica"] == b.politica]
    if not rs:
        return Tabela("12_ocupacao", "Ocupação antes e depois da manutenção", "Sem ocupacao.csv.", [], [])
    antes = {r["estrutura"]: r for r in rs if r["momento"] == "antes"}
    depois = {r["estrutura"]: r for r in rs if r["momento"] == "depois"}
    linhas, brutas, iguais = [], [], 0
    for e, a in antes.items():
        d = depois.get(e)
        if not d:
            continue
        ta, td = int(a["total"]), int(d["total"])
        if ta == td:
            iguais += 1
            continue
        linhas.append([e, fmt_int(int(a["dados"])), fmt_int(int(d["dados"])), fmt_int(int(a["overflow"])),
                       fmt_int(int(d["overflow"])), fmt_int(ta), fmt_int(td), fmt_pct(erro_pct(td, ta))])
        brutas.append([e, a["dados"], d["dados"], a["overflow"], d["overflow"], ta, td, erro_pct(td, ta)])
    return Tabela(
        "12_ocupacao", "Ocupação antes e depois da carga de manutenção",
        f"Blocos por estrutura. Só as estruturas que mudaram; as outras {iguais} ficaram iguais (tabelas fora da "
        "carga de manutenção).",
        ["Estrutura", "Dados antes", "Dados depois", "Overflow antes", "Overflow depois", "Total antes",
         "Total depois", "Δ"], linhas, brutas,
        ["estrutura", "dados_antes", "dados_depois", "overflow_antes", "overflow_depois", "total_antes",
         "total_depois", "delta_pct"])


def t_nao_executadas(b):
    rs = [r for r in b.nao_exec if r["politica"] == b.politica]
    grupos = OrderedDict()
    for r in sorted(rs, key=lambda r: r["motivo"]):
        caso = r["motivo"].split(":")[0]
        g = grupos.setdefault((r["status"], caso), {"n": 0, "ops": OrderedDict(), "motivo": r["motivo"]})
        g["n"] += 1
        g["ops"][r["operacao"]] = None
    linhas = [[s, c, fmt_int(g["n"]), ", ".join(g["ops"]), g["motivo"].split(":", 1)[-1].strip()]
              for (s, c), g in grupos.items()]
    brutas = [[s, c, g["n"], " ".join(g["ops"]), g["motivo"]] for (s, c), g in grupos.items()]
    return Tabela(
        "13_nao_executadas", "Células que não rodam",
        "Células da matriz do enunciado que aparecem no resultados.csv sem execução, com o motivo (decisão 170).",
        ["Status", "Caso", "Linhas por política", "Operações", "Motivo"], linhas, brutas,
        ["status", "caso", "linhas", "operacoes", "motivo"])


GERADORES = (t_resumo, t_igualdade, t_faixa, t_organizacoes, t_juncoes, t_vencedor, t_ordem, t_politicas,
             t_estimativa, t_fases, t_estruturas, t_ocupacao, t_nao_executadas)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("saida", help="pasta de saída da bancada (com resultados.csv)")
    ap.add_argument("--politica", help="política das tabelas principais (padrão: a primeira do CSV)")
    ap.add_argument("--md", help="arquivo Markdown de saída (padrão: SAIDA/tabelas.md)")
    ap.add_argument("--csv", help="pasta dos CSVs (padrão: SAIDA/tabelas)")
    ap.add_argument("--sem-csv", action="store_true", help="não grava os CSVs")
    a = ap.parse_args()

    b = Base(a.saida, a.politica)
    tabelas = [g(b) for g in GERADORES]

    md = a.md or os.path.join(a.saida, "tabelas.md")
    with open(md, "w", encoding="utf-8") as f:
        f.write(f"# Tabelas da rodada ({b.politica})\n\n")
        f.write(f"Geradas por `tools/tabelas.py` a partir de `{os.path.basename(os.path.normpath(a.saida))}/`. "
                "A métrica é o número de blocos transferidos (leituras + gravações); o tempo é só "
                "complementar.\n\n")
        f.write("Índice: " + " · ".join(f"[{t.titulo}](#{ancora(t.titulo)})" for t in tabelas) + "\n\n")
        for t in tabelas:
            f.write(t.markdown() + "\n")

    if not a.sem_csv:
        pasta = a.csv or os.path.join(a.saida, "tabelas")
        os.makedirs(pasta, exist_ok=True)
        for t in tabelas:
            if not t.cab_bruto:
                continue
            with open(os.path.join(pasta, t.nome + ".csv"), "w", newline="", encoding="utf-8") as f:
                w = csv.writer(f)
                w.writerow(t.cab_bruto)
                for l in t.brutas:
                    w.writerow(["" if x is None else (round(x, 4) if isinstance(x, float) else x) for x in l])
        print(f"{len(tabelas)} tabelas em {md} e {pasta}/")
    else:
        print(f"{len(tabelas)} tabelas em {md}")
    # na seleção pontual o lote muda a cada repetição, então variar é esperado
    variaram = [c for c in b.combos if c.variou and not c["tipo"].startswith("igualdade")]
    if variaram:
        print(f"aviso: {len(variaram)} combinações têm contagens diferentes entre repetições; usei a mediana",
              file=sys.stderr)
    return 0


def ordem(lista, valor):
    return lista.index(valor) if valor in lista else len(lista)


def ancora(titulo):
    """Âncora no estilo do GitHub."""
    s = titulo.strip().lower()
    s = re.sub(r"[^\w\- ]", "", s)
    return s.replace(" ", "-")


if __name__ == "__main__":
    sys.exit(main())

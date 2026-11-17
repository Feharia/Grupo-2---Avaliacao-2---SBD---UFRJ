"""Inventário da matriz de experimentos do enunciado v2 (04/11/2026) e estado de cada célula.

Usado por tools/tabelas.py (tabela de cobertura) e por tools/verificar_matriz.py (verificador que
reprova), para os dois lerem a matriz do mesmo jeito (BENCH-005). Só usa a biblioteca padrão.

Uma célula é (tipo, tamanho, config, seletividade, algoritmo):
- junções: tamanho preenchido, seletividade vazia, um algoritmo (bnl, sort_merge, hash);
- seleções: tamanho e algoritmo vazios, uma seletividade (pontual, 1%, 50%).
Obrigatórias: NATURAL JOIN (configurações a–e × três portes × três algoritmos), igualdade em UNIQUE
(a–c × pontual, 1%, 50%) e faixa em NOT UNIQUE (a–c × 1%, 50%). Os equijoins e a faixa em UNIQUE são
opcionais.
"""

CONFIGS = ("a", "b", "c", "d", "e")
CONFIGS_SELECAO = ("a", "b", "c")
TAMANHOS = ("menor_menor", "menor_maior", "maior_maior")
ALGORITMOS = ("bnl", "sort_merge", "hash")
POLITICAS = ("LRU", "MRU")
SELET_IGUALDADE = ("pontual", "1%", "50%")
SELET_FAIXA = ("1%", "50%")

JUNCOES = (("natural_join", "obrigatória"), ("equijoin_unique", "opcional"), ("equijoin_not_unique", "opcional"))
SELECOES = (("igualdade_unique", "obrigatória"), ("faixa_not_unique", "obrigatória"), ("faixa_unique", "opcional"))


def celulas_juncao(tipo):
    """(tamanho, config) → as três células (uma por algoritmo), na ordem da tabela."""
    return [((tam, cfg), [(tipo, tam, cfg, "", a) for a in ALGORITMOS]) for tam in TAMANHOS for cfg in CONFIGS]


def celulas_selecao(tipo):
    """config → as células por seletividade; a faixa não tem nível pontual (None)."""
    selets = SELET_IGUALDADE if tipo.startswith("igualdade") else (None,) + SELET_FAIXA
    return [(cfg, [None if s is None else (tipo, "", cfg, s, "") for s in selets]) for cfg in CONFIGS_SELECAO]


def obrigatorias():
    """Lista plana das células obrigatórias (por política)."""
    out = []
    for tipo, obrig in JUNCOES:
        if obrig == "obrigatória":
            for _, cs in celulas_juncao(tipo):
                out.extend(cs)
    for tipo, obrig in SELECOES:
        if obrig == "obrigatória":
            for _, cs in celulas_selecao(tipo):
                out.extend(c for c in cs if c is not None)
    return out


def linhas_da_celula(brutos, politica, tipo, tamanho, config, selet, alg):
    return [r for r in brutos if r["politica"] == politica and r["tipo"] == tipo and r["config"] == config
            and r["tamanho"] == tamanho and r["seletividade"] == selet and r["algoritmo"] == alg]


def estado_celula(brutos, politica, tipo, tamanho, config, selet, alg):
    """(texto, estado): estado ∈ ok, inviavel, nao_aplicavel, ausente. Basta uma variante ok."""
    rs = linhas_da_celula(brutos, politica, tipo, tamanho, config, selet, alg)
    ok = {r["variante"].replace(" (papeis trocados)", "") for r in rs if r["status"] == "ok"}
    if ok:
        return "ok" if len(ok) == 1 else f"ok ({len(ok)} variantes)", "ok"
    for st in ("inviavel", "nao_aplicavel"):
        motivo = next((r["motivo"] for r in rs if r["status"] == st), None)
        if motivo is not None:
            caso = motivo.split(":")[0] if motivo.startswith("caso") else ""
            return ("inviável" if st == "inviavel" else "n/a") + (f" ({caso})" if caso else ""), st
    return None, "ausente"


def motivos(brutos, politica, celula, status):
    return sorted({r["motivo"] for r in linhas_da_celula(brutos, politica, *celula) if r["status"] == status})


def nao_aplicavel_fundamentado(celula, motivos_da_celula):
    """Numa célula obrigatória, só a igualdade em UNIQUE com 1% ou 50% pode ficar sem execução: a
    consulta devolve uma linha e é medida no nível pontual (caso 3, decisão 170). Junção obrigatória
    nunca é não aplicável."""
    tipo, _, _, selet, _ = celula
    return (tipo == "igualdade_unique" and selet in ("1%", "50%") and bool(motivos_da_celula)
            and all(m.startswith("caso 3") for m in motivos_da_celula))


def porte(blocos, m):
    """menor: a tabela cabe no chunk do BNL (b ≤ m − 2, emenda da 108); maior: não cabe."""
    return "menor" if blocos <= m - 2 else "maior"

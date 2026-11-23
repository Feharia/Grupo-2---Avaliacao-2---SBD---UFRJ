# Aumentos de I/O contra a main — confirmação local

Referência: `4091762d65a52495c02c9cf63da5bc630998ff4d`.
Candidato: `a6b3d48dd913e12f6aa1623954edd34ed13031ae`.
Nenhuma implementação foi alterada nesta investigação.

O usuário informou a publicação das correções nas duas branches e o sucesso
das três execuções do Actions; a origem dessa informação fica explícita em
`publicacao-informada.json`. O relatório do evento pull_request identifica o
candidato como `85069ed`, compatível com o checkout do merge temporário usado
pelo workflow. Esse objeto não está disponível localmente; a reprodução
independente mede diretamente `a6b3d48`.

## Resultado reproduzido

Perfil io-rapido-v2: escala 0,1, seed 20261122, uma repetição, lote de 100,
M=32 nas junções e M=2 nas seleções, LRU/MRU. Driver do candidato compilado
contra headers e biblioteca de cada revisão. Assinaturas e inventário
aprovados pelo comparador; fontes da referência conferidas byte a byte com
o commit. Reutilizadas bibliotecas Release já compiladas dessas revisões.

- 168 cenários pareados; 29 alertas: 12 sort-merge, 2 hash, 8 BNL e 7 seleções.
- Quatro cenários com redução do total de transferências.
- Maior aumento de escritas: dois blocos por cenário. Flush final sem aumento.
- Ambos os motores conferidos com multiconjuntos de tuplas SQLite: 168
  combinações, zero ausências e zero divergências.

`decomposicao-pontuais.csv` deriva dos dois resultados completos preservados:

| Caso | Leituras totais | Leituras de índice | Demais leituras |
|---|---|---|---|
| CPF/hash secundário, LRU | 116 → 195 | 19 → 96 | 97 → 99 |
| CPF/hash secundário, MRU | 119 → 195 | 22 → 97 | 97 → 98 |
| DRE/B+ primária, LRU | 103 → 108 | 1 → 6 | 102 → 102 |
| DRE/B+ primária, MRU | 102 → 202 | 1 → 98 | 101 → 104 |

## Explicação e limites

O efeito já está descrito em `docs/IO.md` e nas decisões 172/173. Na main,
índices usavam a área de índices (16 molduras), adicional às duas molduras
reservadas pela seleção. No PR, páginas de índice e dados disputam M=2.
A B+ medida tem altura 2, uma página interna e 95 folhas; a MRU passa a
expulsar repetidamente a raiz quando precisa carregar outra folha. Os
contadores por tipo confirmam que 97 das 100 leituras adicionais do caso
MRU são de índice. O hash secundário também perde retenção de buckets.

São transferências adicionais efetivas no modelo arquivo↔buffer; não são
apenas contadores novos para leituras anteriormente ignoradas. A comparação
inclui uma mudança de contrato do orçamento, portanto não isola uma
otimização sob a mesma memória efetiva. A área de índices continua existindo
para carga/manutenção, fora da operação medida.

Nos demais grupos, os diretórios auxiliares passam a consumir o orçamento:
menor capacidade para runs/partições e menos cache da interna no BNL.
O diff e a classificação dos alertas são consistentes com as explicações
anteriores em `docs/IO.md`. Não foi feita nova ablação por commit nem
reprodução em escala integral nesta investigação.

O resultado anterior sem mudanças comparava o motor `b1c0de5` com a etapa
final da revisão, sob driver comum v2. Ele não comparava a main com todo o
PR. Portanto, os dois resultados têm referências diferentes.

O CI exige resultados corretos e comparação válida, mas aumentos de I/O
geram alertas, sem reprovar automaticamente o job. Aceitar o custo de cumprir
M não significa desativar esses alertas. A recomendação é preservar o
contrato atual e registrar o custo; qualquer otimização posterior deve
respeitar o mesmo orçamento e passar pela mesma comparação.

## Reprodução

Da raiz do worktree, com saídas novas e bibliotecas das revisões indicadas:

```bash
python3 -I PrototipoIA/Implementacao/tools/medir_io.py \
  --source /tmp/cbd-pr19-review-CUFYlj/base/PrototipoIA/Implementacao \
  --build-dir /tmp/cbd-pr19-review-CUFYlj/base-build \
  --commit 4091762d65a52495c02c9cf63da5bc630998ff4d \
  --saida /tmp/cbd-pr19-io-main-i094l1u3/referencia
python3 -I PrototipoIA/Implementacao/tools/medir_io.py \
  --source "$PWD/PrototipoIA/Implementacao" \
  --build-dir /tmp/cbd-pr19-slot-prefetch-release \
  --commit a6b3d48dd913e12f6aa1623954edd34ed13031ae \
  --saida /tmp/cbd-pr19-io-main-i094l1u3/candidato
python3 -I PrototipoIA/Implementacao/tools/comparar_io.py \
  /tmp/cbd-pr19-io-main-i094l1u3/referencia \
  /tmp/cbd-pr19-io-main-i094l1u3/candidato \
  /tmp/cbd-pr19-io-main-i094l1u3/comparacao
```

Os caminhos acima registram os comandos executados; para repetir, substituir
o diretório de saída por outro novo. CSVs, metadados e logs selecionados
estão preservados aqui. Dados, lotes e dumps completos continuam no
diretório temporário e podem ser regenerados a partir do perfil e da semente.

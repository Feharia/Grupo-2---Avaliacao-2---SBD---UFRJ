# Log de prompts e origem do código (§7.2.3 a)

Este diretório é **código gerado por IA** (Claude Opus 5.5, no Claude Code, 08/11/2026), a partir da arquitetura do `HANDOFF_ARQUITETURA_2026-11-08.md` (decisões 1 a 169). Ele ainda **não passou por revisão do grupo**. O enunciado (§7.2.3 b) pede, no relatório de IA, os fragmentos devolvidos pela IA antes de qualquer modificação manual. Por isso:

1. versione este diretório **como está**, num commit próprio (por exemplo, com a tag `ia-bruto-v1`), antes de qualquer alteração manual;
2. faça as correções do grupo em commits seguintes, citando o arquivo e a linha do código bruto, para alimentar o `POST_MORTEM.md`.

## Prompts usados nesta sessão

Na ordem em que foram dados (texto do usuário, sem edição):

1. Anexos: o PDF do enunciado, `HANDOFF_ARQUITETURA_2026-11-08.md` e `revisao_interfaces_claude_cbd_2026-11-06.md`.
   > Leia documentos 1 e 2, absorva esse contexto. Leia documento 3 de revisão. Analise o que do documento 3 é válido.
2. > Sim, incorporar
   (A IA acrescentou ao handoff as decisões 157 a 169.)
3. Anexo: o diagrama de entidades por camada da versão anterior.
   > Faça novo diagrama atualizado no mesmo estilo do anexado.
   (Gerou `diagrama_camadas_v4.svg` e `.png`.)
4. > Após, faça um prototipo com tudo que temos dentro da pasta /PrototipoIA

O prompt 4 é o que gerou este código. A IA o interpretou assim: implementar a arquitetura completa do handoff (camadas 1 a 4, common/, scripts de conferência, testes e build), compilar, testar e rodar a bancada.

## Correções que a própria IA fez durante a geração

Estas correções já estão no código deste diretório. Elas foram feitas antes de qualquer revisão humana e valem como material para o code review (§7.1, §7.2.3 b):

| Onde | Defeito encontrado | Como apareceu | Correção |
|---|---|---|---|
| `src/layer3/bplus.cpp`, `split_leaf` | A divisão da folha separava ao meio só os registros que já estavam nela, ignorando o registro novo. Uma das folhas ficava com 15 registros, abaixo do mínimo ⌈31/2⌉ = 16 (decisão 37; "árvores B+ que não mantêm a ocupação mínima", §7.1). | `verify()` acusou "folha abaixo da ocupação mínima" depois da carga de manutenção da bancada | A divisão passou a considerar os n + 1 registros, com ⌈(n+1)/2⌉ à esquerda e o separador escolhido de modo que a descida leve o registro novo ao lado certo |
| `tests/mini_test.hpp`, `MINI_CMP` | `const auto&` ligado a `*optional` temporário deixava a referência pendente (*dangling*) | O teste `SlottedPage.SlotsNaoSaoRenumerados` falhou com valores lixo, e o clang avisou `-Wdangling-gsl` | Os valores comparados passaram a ser copiados |
| `src/layer2/algorithms.cpp` | Inicialização parcial de `KeyRange` e uma função não usada | Avisos `-Wmissing-field-initializers` e `-Wunused-function` | Inicialização explícita; função removida |

## Prompts posteriores que mudaram o código

Depois da geração inicial, a IA mudou o código a pedido do usuário. Estas mudanças não fazem parte do código bruto original (§7.2.3 b):

5. > Para os casos 1, 2 e 3 use algo como `nao aplicavel` para eles nos resultados. Registre casos 1 a 4 no .md existente que contém o projeto.
   - Decisão 170 do handoff.
   - `NotApplicable` passou a ter um tipo, `NaoAplicavel` ou `Inviavel`.
   - O `resultados.csv` ganhou as colunas da matriz do enunciado e os status `ok`, `nao_aplicavel` e `inviavel`.
   - A bancada passou a declarar os casos 1 a 3 sem executar.
   - A junção por CPF com Detalhes em hash (tipo 2, configuração d) saiu da execução.
6. > Sim, implementar
   Era a resposta à pergunta "Implemento o particionamento recursivo, com parada para grupos indivisíveis?".
   - Decisão 171 do handoff: hash join com particionamento recursivo, semente por passe, parada e bailout em blocos.
   - Dois testes novos.
   - **Correção feita pela própria IA:** a primeira versão previa o nº de passes com um critério próprio, usando a folga de 1,2. Na rodada completa, o plano previu 2 passes e a execução fez só 1. O estimado das células com B+ ficou em cerca de 17.000 transferências, contra 7.400 medidas. A IA trocou o critério pela fórmula do livro, ⌈log_{m−1}(b_build) − 1⌉, e o estimado passou para cerca de 8.000.

7. > Reavalie a arquitetura, buscando erros
   (Só análise, sem mudança de código.)
8. > otimize com o menor numero de decisoes que nao compromete o resultado do trabalho.
   Nenhuma decisão nova foi criada; as existentes foram emendadas.
   - **108:** tabela menor passa a ser b ≤ m − 2. O BNL usa o chunk do tamanho da externa menor, o hash join não particiona quando o build cabe, e a bancada calcula o tamanho pelas estatísticas.
   - **132:** as repetições usam a tolerância também sobre o tempo de relógio.
   - **143:** blocos densos ⌈n/bfr⌉ nas runs, nas partições e nas estimativas.
   - **146:** a varredura pontual em campo UNIQUE para no primeiro registro.
   - Um teste novo.
   - Inconsistências do handoff corrigidas, e decisões não exercitadas listadas na §9.

## Como verificar o que foi gerado

```bash
make && make test      # 40 testes
make asan              # os mesmos testes com ASan + UBSan
make rapido            # bancada em escala 0,1 + conferência com o SQLite
```

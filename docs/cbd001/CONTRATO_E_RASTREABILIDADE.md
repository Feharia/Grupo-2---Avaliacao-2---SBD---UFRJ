# CBD-001 — contrato e rastreabilidade dos testes

Esta especificação precede a revisão dos testes desta etapa. Ela separa o
enunciado, decisões autorizadas de projeto e configuração física de fixtures.
Nenhum limite de ocupação é obtido de uma função do motor como resultado
esperado. Os testes continuam sendo de integração.

## Autoridade das decisões

Em 12/11/2026, o usuário esclareceu: “Tenho aprovação prévia de todo o grupo
para decidir sozinho esse e outros pontos similares.” A autorização desta
sessão é suficiente para as decisões abaixo. Não se exige ata adicional nem
se atribui ao PDF uma decisão tomada pelo projeto.

Fontes originais consultadas, sob `../codex_context/referencias/` em relação à
raiz do checkout:

- **N:** `ENUNCIADO_04-11-2026.pdf`, oito páginas. §3 (p.2), §3.4 e início do
  §4.2 (p.3), continuação §4.2 (p.4), recomendação de referência após §5.2
  (p.5) e §7.1 (p.7).
- **R:** `registro_decisoes_cbd_fonte_da_verdade_v1.docx`, versão 1 de 07/11,
  D07 e D13–D17/D21: bloco 4096, fixos/variáveis, páginas, RecordView,
  cursores e StorageManager. É um registro de valores definidos, não uma ata.
- **S:** `CBD - Avaliação 2 - Decisões.docx`, tabela Árvore B+ #32–38,
  coluna literalmente denominada “Valor sugerido”. Ela propõe mínimo
  `ceil(n/2)` e bulk load 70%; não comprova ratificação coletiva nem serve
  para impor o fan-out 340 sugerido, incompatível com o layout adotado.

## Propriedades exigidas e decisões do projeto

| ID | Propriedade | Origem e alcance |
|---|---|---|
| N1 | A B+ mantém a ocupação mínima escolhida, encadeia folhas e propaga a estrutura até a raiz | PDF §3.4 e §7.1; o PDF não fornece percentual |
| N2 | B+ primária atende UNIQUE e NOT UNIQUE | PDF §3 e §4.2; inclui preservar cardinalidade e duplicatas admissíveis |
| N3 | Buscas, cursores e manutenção preservam os dados e retornam o predicado solicitado | Semântica das operações do PDF §3/§4.2 e contratos R D17/D21; comparação com referência recomendada após §5.2 |
| D1 | Folha fixa não raiz contém de `ceil(capacidade/2)` até `capacidade` registros | Decisão de projeto autorizada nesta sessão, coerente com proposta S #37 |
| D2 | Folha variável não raiz ocupa ao menos metade da área útil | Decisão de projeto autorizada nesta sessão; bytes úteis = bloco menos cabeçalho; ocupação = bytes dos registros vivos + suas entradas de slot |
| D3 | Interno não raiz tem de `ceil(fan-out máximo/2)` até `fan-out máximo` filhos | Decisão de projeto autorizada; número de chaves = filhos menos um; não confundir com aplicar ceil diretamente ao máximo de chaves |
| D4 | Raiz folha pode ser vazia ou ficar abaixo do mínimo; raiz interna tem pelo menos dois filhos | Decisão de projeto autorizada; a exceção vale somente quando a folha é a árvore inteira |
| D5 | `fill` é meta de preenchimento subordinada às invariantes, para valores finitos em `(0,1]` | Contrato desta correção; meta baixa não autoriza árvore subocupada; testes não impõem distribuição ou altura desnecessárias |
| D6 | Registros variáveis não são divididos entre páginas; caso nenhuma partição ordenada válida exista, a carga falha de forma controlada | Decisão de projeto autorizada; não é promessa de aceitar qualquer sequência de tamanhos nem de rollback transacional |
| D7 | O verificador acusa folha não raiz abaixo do mínimo, inclusive a última | Contrato diagnóstico para operacionalizar N1/D1/D2; teste não usa o próprio verificador para estabelecer a ocupação real |

Estas decisões definem o contrato do CBD-001. Não comprovam, sozinhas, toda a
matriz de organizações, índices e experimentos exigida na avaliação.

## Configuração física independente das fixtures

Os testes de estrutura precisam observar páginas. Para isso se declara um
formato de fixture compatível com o layout vigente, sem chamar funções do
motor para construir seu oráculo. Esses detalhes são **contrato técnico de
integração**, não exigências do professor:

- Bloco 4096 bytes (R D07); cabeçalho 24 bytes; entrada de slot 4 bytes.
- Cabeçalho: nslots u16 no offset2, next u32 no8, prev u32 no12, primeiro
  filho u32 no16, live u16 no20 e level u16 no22.
- Diretório a partir do offset24: pares `(offset u16, length u16)`; length0
  representa slot morto. Slots não são confundidos com ordem física dos dados.
- Internos: primeiro filho no cabeçalho e pares `(chave Int32, filho u32)`
  de 8 bytes após o cabeçalho.
- Inteiros das fixtures na representação nativa do host, como o formato
  vigente; esta revisão não cria requisito de persistência entre endianness.
- Registro fixo: Int32 no offset0 + Char(123) no4 = 127 bytes, sem NULLs.
- Registro variável: Int32 no0, fim do VarChar u16 no4, payload no6.

Derivações feitas aqui, sem consultar `sp_capacity` ou `internal_cap`:

```text
área útil                         = 4096 - 24             = 4072 bytes
capacidade da folha fixa           = floor(4072 / (127+4)) = 31 registros
mínimo da folha fixa               = ceil(31/2)           = 16 registros
mínimo variável                   = ceil(4072/2)         = 2036 bytes
máximo de chaves internas          = floor(4072 / 8)      = 509 chaves
fan-out máximo                    = 509 + 1              = 510 filhos
mínimo de filhos não raiz          = ceil(510/2)          = 255 filhos
mínimo de chaves não raiz          = 255 - 1              = 254 chaves
limite de registros em dois níveis = 31 * 510             = 15810 registros
```

Logo **15.811** registros exigem pelo menos três níveis em qualquer
distribuição dessa fixture. O caso anterior com 10.731 não provava isso:
ele dependia do modo particular de arredondar o fill. O teste será substituído.

Se o layout for legitimamente alterado, atualizam-se esta configuração e
suas fixtures. Os testes de comportamento público continuam tendo como
oráculo os registros lógicos de entrada, não esse layout.

## Rastreabilidade dos cenários

| Teste/cenário | Propriedades | Origem dos dados/resultado esperado |
|---|---|---|
| FolhasRespeitamMinimoCom22Registros | N1/D1/D4/D5 | Regressão da auditoria; mínimo16 da conta acima; não exige duas folhas |
| InternosRespeitamMinimoCom7500Registros | N1/D1/D3 | Regressão; limites literais16/254/509; altura não é fixa |
| VerifyRejeitaUltimaFolhaSubocupada | N1/D7 | Folha reduzida a15 por escrita própria da fixture; leitura própria confirma a corrupção isolada |
| VerifyRejeitaUltimaFolhaVariavelSubocupadaEmBytes | D2/D7 | Vinte registros de custo100 totalizam2000<2036, embora a contagem seja maior que16; isola o critério em bytes |
| RaizFolhaPodeFicarAbaixoDoMinimo | D4 | Entradas conhecidas0/1/15; uma folha é a árvore inteira |
| DuasFolhasValidasPreservamOsRegistros | N1/D1/D3 | Controle42; mínimo16 e máximo31, com conteúdo de entrada conhecido |
| FolhasRespeitamMinimoNosLimitesAdjacentes | N1/D1/D5 | Entradas21/23/31/32/33/42/43; não exige partição exata |
| InternosRespeitamMinimoNosLimitesAdjacentes | N1/D3 | Entradas7499/7501; mínimo254 chaves |
| InternosRespeitamMinimoSemCaudaSubocupada | N1/D3 | Regressão7518 isolando o problema de internos |
| InternosNaoRaizRespeitamMinimoEmTresNiveis | N1/D3 | 15811 derivado de31×510+1, não do algoritmo de construção |
| PreenchimentosExtremosRespeitamMinimo | D1/D3/D5 | Fills0,1/0,5/1,0; metas não substituem mínimos |
| RegistrosVariaveisRespeitamMinimoEmBytes | R D13/D2 | Payloads conhecidos; soma independente de comprimento+4 por slot |
| CargaEReorganizacaoPreservamIndiceSecundario | N3/R D21 | Registros conhecidos e índice como proteção de integração; não é cobertura de todos os métodos de §4.2 |
| VariaveisDesiguaisRepartemMaisDeDuasFolhas | D2/D6 | Sequência conhecida; partição possível2665/2100/2100 bytes, não exige essa partição |
| VariaveisNaoDeixamFolhaIntermediariaSubocupada | N1/D2 | Custos1400/1400/1000/1000/2100; partição possível3800/3100 |
| VariaveisSemParticaoValidaSaoRejeitados | D2/D6 | Custos1000/1000/2100: total4100>4072 e únicos cortes deixam lado<2036 |
| CargaPreservaLinhasEBuscasUnicas (API pública) | N2/N3 | Modelo `std::map` com valores lógicos conhecidos |
| BuscaPorFaixaRespeitaLimites (API pública) | N2/N3 | Chaves pares e intervalo[20,42]; lista esperada20,22,...,42 |
| OrganizacaoNaoUnicaPreservaDuplicatas (API pública) | N2/N3 | Modelo `std::multiset` de48 linhas, seis linhas por chave, antes/depois de reorganizar |

## Independência e limites dos oráculos

- Montagem das entradas: bytes próprios, a partir do formato declarado acima;
  não usa `encode_record` do motor.
- Estrutura: leitor próprio de cabeçalho/slots/internos e ordenação Int32 da
  biblioteca padrão. Esperados literais desta especificação. Não usa
  `sp_capacity`, `internal_cap`, `read_header`, `sp_record`, `sp_used_bytes`
  ou `sorted_slots` para decidir se a estrutura é válida.
- Comportamento: resultados reais obtidos por StorageManager/cursores,
  comparados a modelo lógico independente. RecordView é o resultado observado
  do sistema sob teste, não um gerador de expectativas.
- BufferPool, Schema, codecs e APIs públicas continuam sendo componentes
  exercitados pela integração. Nenhum teste de integração pode ser independente
  do sistema que executa; a independência exigida está na origem das
  expectativas e do oráculo, com dependências explicitadas.
- O verificador do motor é uma verificação adicional. Só no teste específico
  de seu contrato ele é o resultado principal observado.
- As rejeições por inviabilidade não podem ser satisfeitas por `bad_alloc`
  ou `out_of_range` acidentais. A API atual não oferece um código estruturado
  de causa; não se afirma distinção completa de todas as classes de erro.

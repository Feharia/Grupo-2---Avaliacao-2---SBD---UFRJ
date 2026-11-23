# Diagnóstico para congelamento do programa — 23/11/2026

Candidato 4339845; main local 4091762. A avaliação considerou código e
capacidades da bancada, excluindo redação de relatório e documentação.
Plano: docs/superpowers/plans/2026-11-23-congelamento-programa.md.

Release reutilizado em /tmp/cbd-pr19-slot-prefetch-release, com fontes do
motor conferidas contra as assinaturas da medição de I/O do candidato.
CTest: 96 casos principais + 11 de memória aprovados. Python: 63 aprovados,
com BUILD_DIR habilitando integração da CLI. ASan/UBSan e rodada integral
não foram repetidos: a evidência prévia está na revisão do PR #19 e no CI.

Reprodução nova de ocupação: bancada reduzida filtrada por j_pequenas,
em diretório novo, comando preservado em ocupacao-reproduzida.json.
Para heaps não alterados pela manutenção, comparados CSV, cabeçalho/bitmap
e tamanho lógico do arquivo. Curso: 8 usados, 1 livre reservado, 9 totais;
Disciplina: 13 usados, 4 livres reservados, 17 totais. O CSV total coincide
com os usados, mas não disponibiliza a distinção do comprimento lógico.
Cabeçalhos em hexadecimal preservam os bytes necessários à conferência.
Não se trata de medir blocos fisicamente alocados no SSD.

Outra lacuna constatada por leitura de bench.cpp: erro de verify depois da
manutenção só vai ao stderr e a função principal pode terminar com código
zero. Não foi injetada falha nessa etapa; teste negativo e correção foram
planejados. Não se afirma corrupção atual de dados. A referência SQLite
atual usa os CSVs iniciais; testes existentes de manutenção já verificam
RIDs e clustering, e devem ser reaproveitados antes de ampliar cobertura.

Não foram alteradas fontes, headers, testes ou workflow; apenas registros e
plano locais. Nenhum commit, push, merge ou tag nesta avaliação.

# Aceitação do programa em cópia limpa — 23/11/2026

Implementação validada: 8569e0e9f1894b6c647bf72888b4e5c0e9c1de9c.
Base: a59f3336f23f4899667108447c57a8a8e9808544. Checkout Git separado,
limpo antes/depois; dois builds do zero com GoogleTest obrigatório.

- Release: 103 casos principais + 11 de memória aprovados.
- ASan/UBSan (-O1 -g): os mesmos 114 casos aprovados, 54,10 s;
  LeakSanitizer desativado, conforme limite de ambiente já declarado.
- Python: 69 testes aprovados, incluindo CLI normal e falha de manutenção.
- I/O v2: 168 cenários, nenhum delta de leituras, escritas ou flush;
  referência e candidato usam o mesmo driver, seus próprios headers/libs,
  dados/consultas/assinaturas iguais e multiconjuntos conferidos com SQLite.
- Integral escala 1, M=32/2, lote 1000, seed 20261122, LRU/MRU, 3..10
  repetições: 691 linhas medidas em 168 combinações; zero ausências ou
  divergências de contagem/checksum contra SQLite. Não há dumps de tuplas
  da integral; os multiconjuntos completos foram conferidos na reduzida.
- Matriz: 108 células executadas, 12 não aplicáveis, zero inviáveis e
  ausentes; 476 execuções de junção com portes conferidos.
- E/S convergiu em todos os grupos; 11 atingiram o teto apenas por tempo.
  O programa registra essa não convergência temporal, sem declarar sucesso
  de convergência. Nenhum flush final não zero ou erro de manutenção.

A integral rodou após o término dos builds, sanitizers e perfis reduzidos,
sem essas tarefas concorrentes. A duração integral foi 82,45 s. O total de
repetições difere das rodadas históricas porque o critério também usa tempo.

Os CSVs integrais e de I/O, metadados, logs, ambiente e hashes de binários
estão preservados. Dados, lotes, dumps reduzidos e arquivos binários do motor
continuam no diretório temporário de workdir.txt; podem ser regenerados.
run_acceptance.py preserva os comandos executados; seus diretórios de saída
precisam ser novos para uma repetição. Os hashes dos fontes medidos estão
em acceptance.json e medicao.json. Builds não misturam ABIs de Occupancy.

Esta pasta consolida resultados posteriores ao commit de código: sua
publicação pode acrescentar um commit documental, mas deve manter a árvore
PrototipoIA/Implementacao idêntica à implementação validada. Não é necessário
reexecutar a integral só por gravar esses resultados. A integração remota
das correções permanece uma etapa separada, com CI do SHA enviado.

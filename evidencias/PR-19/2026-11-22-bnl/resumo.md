# Comparação de I/O

Referência: árvore de `2f0148c` com as correções anteriores de VarChar e clustering. Candidato: mesma árvore com `correcao.patch` deste diretório. Os hashes das árvores estão em `medicao-before.json` e `medicao-after.json`.
168 cenários pareados; 0 com aumento em alguma métrica.
Aumentos são alertas; contagens representam transferências arquivo↔buffer, não I/O físico do SSD.
Total = leituras + escritas, antes do flush final. Carga e manutenção fora desta versão.

| Política / operação / variante / algoritmo | Leituras antes→depois (Δ) | Escritas antes→depois (Δ) | Total Δ / % | Flush antes→depois |
|---|---:|---:|---:|---:|
| LRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / sort_merge | 145→145 (+0) | 73→73 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / bnl | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / hash | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / sort_merge | 110→110 (+0) | 7→7 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / bnl | 104→104 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / hash | 104→104 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / sort_merge | 106→106 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / bnl | 123→123 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / hash | 123→123 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / sort_merge | 196→196 (+0) | 73→73 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / bnl | 74→74 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / hash | 74→74 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / sort_merge | 141→141 (+0) | 66→66 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / sort_merge | 145→145 (+0) | 73→73 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / bnl | 915→915 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / bnl | 885→885 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / hash | 479→479 (+0) | 244→244 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / bnl | 1115→1115 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / sort_merge | 437→437 (+0) | 171→171 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / bnl | 1559→1559 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / hash | 576→576 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / sort_merge | 341→341 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / bnl | 1691→1691 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / hash | 422→422 (+0) | 66→66 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / sort_merge | 593→593 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / bnl | 1115→1115 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / sort_merge | 437→437 (+0) | 171→171 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / bnl | 915→915 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / bnl | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / hash | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / sort_merge | 135→135 (+0) | 65→65 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / bnl | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / hash | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / sort_merge | 38→38 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:scan / bnl | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:scan / hash | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:scan / sort_merge | 34→34 (+0) | 12→12 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_bt_curso:primary_btree / bnl | 25→25 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_bt_curso:primary_btree / hash | 25→25 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_bt_curso:primary_btree / sort_merge | 27→27 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_h_codigo:primary_hash x disciplina_h_curso:primary_hash / bnl | 28→28 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_h_codigo:primary_hash x disciplina_h_curso:primary_hash / hash | 28→28 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_h_codigo:primary_hash x disciplina_h_curso:primary_hash / sort_merge | 47→47 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:secondary_btree / bnl | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:secondary_btree / hash | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:secondary_btree / sort_merge | 34→34 (+0) | 12→12 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:secondary_hash x disciplina_heap:secondary_btree / bnl | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:secondary_hash x disciplina_heap:secondary_btree / hash | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:secondary_hash x disciplina_heap:secondary_btree / sort_merge | 38→38 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_unique / aluno_heap:scan x detalhes_heap:scan / bnl | 314→314 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_unique / aluno_heap:scan x detalhes_heap:scan / hash | 243→243 (+0) | 124→124 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_unique / aluno_heap:scan x detalhes_heap:scan / sort_merge | 240→240 (+0) | 121→121 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_cpf / aluno_heap:scan /  | 3037→3037 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_cpf / aluno_heap:secondary_hash /  | 195→195 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_dre / aluno_heap:scan /  | 3344→3344 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_dre / aluno_bt_dre:primary_btree /  | 108→108 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_dre / aluno_h_dre:primary_hash /  | 99→99 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_alta / aluno_heap:secondary_btree /  | 964→964 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_baixa / aluno_heap:secondary_btree /  | 22→22 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_bt_curso:primary_btree /  | 49→49 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_bt_curso:clustering_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_h_curso:clustering_btree /  | 158→158 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_bt_curso:primary_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_bt_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_h_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_alta / aluno_bt_dre:primary_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_baixa / aluno_bt_dre:primary_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / sort_merge | 145→145 (+0) | 73→73 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / bnl | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / hash | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / sort_merge | 110→110 (+0) | 7→7 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / bnl | 104→104 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / hash | 104→104 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / sort_merge | 106→106 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / bnl | 123→123 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / hash | 123→123 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / sort_merge | 196→196 (+0) | 73→73 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / bnl | 74→74 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / hash | 74→74 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / sort_merge | 141→141 (+0) | 66→66 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / sort_merge | 145→145 (+0) | 73→73 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / bnl | 863→863 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / bnl | 785→785 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / hash | 479→479 (+0) | 244→244 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / bnl | 1050→1050 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / sort_merge | 437→437 (+0) | 171→171 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / bnl | 1494→1494 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / hash | 576→576 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / sort_merge | 341→341 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / bnl | 1626→1626 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / hash | 422→422 (+0) | 66→66 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / sort_merge | 593→593 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / bnl | 1050→1050 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / sort_merge | 437→437 (+0) | 171→171 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / bnl | 863→863 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / bnl | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / hash | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / sort_merge | 135→135 (+0) | 65→65 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / bnl | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / hash | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / sort_merge | 38→38 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:scan / bnl | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:scan / hash | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:scan / sort_merge | 34→34 (+0) | 12→12 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_bt_curso:primary_btree / bnl | 25→25 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_bt_curso:primary_btree / hash | 25→25 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_bt_curso:primary_btree / sort_merge | 27→27 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_h_codigo:primary_hash x disciplina_h_curso:primary_hash / bnl | 28→28 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_h_codigo:primary_hash x disciplina_h_curso:primary_hash / hash | 28→28 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_h_codigo:primary_hash x disciplina_h_curso:primary_hash / sort_merge | 47→47 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:secondary_btree / bnl | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:secondary_btree / hash | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap:secondary_btree / sort_merge | 34→34 (+0) | 12→12 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:secondary_hash x disciplina_heap:secondary_btree / bnl | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:secondary_hash x disciplina_heap:secondary_btree / hash | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:secondary_hash x disciplina_heap:secondary_btree / sort_merge | 38→38 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_unique / aluno_heap:scan x detalhes_heap:scan / bnl | 278→278 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_unique / aluno_heap:scan x detalhes_heap:scan / hash | 243→243 (+0) | 124→124 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_unique / aluno_heap:scan x detalhes_heap:scan / sort_merge | 240→240 (+0) | 121→121 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_cpf / aluno_heap:scan /  | 2982→2982 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_cpf / aluno_heap:secondary_hash /  | 195→195 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_dre / aluno_heap:scan /  | 3244→3244 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_dre / aluno_bt_dre:primary_btree /  | 202→202 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_dre / aluno_h_dre:primary_hash /  | 99→99 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_alta / aluno_heap:secondary_btree /  | 964→964 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_baixa / aluno_heap:secondary_btree /  | 22→22 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_bt_curso:primary_btree /  | 49→49 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_bt_curso:clustering_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_h_curso:clustering_btree /  | 158→158 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_bt_curso:primary_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_bt_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_h_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_alta / aluno_bt_dre:primary_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_baixa / aluno_bt_dre:primary_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |

# Comparação de I/O

Referência: `4091762d65a52495c02c9cf63da5bc630998ff4d`. Candidato: `a6b3d48dd913e12f6aa1623954edd34ed13031ae`.
168 cenários pareados; 29 com aumento em alguma métrica.
Aumentos são alertas; contagens representam transferências arquivo↔buffer, não I/O físico do SSD.
Total = leituras + escritas, antes do flush final. Carga e manutenção fora desta versão.

| Política / operação / variante / algoritmo | Leituras antes→depois (Δ) | Escritas antes→depois (Δ) | Total Δ / % | Flush antes→depois |
|---|---:|---:|---:|---:|
| LRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_heap_sem_indices:scan / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_heap_sem_indices:scan / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_heap_sem_indices:scan / sort_merge | 144→145 (+1) | 72→73 (+1) | +2 / +0.93% | 0→0 |
| LRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_bt_curso:primary_btree / bnl | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_bt_curso:primary_btree / hash | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_bt_curso:primary_btree / sort_merge | 110→110 (+0) | 7→7 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / bnl | 104→104 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / hash | 104→104 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / sort_merge | 106→106 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / bnl | 123→123 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / hash | 123→123 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / sort_merge | 195→196 (+1) | 72→73 (+1) | +2 / +0.75% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / bnl | 74→74 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / hash | 74→74 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / sort_merge | 140→141 (+1) | 65→66 (+1) | +2 / +0.98% | 0→0 |
| LRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / sort_merge | 144→145 (+1) | 72→73 (+1) | +2 / +0.93% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan / bnl | 915→915 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan (papeis trocados) / bnl | 885→885 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan (papeis trocados) / hash | 477→479 (+2) | 242→244 (+2) | +4 / +0.56% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan (papeis trocados) / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap_sem_indices:scan / bnl | 1115→1115 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap_sem_indices:scan / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap_sem_indices:scan / sort_merge | 438→437 (-1) | 172→171 (-1) | -2 / -0.33% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / bnl | 1559→1559 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / hash | 576→576 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / sort_merge | 341→341 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / bnl | 1691→1691 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / hash | 422→422 (+0) | 66→66 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / sort_merge | 593→593 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / bnl | 1115→1115 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / sort_merge | 438→437 (-1) | 172→171 (-1) | -2 / -0.33% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / bnl | 915→915 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / bnl | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / hash | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / sort_merge | 134→135 (+1) | 64→65 (+1) | +2 / +1.01% | 0→0 |
| LRU / j_pequenas / curso_heap_sem_indices:scan x disciplina_heap_sem_indices:scan / bnl | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap_sem_indices:scan x disciplina_heap_sem_indices:scan / hash | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap_sem_indices:scan x disciplina_heap_sem_indices:scan / sort_merge | 38→38 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap_sem_indices:scan / bnl | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap_sem_indices:scan / hash | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap_sem_indices:scan / sort_merge | 34→34 (+0) | 12→12 (+0) | +0 / +0.00% | 0→0 |
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
| LRU / j_unique / aluno_heap_sem_indices:scan x detalhes_heap:scan / bnl | 314→314 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_unique / aluno_heap_sem_indices:scan x detalhes_heap:scan / hash | 243→243 (+0) | 124→124 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_unique / aluno_heap_sem_indices:scan x detalhes_heap:scan / sort_merge | 238→240 (+2) | 119→121 (+2) | +4 / +1.12% | 0→0 |
| LRU / sel_pt_cpf / aluno_heap_sem_indices:scan /  | 3037→3037 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_cpf / aluno_heap:secondary_hash /  | 116→195 (+79) | 0→0 (+0) | +79 / +68.10% | 0→0 |
| LRU / sel_pt_dre / aluno_heap_sem_indices:scan /  | 3344→3344 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_dre / aluno_bt_dre:primary_btree /  | 103→108 (+5) | 0→0 (+0) | +5 / +4.85% | 0→0 |
| LRU / sel_pt_dre / aluno_h_dre:primary_hash /  | 99→99 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_alta / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_alta / aluno_heap:secondary_btree /  | 960→964 (+4) | 0→0 (+0) | +4 / +0.42% | 0→0 |
| LRU / sel_rg_cr_baixa / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_baixa / aluno_heap:secondary_btree /  | 22→22 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_bt_curso:primary_btree /  | 49→49 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_bt_curso:clustering_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_h_curso:clustering_btree /  | 158→158 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_bt_curso:primary_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_bt_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_h_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_alta / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_alta / aluno_bt_dre:primary_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_baixa / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_baixa / aluno_bt_dre:primary_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_heap_sem_indices:scan / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_heap_sem_indices:scan / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_heap_sem_indices:scan / sort_merge | 144→145 (+1) | 72→73 (+1) | +2 / +0.93% | 0→0 |
| MRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_bt_curso:primary_btree / bnl | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_bt_curso:primary_btree / hash | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap_sem_indices:scan x aluno_bt_curso:primary_btree / sort_merge | 110→110 (+0) | 7→7 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / bnl | 104→104 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / hash | 104→104 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_bt_curso:primary_btree / sort_merge | 106→106 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / bnl | 123→123 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / hash | 123→123 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_h_codigo:primary_hash x aluno_h_curso:primary_hash / sort_merge | 195→196 (+1) | 72→73 (+1) | +2 / +0.75% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / bnl | 74→74 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / hash | 74→74 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_bt_codigo:primary_btree x aluno_heap:secondary_btree / sort_merge | 140→141 (+1) | 65→66 (+1) | +2 / +0.98% | 0→0 |
| MRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:secondary_hash x aluno_heap:secondary_btree / sort_merge | 144→145 (+1) | 72→73 (+1) | +2 / +0.93% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan / bnl | 851→863 (+12) | 0→0 (+0) | +12 / +1.41% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan (papeis trocados) / bnl | 725→785 (+60) | 0→0 (+0) | +60 / +8.28% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan (papeis trocados) / hash | 477→479 (+2) | 242→244 (+2) | +4 / +0.56% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap_sem_indices:scan x inscricao_heap_sem_indices:scan (papeis trocados) / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap_sem_indices:scan / bnl | 1035→1050 (+15) | 0→0 (+0) | +15 / +1.45% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap_sem_indices:scan / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap_sem_indices:scan / sort_merge | 438→437 (-1) | 172→171 (-1) | -2 / -0.33% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / bnl | 1479→1494 (+15) | 0→0 (+0) | +15 / +1.01% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / hash | 576→576 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / sort_merge | 341→341 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / bnl | 1611→1626 (+15) | 0→0 (+0) | +15 / +0.93% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / hash | 422→422 (+0) | 66→66 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / sort_merge | 593→593 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / bnl | 1035→1050 (+15) | 0→0 (+0) | +15 / +1.45% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / sort_merge | 438→437 (-1) | 172→171 (-1) | -2 / -0.33% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / bnl | 851→863 (+12) | 0→0 (+0) | +12 / +1.41% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:secondary_hash x inscricao_heap:secondary_btree / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / bnl | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / hash | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / sort_merge | 134→135 (+1) | 64→65 (+1) | +2 / +1.01% | 0→0 |
| MRU / j_pequenas / curso_heap_sem_indices:scan x disciplina_heap_sem_indices:scan / bnl | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap_sem_indices:scan x disciplina_heap_sem_indices:scan / hash | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap_sem_indices:scan x disciplina_heap_sem_indices:scan / sort_merge | 38→38 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap_sem_indices:scan / bnl | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap_sem_indices:scan / hash | 21→21 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_bt_codigo:primary_btree x disciplina_heap_sem_indices:scan / sort_merge | 34→34 (+0) | 12→12 (+0) | +0 / +0.00% | 0→0 |
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
| MRU / j_unique / aluno_heap_sem_indices:scan x detalhes_heap:scan / bnl | 266→278 (+12) | 0→0 (+0) | +12 / +4.51% | 0→0 |
| MRU / j_unique / aluno_heap_sem_indices:scan x detalhes_heap:scan / hash | 243→243 (+0) | 124→124 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_unique / aluno_heap_sem_indices:scan x detalhes_heap:scan / sort_merge | 238→240 (+2) | 119→121 (+2) | +4 / +1.12% | 0→0 |
| MRU / sel_pt_cpf / aluno_heap_sem_indices:scan /  | 2982→2982 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_cpf / aluno_heap:secondary_hash /  | 119→195 (+76) | 0→0 (+0) | +76 / +63.87% | 0→0 |
| MRU / sel_pt_dre / aluno_heap_sem_indices:scan /  | 3244→3244 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_dre / aluno_bt_dre:primary_btree /  | 102→202 (+100) | 0→0 (+0) | +100 / +98.04% | 0→0 |
| MRU / sel_pt_dre / aluno_h_dre:primary_hash /  | 99→99 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_alta / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_alta / aluno_heap:secondary_btree /  | 951→964 (+13) | 0→0 (+0) | +13 / +1.37% | 0→0 |
| MRU / sel_rg_cr_baixa / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_baixa / aluno_heap:secondary_btree /  | 21→22 (+1) | 0→0 (+0) | +1 / +4.76% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_bt_curso:primary_btree /  | 49→49 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_bt_curso:clustering_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_h_curso:clustering_btree /  | 158→158 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_bt_curso:primary_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_bt_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_h_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_alta / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_alta / aluno_bt_dre:primary_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_baixa / aluno_heap_sem_indices:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_baixa / aluno_bt_dre:primary_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |

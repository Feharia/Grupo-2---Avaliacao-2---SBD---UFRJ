# Comparação de I/O

Referência: `edb9c84502bcfbb79b5fb974ee866b46374214f3`. Candidato: `edb9c84502bcfbb79b5fb974ee866b46374214f3`.
108 cenários pareados; 0 com aumento em alguma métrica.
Aumentos são alertas; contagens representam transferências arquivo↔buffer, não I/O físico do SSD.
Total = leituras + escritas, antes do flush final. Carga e manutenção fora desta versão.

| Política / operação / variante / algoritmo | Leituras antes→depois (Δ) | Escritas antes→depois (Δ) | Total Δ / % | Flush antes→depois |
|---|---:|---:|---:|---:|
| LRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / sort_merge | 144→144 (+0) | 72→72 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / bnl | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / hash | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / sort_merge | 110→110 (+0) | 7→7 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / bnl | 915→915 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / bnl | 885→885 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / hash | 477→477 (+0) | 242→242 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / bnl | 1115→1115 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / sort_merge | 438→438 (+0) | 172→172 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / bnl | 1559→1559 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / hash | 576→576 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / sort_merge | 341→341 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / bnl | 1691→1691 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / hash | 422→422 (+0) | 66→66 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / sort_merge | 593→593 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / bnl | 1115→1115 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / sort_merge | 438→438 (+0) | 172→172 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / bnl | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / hash | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / sort_merge | 134→134 (+0) | 64→64 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / bnl | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / hash | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / sort_merge | 38→38 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_unique / aluno_heap:scan x detalhes_heap:scan / bnl | 314→314 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_unique / aluno_heap:scan x detalhes_heap:scan / hash | 243→243 (+0) | 124→124 (+0) | +0 / +0.00% | 0→0 |
| LRU / j_unique / aluno_heap:scan x detalhes_heap:scan / sort_merge | 238→238 (+0) | 119→119 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_cpf / aluno_heap:scan /  | 3037→3037 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_cpf / aluno_heap:secondary_hash /  | 116→116 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_dre / aluno_heap:scan /  | 3344→3344 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_dre / aluno_bt_dre:primary_btree /  | 103→103 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_pt_dre / aluno_h_dre:primary_hash /  | 99→99 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_alta / aluno_heap:secondary_btree /  | 960→960 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_cr_baixa / aluno_heap:secondary_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_bt_curso:primary_btree /  | 49→49 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_bt_curso:clustering_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_alta / aluno_h_curso:clustering_btree /  | 160→160 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_bt_curso:primary_btree /  | 2→2 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_bt_curso:clustering_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_curso_baixa / aluno_h_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_alta / aluno_bt_dre:primary_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| LRU / sel_rg_dre_baixa / aluno_bt_dre:primary_btree /  | 2→2 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / bnl | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / hash | 72→72 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_heap:scan / sort_merge | 144→144 (+0) | 72→72 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / bnl | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / hash | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_grande_pequena / curso_heap:scan x aluno_bt_curso:primary_btree / sort_merge | 110→110 (+0) | 7→7 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / bnl | 851→851 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / hash | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / bnl | 725→725 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / hash | 477→477 (+0) | 242→242 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_heap:scan x inscricao_heap:scan (papeis trocados) / sort_merge | 472→472 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / bnl | 1035→1035 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:scan / sort_merge | 438→438 (+0) | 172→172 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / bnl | 1479→1479 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / hash | 576→576 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_bt_dre:primary_btree / sort_merge | 341→341 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / bnl | 1611→1611 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / hash | 422→422 (+0) | 66→66 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_h_dre:primary_hash x inscricao_h_dre:primary_hash / sort_merge | 593→593 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / bnl | 1035→1035 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / hash | 502→502 (+0) | 237→237 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_mestre_detalhe / aluno_bt_dre:primary_btree x inscricao_heap:secondary_btree / sort_merge | 438→438 (+0) | 172→172 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / bnl | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / hash | 70→70 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_not_unique / aluno_heap:secondary_btree x turma_heap:secondary_btree / sort_merge | 134→134 (+0) | 64→64 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / bnl | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / hash | 19→19 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_pequenas / curso_heap:scan x disciplina_heap:scan / sort_merge | 38→38 (+0) | 19→19 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_unique / aluno_heap:scan x detalhes_heap:scan / bnl | 266→266 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_unique / aluno_heap:scan x detalhes_heap:scan / hash | 243→243 (+0) | 124→124 (+0) | +0 / +0.00% | 0→0 |
| MRU / j_unique / aluno_heap:scan x detalhes_heap:scan / sort_merge | 238→238 (+0) | 119→119 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_cpf / aluno_heap:scan /  | 2982→2982 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_cpf / aluno_heap:secondary_hash /  | 119→119 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_dre / aluno_heap:scan /  | 3244→3244 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_dre / aluno_bt_dre:primary_btree /  | 102→102 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_pt_dre / aluno_h_dre:primary_hash /  | 99→99 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_alta / aluno_heap:secondary_btree /  | 951→951 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_cr_baixa / aluno_heap:secondary_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_bt_curso:primary_btree /  | 49→49 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_bt_curso:clustering_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_alta / aluno_h_curso:clustering_btree /  | 160→160 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_bt_curso:primary_btree /  | 2→2 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_bt_curso:clustering_btree /  | 3→3 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_curso_baixa / aluno_h_curso:clustering_btree /  | 4→4 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_alta / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_alta / aluno_bt_dre:primary_btree /  | 50→50 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_baixa / aluno_heap:scan /  | 65→65 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |
| MRU / sel_rg_dre_baixa / aluno_bt_dre:primary_btree /  | 2→2 (+0) | 0→0 (+0) | +0 / +0.00% | 0→0 |

# CBD-001 — comparação de desempenho

10 repetições pareadas por tamanho; ordem alternada. Tempos em microssegundos.
Contadores representam transferências arquivo↔buffer, não I/O físico do SSD.

| Registros/layout | Fase | Mediana antes (µs) | Mediana depois (µs) | Delta | Leituras antes/depois | Escritas antes/depois |
|---:|---|---:|---:|---:|---:|---:|
| 22-fixed | load | 20.44 | 13.41 | -34.4% | 1/1 | 5/2 |
| 22-fixed | points | 1008.06 | 903.06 | -10.4% | 3/1 | 0/0 |
| 22-fixed | scan | 7.17 | 6.12 | -14.7% | 2/1 | 0/0 |
| 32-fixed | load | 20.56 | 34.96 | +70.1% | 1/1 | 5/5 |
| 32-fixed | points | 927.89 | 975.57 | +5.1% | 3/3 | 0/0 |
| 32-fixed | scan | 8.28 | 8.37 | +1.1% | 2/2 | 0/0 |
| 7518-fixed | load | 1913.26 | 1908.64 | -0.2% | 1/1 | 363/361 |
| 7518-fixed | points | 4251.41 | 5861.18 | +37.9% | 1051/1049 | 0/0 |
| 7518-fixed | scan | 1438.28 | 1415.80 | -1.6% | 358/358 | 0/0 |
| 20000-fixed | load | 5304.98 | 5269.68 | -0.7% | 1/1 | 959/958 |
| 20000-fixed | points | 5711.98 | 5969.06 | +4.5% | 1052/1052 | 0/0 |
| 20000-fixed | scan | 3782.06 | 3801.11 | +0.5% | 953/952 | 0/0 |
| 64-variable | load | 34.20 | 67.08 | +96.1% | 1/1 | 6/5 |
| 64-variable | points | 1143.43 | 1426.70 | +24.8% | 4/3 | 0/0 |
| 64-variable | scan | 19.63 | 18.69 | -4.8% | 3/2 | 0/0 |
| 5-variable | load | 25.50 | 31.29 | +22.7% | 1/1 | 6/5 |
| 5-variable | points | 691.97 | 614.36 | -11.2% | 4/3 | 0/0 |
| 5-variable | scan | 12.14 | 9.38 | -22.7% | 3/2 | 0/0 |
| 20000-variable | load | 6781.59 | 6765.33 | -0.2% | 1/1 | 959/958 |
| 20000-variable | points | 5772.10 | 5955.59 | +3.2% | 1052/1052 | 0/0 |
| 20000-variable | scan | 5220.12 | 5156.89 | -1.2% | 953/952 | 0/0 |

Variabilidade, mínimos e máximos: `resumo.json`; valores brutos: `amostras.csv`.
A baseline pode violar ocupação mínima; `valid` inclui checagem independente da última folha.
Resultados de pontos e varredura são conferidos em ambas as versões.

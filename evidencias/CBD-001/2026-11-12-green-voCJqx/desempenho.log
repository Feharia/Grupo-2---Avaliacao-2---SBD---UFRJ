# CBD-001 — comparação de desempenho

10 repetições pareadas por tamanho; ordem alternada. Tempos em microssegundos.
Contadores representam transferências arquivo↔buffer, não I/O físico do SSD.

| Registros/layout | Fase | Mediana antes (µs) | Mediana depois (µs) | Delta | Leituras antes/depois | Escritas antes/depois |
|---:|---|---:|---:|---:|---:|---:|
| 22-fixed | load | 20.53 | 13.58 | -33.8% | 1/1 | 5/2 |
| 22-fixed | points | 1000.03 | 902.78 | -9.7% | 3/1 | 0/0 |
| 22-fixed | scan | 7.05 | 5.96 | -15.6% | 2/1 | 0/0 |
| 32-fixed | load | 20.28 | 35.52 | +75.2% | 1/1 | 5/5 |
| 32-fixed | points | 924.46 | 981.64 | +6.2% | 3/3 | 0/0 |
| 32-fixed | scan | 8.24 | 8.40 | +1.9% | 2/2 | 0/0 |
| 7518-fixed | load | 1944.70 | 1910.16 | -1.8% | 1/1 | 363/361 |
| 7518-fixed | points | 4602.78 | 5933.40 | +28.9% | 1051/1049 | 0/0 |
| 7518-fixed | scan | 1417.57 | 1411.84 | -0.4% | 358/358 | 0/0 |
| 20000-fixed | load | 5319.83 | 5305.53 | -0.3% | 1/1 | 959/958 |
| 20000-fixed | points | 5740.62 | 5966.22 | +3.9% | 1052/1052 | 0/0 |
| 20000-fixed | scan | 3838.93 | 3829.05 | -0.3% | 953/952 | 0/0 |
| 64-variable | load | 34.16 | 67.49 | +97.6% | 1/1 | 6/5 |
| 64-variable | points | 1142.38 | 1428.32 | +25.0% | 4/3 | 0/0 |
| 64-variable | scan | 19.20 | 17.96 | -6.5% | 3/2 | 0/0 |
| 5-variable | load | 26.74 | 32.38 | +21.1% | 1/1 | 6/5 |
| 5-variable | points | 686.48 | 615.50 | -10.3% | 4/3 | 0/0 |
| 5-variable | scan | 11.42 | 10.55 | -7.6% | 3/2 | 0/0 |
| 20000-variable | load | 6749.49 | 6780.62 | +0.5% | 1/1 | 959/958 |
| 20000-variable | points | 5734.60 | 5951.93 | +3.8% | 1052/1052 | 0/0 |
| 20000-variable | scan | 5222.82 | 5158.68 | -1.2% | 953/952 | 0/0 |

Variabilidade, mínimos e máximos: `resumo.json`; valores brutos: `amostras.csv`.
A baseline pode violar ocupação mínima; `valid` inclui checagem independente da última folha.
Resultados de pontos e varredura são conferidos em ambas as versões.

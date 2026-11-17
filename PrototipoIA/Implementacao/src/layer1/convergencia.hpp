// Camada 1 — critério de parada das repetições (decisão 132, emendada; P07, BENCH-007).
//
// Repete uma medição até a dispersão das últimas min_reps ficar abaixo da tolerância, na E/S e no
// tempo, com no mínimo min_reps e no máximo max_reps repetições. Parar no teto não é convergir: o
// resumo diz qual critério falhou. Fica num header só para a bancada e os testes usarem o mesmo
// código.
#pragma once

#include <algorithm>
#include <functional>
#include <numeric>
#include <vector>

namespace cbd {

struct Convergence {
    bool executed = false;
    int reps = 0;
    double io_spread = 0, time_spread = 0;
    bool io_ok = false, time_ok = false;
    const char* stop() const {
        if (!executed) return "nao_executada";
        return io_ok && time_ok ? "convergiu" : "teto";
    }
};

// Uma repetição: se executou e, nesse caso, os blocos transferidos e o tempo.
struct Sample {
    bool ok = false;
    double io = 0, time = 0;
};

// (máx − mín) / média das últimas `last` amostras; 0 se a média for 0
inline double spread_of(const std::vector<double>& v, int last) {
    const auto [mn, mx] = std::minmax_element(v.end() - last, v.end());
    const double mean = std::accumulate(v.end() - last, v.end(), 0.0) / last;
    return mean == 0 ? 0.0 : (*mx - *mn) / mean;
}

inline Convergence repeat_until_stable(int min_reps, int max_reps, double tolerance,
                                       const std::function<Sample(int rep)>& once) {
    std::vector<double> io, ms;
    Convergence c;
    for (int rep = 1; rep <= max_reps; ++rep) {
        const Sample s = once(rep);
        c.reps = rep;
        if (!s.ok) return c;
        c.executed = true;
        io.push_back(s.io);
        ms.push_back(s.time);
        if (rep >= min_reps) {
            c.io_spread = spread_of(io, min_reps);
            c.time_spread = spread_of(ms, min_reps);
            c.io_ok = c.io_spread < tolerance;
            c.time_ok = c.time_spread < tolerance;
            if (c.io_ok && c.time_ok) return c;
        }
    }
    return c;
}

}  // namespace cbd

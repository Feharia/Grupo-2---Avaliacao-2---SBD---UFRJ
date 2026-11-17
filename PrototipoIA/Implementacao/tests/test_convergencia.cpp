// BENCH-007 (issue #16): o critério de parada das repetições, com séries determinísticas. A bancada
// usa o mesmo código (layer1/convergencia.hpp); parar no teto não pode ser chamado de convergir.
#include "layer1/convergencia.hpp"
#include "mini_test.hpp"

#include <cstring>
#include <vector>

using namespace cbd;

namespace {

// série fixa: amostra rep → (io[rep − 1], tempo[rep − 1]); se acabar, repete a última
std::function<Sample(int)> serie(std::vector<double> io, std::vector<double> tempo) {
    return [io, tempo](int rep) {
        const std::size_t i = static_cast<std::size_t>(rep - 1);
        return Sample{true, io[std::min(i, io.size() - 1)], tempo[std::min(i, tempo.size() - 1)]};
    };
}

}  // namespace

TEST(Convergencia, SerieEstavelConvergeNoMinimo) {
    const Convergence c = repeat_until_stable(3, 10, 0.05, serie({100}, {10.0}));
    EXPECT_TRUE(c.executed);
    EXPECT_EQ(c.reps, 3);
    EXPECT_TRUE(c.io_ok && c.time_ok);
    EXPECT_TRUE(std::strcmp(c.stop(), "convergiu") == 0);
    EXPECT_TRUE(c.io_spread == 0.0 && c.time_spread == 0.0);
}

TEST(Convergencia, TempoOscilanteParaNoTeto) {   // E/S estável, tempo alternando 10 e 20
    const Convergence c = repeat_until_stable(3, 10, 0.05, [](int rep) {
        return Sample{true, 500.0, rep % 2 ? 10.0 : 20.0};
    });
    EXPECT_EQ(c.reps, 10);
    EXPECT_TRUE(c.io_ok);
    EXPECT_FALSE(c.time_ok);
    EXPECT_TRUE(std::strcmp(c.stop(), "teto") == 0);
    // últimas 3 (reps 8, 9, 10): 20, 10, 20 → (20 − 10) / (50 / 3) = 0,6
    EXPECT_TRUE(c.time_spread > 0.59 && c.time_spread < 0.61);
}

TEST(Convergencia, EsVariavelTambemParaNoTeto) {   // lote que muda a cada repetição
    const Convergence c = repeat_until_stable(3, 6, 0.05, [](int rep) {
        return Sample{true, rep % 2 ? 100.0 : 120.0, 10.0};
    });
    EXPECT_EQ(c.reps, 6);
    EXPECT_FALSE(c.io_ok);
    EXPECT_TRUE(c.time_ok);
    EXPECT_TRUE(std::strcmp(c.stop(), "teto") == 0);
}

TEST(Convergencia, ConvergeDepoisDoAquecimento) {   // 1ª repetição lenta; a janela das últimas 3 estabiliza na 4ª
    const Convergence c = repeat_until_stable(3, 10, 0.05, serie({100}, {50.0, 10.0, 10.2, 10.1}));
    EXPECT_EQ(c.reps, 4);
    EXPECT_TRUE(std::strcmp(c.stop(), "convergiu") == 0);
}

TEST(Convergencia, FalhaNaPrimeiraNaoExecuta) {
    const Convergence c = repeat_until_stable(3, 10, 0.05, [](int) { return Sample{}; });
    EXPECT_FALSE(c.executed);
    EXPECT_EQ(c.reps, 1);
    EXPECT_TRUE(std::strcmp(c.stop(), "nao_executada") == 0);
}

TEST(Convergencia, TetoIgualAoMinimoNaoConvergeSemCriterio) {   // max = min: uma janela só
    const Convergence c = repeat_until_stable(3, 3, 0.05, serie({100, 100, 130}, {10.0}));
    EXPECT_EQ(c.reps, 3);
    EXPECT_FALSE(c.io_ok);
    EXPECT_TRUE(std::strcmp(c.stop(), "teto") == 0);
}

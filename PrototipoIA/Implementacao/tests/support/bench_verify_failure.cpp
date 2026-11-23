// Executa a bancada real com uma falha de verificação na fronteira de manutenção.
// A substituição existe só neste executável de teste; preserva a verificação real.
#include "layer2/monitor.hpp"

class VerificationFailureMonitor : public cbd::Monitor {
public:
    using cbd::Monitor::Monitor;
    std::string verify(cbd::TableId table) const {
        const auto error = cbd::Monitor::verify(table);
        return error.empty() ? "falha_injetada_no_verificador" : error;
    }
};

#define Monitor VerificationFailureMonitor
#define main bench_under_test
#include "../../src/layer1/bench.cpp"
#undef main
#undef Monitor

int main(int argc, char** argv) { return bench_under_test(argc, argv); }

// main do shim de testes (não é usado quando o GoogleTest está disponível).
#include "mini_test.hpp"

#if !defined(CBD_USE_GTEST)
#include <chrono>
#include <cstring>

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0, failed_tests = 0;
    for (const auto& t : mini::registry()) {
        const std::string full = t.suite + "." + t.name;
        if (filter && full.find(filter) == std::string::npos) continue;
        const int before = mini::failures();
        const auto t0 = std::chrono::steady_clock::now();
        std::cout << "[ RODA ] " << full << std::endl;
        try {
            t.body();
        } catch (const mini::AssertFail&) {
        } catch (const std::exception& e) {
            mini::fail(full.c_str(), 0, std::string("exceção não tratada: ") + e.what());
        }
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        ++ran;
        if (mini::failures() != before) {
            ++failed_tests;
            std::cout << "[FALHOU] " << full << " (" << ms << " ms)" << std::endl;
        } else {
            std::cout << "[  OK  ] " << full << " (" << ms << " ms)" << std::endl;
        }
    }
    std::cout << "\n" << ran << " testes, " << failed_tests << " com falha\n";
    return failed_tests == 0 ? 0 : 1;
}
#endif

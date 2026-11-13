// Probe da configuração pública, compilado contra os headers de cada revisão.
// Não instancia o motor nem altera os contadores ou o protocolo de medição.
#include "common/common.hpp"

#include <iostream>

int main() {
    const cbd::Config config;
    std::cout << "{\"block_size\":" << config.block_size
              << ",\"extent_blocks\":" << config.extent_blocks
              << ",\"area_frames\":[" << config.area_frames[0] << ',' << config.area_frames[1]
              << ',' << config.area_frames[2] << ',' << config.area_frames[3]
              << "],\"toss_immediate\":" << (config.toss_immediate ? "true" : "false")
              << ",\"prefetch\":" << (config.prefetch ? "true" : "false") << "}\n";
}

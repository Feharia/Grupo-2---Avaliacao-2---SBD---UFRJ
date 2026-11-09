#pragma once

#include "common/common.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

inline cbd::Config test_config(const std::string& name, cbd::Policy p = cbd::Policy::Lru) {
    cbd::Config c;
    c.policy = p;
    const char* base = std::getenv("CBD_TEST_DIR");
    const auto root = base ? std::filesystem::path(base) : std::filesystem::temp_directory_path() / "cbd_testes";
    c.data_dir = root / name;
    std::filesystem::remove_all(c.data_dir);
    return c;
}

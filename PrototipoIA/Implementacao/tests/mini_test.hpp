// Shim mínimo com a sintaxe do GoogleTest (TEST, EXPECT_*, ASSERT_*). Com CBD_USE_GTEST, usa o GoogleTest real.
#pragma once

#if defined(CBD_USE_GTEST)
#include <gtest/gtest.h>
#else

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mini {

struct TestCase {
    std::string suite, name;
    std::function<void()> body;
};
inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}
inline int& failures() {
    static int f = 0;
    return f;
}
struct Registrar {
    Registrar(const char* s, const char* n, void (*fn)()) { registry().push_back({s, n, fn}); }
};
struct AssertFail {};
inline void fail(const char* file, int line, const std::string& what) {
    ++failures();
    std::cerr << "  FALHOU " << file << ":" << line << ": " << what << "\n";
}
template <class T>
std::string show(const T& v) {
    if constexpr (requires(std::ostream& o, const T& x) { o << x; }) {
        std::ostringstream os;
        os << v;
        return os.str();
    } else {
        return "?";
    }
}

}  // namespace mini

#define TEST(S, N)                                                         \
    static void S##_##N##_body();                                          \
    static ::mini::Registrar S##_##N##_reg(#S, #N, &S##_##N##_body);       \
    static void S##_##N##_body()

#define MINI_CHECK(cond, text, fatal)                                      \
    do {                                                                   \
        if (!(cond)) {                                                     \
            ::mini::fail(__FILE__, __LINE__, text);                        \
            if (fatal) throw ::mini::AssertFail{};                         \
        }                                                                  \
    } while (0)

#define MINI_CMP(a, b, op, fatal)                                                                 \
    do {                                                                                          \
        const auto mini_a_ = (a); /* cópia: (a) pode ser *optional temporário */                  \
        const auto mini_b_ = (b);                                                                 \
        if (!(mini_a_ op mini_b_)) {                                                              \
            ::mini::fail(__FILE__, __LINE__,                                                      \
                         std::string(#a " " #op " " #b " (") + ::mini::show(mini_a_) + " vs " +  \
                             ::mini::show(mini_b_) + ")");                                        \
            if (fatal) throw ::mini::AssertFail{};                                                \
        }                                                                                         \
    } while (0)

#define EXPECT_TRUE(c)  MINI_CHECK((c), #c, false)
#define ASSERT_TRUE(c)  MINI_CHECK((c), #c, true)
#define EXPECT_FALSE(c) MINI_CHECK(!(c), "!(" #c ")", false)
#define ASSERT_FALSE(c) MINI_CHECK(!(c), "!(" #c ")", true)
#define EXPECT_EQ(a, b) MINI_CMP(a, b, ==, false)
#define ASSERT_EQ(a, b) MINI_CMP(a, b, ==, true)
#define EXPECT_NE(a, b) MINI_CMP(a, b, !=, false)
#define EXPECT_LT(a, b) MINI_CMP(a, b, <, false)
#define EXPECT_LE(a, b) MINI_CMP(a, b, <=, false)
#define EXPECT_GT(a, b) MINI_CMP(a, b, >, false)
#define EXPECT_GE(a, b) MINI_CMP(a, b, >=, false)
#define EXPECT_THROW(stmt, ex)                                                     \
    do {                                                                           \
        bool mini_caught_ = false;                                                 \
        try {                                                                      \
            stmt;                                                                  \
        } catch (const ex&) {                                                      \
            mini_caught_ = true;                                                   \
        } catch (...) {                                                            \
        }                                                                          \
        MINI_CHECK(mini_caught_, "esperava " #ex " de: " #stmt, false);            \
    } while (0)
#define EXPECT_NO_THROW(stmt)                                                      \
    do {                                                                           \
        try {                                                                      \
            stmt;                                                                  \
        } catch (const std::exception& mini_e_) {                                  \
            ::mini::fail(__FILE__, __LINE__, std::string("exceção: ") + mini_e_.what()); \
        }                                                                          \
    } while (0)

#endif

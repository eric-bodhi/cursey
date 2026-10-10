#pragma once

// A deliberately tiny test framework: TEST(name) { CHECK(...); CHECK_EQ(a, b);
// } Swap in Catch2/doctest/GoogleTest later if the suite outgrows it.

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace test {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

template <typename T>
std::string show(const T& value) {
    std::ostringstream ss;
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        ss << '"' << value << '"';
    } else {
        ss << value;
    }
    return ss.str();
}

inline void fail(const char* file, const int line, const std::string& what) {
    ++failures();
    std::cerr << "  " << file << ":" << line << ": " << what << "\n";
}

} // namespace test

#define TEST_CONCAT_(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_(a, b)

#define TEST(name)                                                             \
    static void TEST_CONCAT(test_fn_, name)();                                 \
    static const test::Registrar TEST_CONCAT(test_reg_, name)(                 \
        #name, TEST_CONCAT(test_fn_, name));                                   \
    static void TEST_CONCAT(test_fn_, name)()

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            test::fail(__FILE__, __LINE__, "CHECK(" #expr ") failed");         \
        }                                                                      \
    } while (0)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        const auto a_ = (actual);                                              \
        const auto e_ = (expected);                                            \
        if (!(a_ == e_)) {                                                     \
            test::fail(__FILE__, __LINE__,                                     \
                       #actual " == " #expected "\n      got:      " +         \
                           test::show(a_) +                                    \
                           "\n      expected: " + test::show(e_));             \
        }                                                                      \
    } while (0)

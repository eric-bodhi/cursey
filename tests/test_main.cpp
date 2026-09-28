#include "test.h"

#include <cstring>

// Usage: cursey_tests [substring]   runs only tests whose name contains it.
int main(const int argc, char* argv[]) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    int failed_cases = 0;
    for (const auto& c : test::registry()) {
        if (filter && !std::strstr(c.name, filter)) {
            continue;
        }
        const int before = test::failures();
        c.fn();
        ++run;
        if (test::failures() != before) {
            ++failed_cases;
            std::cerr << "FAIL " << c.name << "\n";
        }
    }
    std::cout << run - failed_cases << "/" << run << " tests passed\n";
    return failed_cases == 0 ? 0 : 1;
}

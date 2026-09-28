#pragma once

#include <array>
#include <optional>
#include <string>

struct Register {
    std::string text;
    // Linewise registers hold whole lines ("line1\nline2", no trailing '\n')
    // and are pasted as new lines above/below the cursor line.
    bool linewise = false;
};

// Vim register semantics (subset):
//   ""       unnamed: the last yank or delete
//   "0       last yank
//   "1-"9    last multi-line/linewise deletes, shifted on each delete
//   "-       last small (within one line) delete
//   "a-"z    named; "A-"Z append to the lowercase one
//   "_       black hole: writes are discarded
class Registers {
public:
    static bool is_valid(char name);

    void yank(char name, const Register& reg);
    void del(char name, const Register& reg);
    // Store directly (macros); no unnamed/numbered side effects.
    void set(char name, const Register& reg);

    std::optional<Register> get(char name) const;

private:
    Register* slot(char name);
    const Register* slot(char name) const;
    void write_named(char name, const Register& reg);

    Register m_unnamed;
    Register m_small_delete;
    std::array<Register, 10> m_numbered{};
    std::array<Register, 26> m_named{};
    std::array<bool, 36> m_set{}; // which numbered/named registers hold data
};

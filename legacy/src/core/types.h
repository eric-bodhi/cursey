#pragma once

#include <compare>
#include <cstddef>

// A location in the buffer. Both fields are 0-indexed; `col` is a *byte*
// offset into the line (lines are stored as UTF-8), so it always points at the
// first byte of a character. Convert to display columns with utf8::display_col.
struct Position {
    std::size_t row = 0;
    std::size_t col = 0;

    // Lexicographic: row first, then column.
    auto operator<=>(const Position&) const = default;
};

enum class Mode {
    Normal,
    Insert,
    Visual,
    VisualLine,
    CommandLine, // ':' commands and '/' '?' searches
};

inline bool is_visual(const Mode mode) {
    return mode == Mode::Visual || mode == Mode::VisualLine;
}

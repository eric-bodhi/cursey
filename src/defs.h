#pragma once

#include <cstddef>

struct Cursor {
    std::size_t row = 0;
    std::size_t col = 0;
    std::size_t original_col = 0;

    bool operator==(Cursor& other) {
        return row == other.row && col == other.col;
    }
};

enum class Direction {
    Up,
    Down,
    Left,
    Right,
};

enum class CursorMode {
    Block,
    Bar,
};

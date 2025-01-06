#pragma once

struct Cursor {
    std::size_t row = 0;
    std::size_t col = 0;
    std::size_t original_col = 0;
};

enum class Direction {
    Up,
    Down,
    Left,
    Right,
};

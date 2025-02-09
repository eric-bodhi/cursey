#pragma once

#include <cstddef>
#include <optional>

struct Cursor {
    std::size_t row = 0;
    std::size_t col = 0;
    std::size_t original_col = 0;

    bool operator==(Cursor& other) {
        return row == other.row && col == other.col;
    }
};

struct VisualRange {
    const std::optional<Cursor>& visual_start, visual_end;
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

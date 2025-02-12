#pragma once

#include "../defs.h"
#include "tui.h"
#include <cstddef>

class ViewportManager {
private:
    std::size_t view_offset, max_visible_rows, max_visible_cols;
    TermBoundaries term;

public:
    ViewportManager(TermBoundaries boundaries);

    void update_term_size(TermBoundaries boundaries);

    // 0 to 1-idx
    Cursor model_to_screen(const Cursor& modelPos) const;

    // 1 to 0-idx
    Cursor screenToModel(const Cursor& screenPos) const;

    bool isVisible(const Cursor& modelPos) const;

    void adjust_viewport(const Cursor& modelPos);

    std::size_t get_view_offset() const;

    std::pair<std::size_t, std::size_t> getVisibleRange() const;
    std::size_t get_max_row() const;
    std::size_t get_max_col() const;
};

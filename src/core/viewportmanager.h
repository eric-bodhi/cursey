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

    void updateTerminalSize(TermBoundaries boundaries);

    // 0 to 1-idx
    Cursor modelToScreen(const Cursor& modelPos) const;

    // 1 to 0-idx
    Cursor screenToModel(const Cursor& screenPos) const;

    bool isVisible(const Cursor& modelPos) const;

    void adjustViewPort(const Cursor& modelPos);

    std::size_t getViewOffset() const;

    std::pair<std::size_t, std::size_t> getVisibleRange() const;
    std::size_t getMaxRow() const;
    std::size_t getMaxCol() const;
};

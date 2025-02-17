#include "viewportmanager.h"
#include "../defs.h"
#include "tui.h"
#include <cstddef>

ViewportManager::ViewportManager(const TermBoundaries boundaries)
    : view_offset(0), max_visible_rows(boundaries.max_row - 2),
      max_visible_cols(boundaries.max_col),
      term(boundaries) {
}

void ViewportManager::update_term_size(const TermBoundaries boundaries) {
    term = boundaries;
    max_visible_rows = boundaries.max_row - 2;
    max_visible_cols = boundaries.max_col;
}

Cursor ViewportManager::model_to_screen(const Cursor& modelPos) const {
    return Cursor{modelPos.row - view_offset, modelPos.col,
                  modelPos.original_col};
}

Cursor ViewportManager::screenToModel(const Cursor& screenPos) const {
    return Cursor{screenPos.row + view_offset, screenPos.col,
                  screenPos.original_col};
}

bool ViewportManager::isVisible(const Cursor& modelPos) const {
    return modelPos.row >= view_offset &&
           modelPos.row < view_offset + max_visible_rows;
}

void ViewportManager::adjust_viewport(const Cursor& modelPos) {
    if (modelPos.row < view_offset) {
        view_offset = modelPos.row;
    } else if (modelPos.row > view_offset + max_visible_rows - 1) {
        view_offset = modelPos.row - (max_visible_rows - 1);
    }
}

std::size_t ViewportManager::get_view_offset() const {
    return view_offset;
}

std::pair<std::size_t, std::size_t> ViewportManager::getVisibleRange() const {
    return {view_offset, view_offset + max_visible_rows};
}

// viewportmanager.h
std::size_t ViewportManager::get_max_row() const {
    return term.max_row;
}
std::size_t ViewportManager::get_max_col() const {
    return term.max_col;
}

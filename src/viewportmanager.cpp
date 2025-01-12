#include "viewportmanager.h"
#include "defs.h"
#include <cstddef>

ViewportManager::ViewportManager(std::size_t max_rows, std::size_t max_cols)
    : view_offset(0), max_visible_rows(max_rows), max_visible_cols(max_cols) {
}

Cursor ViewportManager::modelToScreen(const Cursor& modelPos) const {
    return Cursor{
        modelPos.row - view_offset + 1,
        modelPos.col + 1,
        modelPos.original_col + 1
    };
}

Cursor ViewportManager::screenToModel(const Cursor& screenPos) const {
    return Cursor{
        screenPos.row + view_offset - 1,
        screenPos.col - 1,
        screenPos.original_col - 1
    };
}

bool ViewportManager::isVisible(const Cursor& modelPos) const {
    return modelPos.row >= view_offset && modelPos.row < view_offset + max_visible_rows;
}

void ViewportManager::adjustViewPort(const Cursor& modelPos) {
    if (modelPos.row < view_offset) {
        view_offset = modelPos.row;
    }
    else if (modelPos.row >= view_offset + max_visible_rows) {
        view_offset = modelPos.row - max_visible_rows + 1;
    }
}

std::size_t ViewportManager::getViewOffset() const { return view_offset; }

std::pair<std::size_t, std::size_t> ViewportManager::getVisibleRange() const {
    return {view_offset, view_offset + max_visible_rows};
}


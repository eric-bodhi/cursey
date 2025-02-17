#include "cursor.h"
#include "../defs.h"

CursorManager::CursorManager(Buffer &buffer, const Cursor &arg_cursor)
    : m_cursor(arg_cursor), m_buffer(buffer) {}

void CursorManager::move_dir(const Direction direction) {
    switch (direction) {
    case Direction::Up:
        if (m_cursor.row > 0) {
            if (m_buffer.get_line_length(m_cursor.row - 1) == 0) {
                m_cursor.col = 0;
            } else if (m_buffer.get_line_length(m_cursor.row - 1) - 1 <
                       m_cursor.original_col) {
                m_cursor.col = m_buffer.get_line_length(m_cursor.row - 1) - 1;
            } else if (m_buffer.get_line_length(m_cursor.row - 1) >=
                       m_cursor.original_col) {
                m_cursor.col = m_cursor.original_col;
            }
            --m_cursor.row;
        }
        break;

    case Direction::Down:
        if (m_cursor.row < m_buffer.line_count() - 1) {
            if (m_buffer.get_line_length(m_cursor.row + 1) == 0) {
                m_cursor.col = 0;
            } else if (m_buffer.get_line_length(m_cursor.row + 1) - 1 <=
                       m_cursor.original_col) {
                m_cursor.col = m_buffer.get_line_length(m_cursor.row + 1) - 1;
            } else if (m_buffer.get_line_length(m_cursor.row + 1) >=
                       m_cursor.original_col) {
                m_cursor.col = m_cursor.original_col;
            }
            ++m_cursor.row;
        }
        break;

    case Direction::Left:
        if (m_cursor.col > 0) {
            --m_cursor.col;
            m_cursor.original_col = m_cursor.col;
        }
        break;

    case Direction::Right:
        if (m_cursor.col < m_buffer.get_line_length(m_cursor.row)) {
            ++m_cursor.col;
            m_cursor.original_col = m_cursor.col;
        }
        break;
    }
}

void CursorManager::move_abs(const Cursor& pos) {
    if (pos.row <= m_buffer.line_count() &&
        pos.col < m_buffer.get_line_length(pos.row)) {
        m_cursor.col = pos.col;
        m_cursor.row = pos.row;
    }
}

const Cursor& CursorManager::get() {
    return m_cursor;
}

const Cursor& CursorManager::get() const {
    return m_cursor;
}

[[maybe_unused]] Cursor CursorManager::get_one_idx() const {
    return Cursor(m_cursor.row + 1, m_cursor.col + 1,
                  m_cursor.original_col + 1);
}

std::size_t CursorManager::row() const {
    return m_cursor.row;
}

std::size_t CursorManager::col() const {
    return m_cursor.col;
}

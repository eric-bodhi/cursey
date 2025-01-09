#include "cursor.h"
#include "defs.h"

CursorManager::CursorManager(TextBuffer& buffer, std::size_t max_r, const Cursor& argCursor)
    : m_buffer(buffer), max_row(max_r), m_cursor(argCursor) {};

void CursorManager::moveDir(Direction direction) {
    switch (direction) {
    case Direction::Up:
        if (m_cursor.row > 1) {
            if (m_buffer.getLineLength(m_cursor.row - 1) <
                m_cursor.original_col) {
                m_cursor.col = m_buffer.getLineLength(m_cursor.row - 1);
            } else if (m_buffer.getLineLength(m_cursor.row - 1) >=
                       m_cursor.original_col) {
                m_cursor.col = m_cursor.original_col;
            }
            --m_cursor.row;
        }
        break;

    case Direction::Down:
        if (m_cursor.row < max_row) {
            if (m_buffer.getLineLength(m_cursor.row + 1) <
                m_cursor.original_col) {
                m_cursor.col = m_buffer.getLineLength(m_cursor.row + 1);
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
        if (m_cursor.col <= m_buffer.getLineLength(m_cursor.row)) {
            ++m_cursor.col;
            m_cursor.original_col = m_cursor.col;
        }
        break;
    }
}

void CursorManager::moveAbs(const Cursor& argCursor) {
    m_cursor.col = argCursor.col;
    m_cursor.row = argCursor.row;
}

const Cursor& CursorManager::get() {
    return m_cursor;
}

const Cursor& CursorManager::get() const {
    return m_cursor;
}

const Cursor CursorManager::getOneIdx() {
    return Cursor(m_cursor.row + 1, m_cursor.col + 1, m_cursor.original_col + 1);
}

const Cursor CursorManager::getOneIdx() const {
    return Cursor(m_cursor.row + 1, m_cursor.col + 1, m_cursor.original_col + 1);
}

#pragma once

#include <cstddef>
#include "defs.h"
#include "textbuffer.h"

/*
 0 indexed (converted to 1 indexed for graphics)
 Handles all movement and position validations
*/

// forward decl
class TextBuffer;

class CursorManager {
private:
    Cursor m_cursor;
    TextBuffer& m_buffer; // Reference to Editor's Textbuffer
    size_t view_offset = 0;
    size_t max_row; // Max Visible rows on screen

public:
    CursorManager(TextBuffer& buffer, std::size_t max_r, const Cursor& argCursor = Cursor());

    const Cursor& get();
    const Cursor& get() const;

    void moveDir(Direction direction);
    void moveAbs(const Cursor& pos);
};

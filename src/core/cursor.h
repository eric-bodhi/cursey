#pragma once

#include "../defs.h"
#include "buffer.h"
#include <cstddef>

/*
 0 indexed (converted to 1 indexed for graphics)
 Handles all movement and position validations
*/

// forward decl
class Buffer;

class CursorManager {
private:
    Cursor m_cursor;
    Buffer& m_buffer;
    size_t max_row;
    Logger logger = Logger("../logfile.txt");

public:
    CursorManager(Buffer& buffer, std::size_t max_r,
                  const Cursor& arg_cursor = Cursor());

    const Cursor& get();
    const Cursor& get() const;

    Cursor get_one_idx();
    Cursor get_one_idx() const;

    void move_dir(Direction direction);
    void move_abs(const Cursor& pos);
};

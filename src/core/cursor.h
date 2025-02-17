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
    Logger logger = Logger("../logfile.txt");

public:
    explicit CursorManager(Buffer &buffer, const Cursor &arg_cursor = Cursor());

    const Cursor& get();
    const Cursor& get() const;

    std::size_t row() const;
    std::size_t col() const;
    [[maybe_unused]] Cursor get_one_idx() const;

    void move_dir(Direction direction);
    void move_abs(const Cursor& pos);
};

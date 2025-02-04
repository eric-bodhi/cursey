#pragma once

#include "cursor.h"
#include "textbuffer.h"
#include <ncurses.h>

struct TermBoundaries {
    std::size_t max_row, max_col;
};

class Cursey {
private:
    WINDOW* main_win;    // Main content window
    WINDOW* cmd_win;     // Command line window
    std::size_t max_row; // Terminal dimensions
    std::size_t max_col;

public:
    explicit Cursey();
    ~Cursey();

    void render_file(const Cursor& cursor, const TextBuffer& buffer,
                     std::size_t view_offset);
    void render_command_line(const std::string& command);
    void clear_screen();
    TermBoundaries get_terminal_size();

    WINDOW* get_cmd_win();
};

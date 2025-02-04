#pragma once

#include <ncurses.h>
#include "cursor.h"
#include "textbuffer.h"

class Cursey {
private:
    WINDOW* main_win;      // Main content window
    WINDOW* cmd_win;       // Command line window
    std::size_t max_row;   // Terminal dimensions
    std::size_t max_col;

public:
    explicit Cursey();
    ~Cursey();

    void render_file(const Cursor& cursor, const TextBuffer& buffer, std::size_t view_offset);
    void render_command_line(const std::string& command);
    void clear_screen();
    std::pair<std::size_t, std::size_t> get_terminal_size();
};

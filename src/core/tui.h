#pragma once

#include "../defs.h"
#include "textbuffer.h"
#include <notcurses/notcurses.h>
#include <string>
#include <cmath>

struct TermBoundaries {
    std::size_t max_row;
    std::size_t max_col;
};

class NotcursesTUI {
private:
    struct notcurses* nc;
    struct ncplane* stdplane;
    struct ncplane* main_plane;
    struct ncplane* line_plane;
    struct ncplane* tool_plane;
    struct ncplane* cmd_plane;

    std::size_t max_row;
    std::size_t max_col;
    std::size_t max_line_col;
    std::size_t line_number_length;

    std::size_t lengthofsize_t(std::size_t value) const;
    void create_planes();
    Logger logger = Logger("../logfile.txt");

public:
    NotcursesTUI(const TextBuffer& buffer);
    ~NotcursesTUI();

    void resize(std::size_t line_count);
    void render_file(const Cursor& cursor, const TextBuffer& buffer, std::size_t view_offset);
    void render_tool_line(const Cursor& cursor);
    void render_command_line(const std::string& command);

    TermBoundaries get_terminal_size() const;
    int getch();
    void setCursorMode(CursorMode mode);
};

#pragma once

#include "../defs.h"
#include "textbuffer.h"
#include <notcurses/notcurses.h>
#include <string>

// A simple struct to hold terminal dimensions.
struct TermBoundaries {
    std::size_t max_row;
    std::size_t max_col;
};

class NotcursesTUI {
private:
    struct notcurses* nc;
    struct ncplane* stdplane;
    struct ncplane* main_plane; // Main content plane
    struct ncplane* line_plane; // Line number plane
    struct ncplane* tool_plane; // Tool line plane
    struct ncplane* cmd_plane;  // Command line plane
    int max_row;
    int max_col;
    int max_line_col; // line_plane columns
    Logger logger = Logger("../logfile.txt");

public:
    NotcursesTUI();
    ~NotcursesTUI();

    void resize(const std::size_t line_plane);
    // Renders the text file from the buffer with the given view offset.
    void render_file(const Cursor& cursor, const TextBuffer& buffer,
                     std::size_t view_offset);
    // Renders the tool line (for status info like cursor position)
    void render_tool_line(const Cursor& cursor);
    // Renders a command line prompt with the given command string.
    void render_command_line(const std::string& command);

    // Returns the terminal size.
    TermBoundaries get_terminal_size();
    // Returns a character from input (blocking).
    int getch();

    // Optionally set the cursor mode using ANSI escape sequences.
    void setCursorMode(CursorMode mode);
};

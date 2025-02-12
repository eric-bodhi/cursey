#pragma once

#include "../defs.h"
#include "textbuffer.h"
#include <cmath>
#include <notcurses/notcurses.h>
#include <string>
#include <optional>

struct TermBoundaries {
    std::size_t max_row;
    std::size_t max_col;
};

class NotcursesTUI {
private:
    struct notcurses* nc;
    struct ncplane* stdplane;
    struct ncplane* main_plane{};
    struct ncplane* line_plane{};
    struct ncplane* tool_plane{};
    struct ncplane* cmd_plane{};

    std::size_t max_row;
    std::size_t max_col;
    std::size_t max_line_col;
    std::size_t line_number_length;

    static std::size_t lengthofsize_t(std::size_t value) ;
    void create_planes();
    Logger logger = Logger("../logfile.txt");
    const std::string filename;

public:
    NotcursesTUI(const TextBuffer& buffer, std::string_view file);
    ~NotcursesTUI();

    void resize(std::size_t line_count);
    void render_file(const Cursor& cursor, const TextBuffer& buffer,
                     std::size_t view_offset,
                     const std::optional<Cursor>& visual_start,
                     const std::optional<Cursor>& visual_end);
    void render_tool_line(const Cursor& cursor, const bool& was_modified);
    void render_command_line(const std::string& command);
    void render_message(const std::string& message);

    TermBoundaries get_terminal_size() const;
    int get_char();
    static void set_cursor_mode(CursorMode mode);
};

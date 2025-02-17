#pragma once

#include "../defs.h"
#include "buffer.h"
#include <cmath>
#include <notcurses/notcurses.h>
#include <optional>
#include <string>

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

    static std::size_t lengthofsize_t(std::size_t value);
    void create_planes();
    void destroy_planes() const;
    Logger logger = Logger("../logfile.txt");
    const std::string filename;

public:
    NotcursesTUI(const Buffer& buffer, std::string_view file);
    ~NotcursesTUI();

    // both return true if need to destroy/recreate planes
    bool resize_by_lineno(std::size_t line_count);
    bool resize_by_term();
    void resize(std::size_t line_count);

    void render_file(const Cursor& cursor, const Buffer& buffer,
                     std::size_t view_offset,
                     const std::optional<Cursor>& visual_start,
                     const std::optional<Cursor>& visual_end);
    void render_tool_line(const Cursor& cursor, const bool& was_modified) const;
    void render_command_line(const std::string& command) const;
    void render_message(const std::string& message) const;
    bool is_selected(const Cursor& pos, const Cursor& start, const Cursor& end);

    TermBoundaries get_terminal_size() const;
    int get_char() const;
    static void set_cursor_mode(CursorMode mode);
};

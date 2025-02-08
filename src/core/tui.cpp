#include "tui.h"
#include "../defs.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <notcurses/notcurses.h>
#include <string>

// Constructor: Initialize Notcurses and create the planes.
NotcursesTUI::NotcursesTUI() {
    notcurses_options opts = {};
    nc = notcurses_init(&opts, stdout);
    if (nc == nullptr) {
        std::fprintf(stderr, "Error initializing Notcurses\n");
        std::exit(EXIT_FAILURE);
    }
    // Enable the hardware cursor
    notcurses_cursor_enable(nc, 0, 4);
    stdplane = notcurses_stdplane(nc);

    // Retrieve dimensions as unsigned ints
    unsigned int u_max_row = 0, u_max_col = 0;
    ncplane_dim_yx(stdplane, &u_max_row, &u_max_col);
    max_row = u_max_row;
    max_col = u_max_col;
    max_line_col = 4;

    // Create the main plane (offset 4 columns for line numbers, leaving 2 rows
    // at the bottom)
    ncplane_options main_opts{};
    main_opts.y = 0;
    main_opts.x = max_line_col;
    main_opts.rows = max_row - 2;
    main_opts.cols = max_col - max_line_col;
    main_opts.userptr = nullptr;
    main_opts.flags = 0;
    main_plane = ncplane_create(stdplane, &main_opts);

    // Create the line number plane (4 columns wide)
    ncplane_options line_opts{};
    line_opts.y = 0;
    line_opts.x = 0;
    line_opts.rows = max_row - 2;
    line_opts.cols = max_line_col;
    line_opts.userptr = nullptr;
    line_opts.flags = 0;
    line_plane = ncplane_create(stdplane, &line_opts);
    uint64_t line_number_channel = 0;
    ncchannels_set_fg_rgb(&line_number_channel, 0xFFFF00);
    ncplane_set_base(line_plane, " ", 0, line_number_channel);

    // Create the tool line plane (for status messages)
    ncplane_options tool_opts{};
    tool_opts.y = max_row - 2;
    tool_opts.x = 0;
    tool_opts.rows = 1;
    tool_opts.cols = max_col;
    tool_opts.userptr = nullptr;
    tool_opts.flags = 0;
    tool_plane = ncplane_create(stdplane, &tool_opts);

    // Create the command line plane
    ncplane_options cmd_opts{};
    cmd_opts.y = max_row - 1;
    cmd_opts.x = 0;
    cmd_opts.rows = 1;
    cmd_opts.cols = max_col;
    cmd_opts.userptr = nullptr;
    cmd_opts.flags = 0;
    cmd_plane = ncplane_create(stdplane, &cmd_opts);
}

// Destructor: Shutdown Notcurses.
NotcursesTUI::~NotcursesTUI() {
    notcurses_stop(nc);
}

// Resize planes according to file length
// Longer the line number = more horizontal space for line plane
void NotcursesTUI::resize(const std::size_t line_count) {
    // length of the max line number. e.g. 100 = 3
    const std::size_t line_number_length = trunc(std::log10(line_count)) + 1;
    if (line_number_length == max_line_col) { // no difference in line number lengths
        return;
    }

    ncplane_resize(line_plane,
                   0, 0, // preserve from (0,0) in the old plane
                   max_row - 2, max_col - line_number_length, // preserve full content
                   0, 0, // place it at 0, 0
                   max_row - 2, line_number_length); // new dimensions

    ncplane_resize(main_plane,
                   0, max_line_col,
                   max_row - 2, max_col - line_number_length,
                   0, line_number_length,
                   max_row - 2, max_col - line_number_length);
    max_line_col = line_number_length;
}

// Render the tool line, displaying (for example) the current cursor
// coordinates.
void NotcursesTUI::render_tool_line(const Cursor& cursor) {
    ncplane_erase(tool_plane);
    std::string cords =
        std::to_string(cursor.row) + "," + std::to_string(cursor.col);
    int x = static_cast<int>(max_col - cords.size());
    // Use ncplane_printf_yx to print at row 0, column x
    ncplane_printf_yx(tool_plane, 0, x, "%s", cords.c_str());
    notcurses_render(nc);
}

// Render the file’s text from the TextBuffer onto the main and line planes.
void NotcursesTUI::render_file(const Cursor& cursor, const TextBuffer& buffer,
                               std::size_t view_offset) {
    ncplane_erase(main_plane);
    ncplane_erase(line_plane);
    resize(buffer.lineCount());
    for (int i = 0; i < max_row - 2; ++i) {
        std::size_t line_index = i + view_offset;
        if (line_index >= buffer.lineCount())
            break;
        std::string line = buffer.getLine(line_index);
        std::string lineNumber = std::to_string(line_index + 1);
        ncplane_printf_yx(line_plane, i, 0, "%s", lineNumber.c_str());
        ncplane_printf_yx(main_plane, i, 0, "%s", line.c_str());
    }
    render_tool_line({cursor.row + view_offset + 1, cursor.col});

    // Move the cursor on the main_plane (adjusting for the view offset)
    int target_row = static_cast<int>(cursor.row - view_offset);
    ncplane_cursor_move_yx(main_plane, target_row, cursor.col);
    notcurses_cursor_enable(nc, cursor.row, cursor.col + 4);
    notcurses_render(nc);
}

// Render a command prompt (for example, when entering “:w” or “:q”).
void NotcursesTUI::render_command_line(const std::string& command) {
    ncplane_erase(cmd_plane);
    ncplane_printf_yx(cmd_plane, 0, 0, ":%s", command.c_str());
    notcurses_render(nc);
}

// Return the terminal dimensions.
TermBoundaries NotcursesTUI::get_terminal_size() {
    return {static_cast<std::size_t>(max_row),
            static_cast<std::size_t>(max_col)};
}

int NotcursesTUI::getch() {
    ncinput ni;
    // For blocking input, pass nullptr for the timespec.
    uint32_t ret = notcurses_get(nc, nullptr, &ni);
    if (ret == 0) {
        return -1; // no input or error
    }
    // Return the input id from the ncinput structure.
    return static_cast<int>(ni.id);
}

// Set the cursor mode. Notcurses does not offer a direct API for changing the
// cursor shape, so we can fall back on ANSI escape sequences.
void NotcursesTUI::setCursorMode(CursorMode mode) {
    switch (mode) {
    case CursorMode::Block:
        std::printf("\033[2 q");
        break;
    case CursorMode::Bar:
        std::printf("\033[6 q");
        break;
    }
    std::fflush(stdout);
}

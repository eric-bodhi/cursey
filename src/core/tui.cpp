#include "tui.h"
#include "../defs.h"
#include "lex.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <notcurses/notcurses.h>
#include <string>

std::size_t NotcursesTUI::lengthofsize_t(std::size_t value) {
    if (value == 0)
        return 1;
    return static_cast<std::size_t>(std::log10(value)) + 1;
}

void NotcursesTUI::create_planes() {
    // Main content plane (right of line numbers)
    ncplane_options main_opts{};
    main_opts.y = 0;
    main_opts.x = static_cast<int>(max_line_col);
    main_opts.rows = static_cast<int>(max_row - 2);
    main_opts.cols = static_cast<int>(max_col - max_line_col);
    main_plane = ncplane_create(stdplane, &main_opts);
    uint64_t main_channel = 0;
    ncchannels_set_bg_rgb(&main_channel, 0x282C34);
    ncchannels_set_fg_rgb(&main_channel, 0xABB2BF);
    ncplane_set_base(main_plane, " ", 0, main_channel);

    // Line number plane (left side)
    ncplane_options line_opts{};
    line_opts.y = 0;
    line_opts.x = 0;
    line_opts.rows = static_cast<int>(max_row - 2);
    line_opts.cols = static_cast<int>(max_line_col);
    line_plane = ncplane_create(stdplane, &line_opts);
    uint64_t line_number_channel = 0;
    ncchannels_set_bg_rgb(&line_number_channel, 0x282C34);
    ncchannels_set_fg_rgb(&line_number_channel, 0x5C6370);
    ncplane_set_base(line_plane, " ", 0, line_number_channel);

    // Status/tool line plane (bottom - 2)
    ncplane_options tool_opts{};
    tool_opts.y = static_cast<int>(max_row - 2);
    tool_opts.x = 0;
    tool_opts.rows = 1;
    tool_opts.cols = static_cast<int>(max_col);
    tool_plane = ncplane_create(stdplane, &tool_opts);
    uint64_t tool_channel = 0;
    ncchannels_set_bg_rgb(&tool_channel, 0x282C34);
    ncplane_set_base(tool_plane, " ", 0, tool_channel);

    // Command line plane (bottom - 1)
    ncplane_options cmd_opts{};
    cmd_opts.y = static_cast<int>(max_row - 1);
    cmd_opts.x = 0;
    cmd_opts.rows = 1;
    cmd_opts.cols = static_cast<int>(max_col);
    cmd_plane = ncplane_create(stdplane, &cmd_opts);
    uint64_t cmd_channel = 0;
    ncchannels_set_bg_rgb(&cmd_channel, 0x282C34);
    ncplane_set_base(cmd_plane, " ", 0, cmd_channel);
}

void NotcursesTUI::destroy_planes() {
    ncplane_destroy(main_plane);
    ncplane_destroy(line_plane);
    ncplane_destroy(tool_plane);
    ncplane_destroy(cmd_plane);
}

NotcursesTUI::NotcursesTUI(const TextBuffer& buffer, std::string_view file)
    : filename(file) {
    notcurses_options opts{};
    nc = notcurses_init(&opts, stdout);
    if (!nc) {
        std::fprintf(stderr, "Error initializing Notcurses\n");
        std::exit(EXIT_FAILURE);
    }
    notcurses_cursor_enable(nc, 0, 4);
    stdplane = notcurses_stdplane(nc);

    unsigned int u_rows = 0, u_cols = 0;
    ncplane_dim_yx(stdplane, &u_rows, &u_cols);
    max_row = static_cast<std::size_t>(u_rows);
    max_col = static_cast<std::size_t>(u_cols);

    // Initialize line number area based on buffer contents
    const std::size_t line_count = buffer.line_count();
    line_number_length = line_count > 0 ? lengthofsize_t(line_count) : 1;
    max_line_col = line_number_length + 2; // Fixed padding of 2 columns
    create_planes();
}

NotcursesTUI::~NotcursesTUI() {
    notcurses_stop(nc);
}

bool NotcursesTUI::resize_by_lineno(std::size_t line_count) {
    const std::size_t new_number_length =
        line_count > 0 ? lengthofsize_t(line_count) : 1;
    if (new_number_length == line_number_length)
        return false;

    line_number_length = new_number_length;
    max_line_col = new_number_length + 2; // Maintain fixed padding
    return true;
}

bool NotcursesTUI::resize_by_term() {
    bool needs_resize = false;
    unsigned int u_rows = 0, u_cols = 0;
    ncplane_dim_yx(stdplane, &u_rows, &u_cols);
    if (max_row != u_rows) {
        max_row = u_rows;
        needs_resize = true;
    }
    if (max_col != u_cols) {
        max_col = u_cols;
        needs_resize = true;
    }
    return needs_resize;
}

void NotcursesTUI::resize(std::size_t line_count) {
    if (resize_by_term() || resize_by_lineno(line_count)) {
        destroy_planes();
        create_planes();
    }
}

void NotcursesTUI::render_file(const Cursor& cursor, const TextBuffer& buffer,
                               std::size_t view_offset,
                               const std::optional<Cursor>& visual_start,
                               const std::optional<Cursor>& visual_end) {
    ncplane_erase(main_plane);
    ncplane_erase(line_plane);
    resize(buffer.line_count());
    for (std::size_t i = 0; i < max_row - 2; ++i) {
        const std::size_t line_index = i + view_offset;
        if (line_index >= buffer.line_count())
            break;

        // Line numbers
        std::string line_num = std::to_string(line_index + 1);
        ncplane_printf_yx(
            line_plane, static_cast<int>(i),
            static_cast<int>(max_line_col - line_num.length() - 1), "%s",
            line_num.c_str());

        // Text content with syntax highlighting
        std::string line_text = buffer.get_line(line_index);
        lex::highlight_line(line_text, [&](int col, TokenType type, char c) {
            bool selected = false;
            if (visual_start && visual_end) {
                Cursor actual_start = *visual_start;
                Cursor actual_end = *visual_end;

                if (actual_start.row > actual_end.row ||
                    (actual_start.row == actual_end.row &&
                     actual_start.col > actual_end.col)) {
                    std::swap(actual_start, actual_end);
                }

                if (line_index >= actual_start.row &&
                    line_index <= actual_end.row) {
                    if (actual_start.row == actual_end.row) {
                        selected =
                            (col >= actual_start.col && col <= actual_end.col);
                    } else {
                        if (line_index == actual_start.row) {
                            selected = (col >= actual_start.col);
                        } else if (line_index == actual_end.row) {
                            selected = (col <= actual_end.col);
                        } else {
                            selected = true;
                        }
                    }
                }
            }

            uint64_t channels = 0;
            ncchannels_set_fg_rgb(&channels, lex::color_map[type]);
            ncchannels_set_bg_rgb(&channels,
                                  selected ? lex::selection_bg : lex::bg_rgb);

            nccell cell = {};
            cell.gcluster = c;
            cell.channels = channels;
            ncplane_putc_yx(main_plane, i, col, &cell);
        });
    }

    // Cursor handling
    const int cursor_row = static_cast<int>(cursor.row - view_offset);
    const int cursor_col = static_cast<int>(cursor.col);
    ncplane_cursor_move_yx(main_plane, cursor.row, cursor_col);
    notcurses_cursor_enable(nc, cursor.row,
                            cursor_col + static_cast<int>(max_line_col));
    render_tool_line(cursor, buffer.is_modified());
}

void NotcursesTUI::render_tool_line(const Cursor& cursor,
                                    const bool& was_modified) {
    ncplane_erase(tool_plane);
    const std::string pos_str =
        std::to_string(cursor.row + 1) + "," + std::to_string(cursor.col + 1);
    ncplane_printf_yx(tool_plane, 0, 0, "%s", filename.c_str());
    if (was_modified) {
        ncplane_printf_yx(tool_plane, 0, filename.size() + 1, "%s", "[+]");
    }
    ncplane_printf_yx(tool_plane, 0,
                      static_cast<int>(max_col - pos_str.length()), "%s",
                      pos_str.c_str());
    notcurses_render(nc);
}

void NotcursesTUI::render_command_line(const std::string& command) {
    ncplane_erase(cmd_plane);
    ncplane_printf_yx(cmd_plane, 0, 0, ":%s", command.c_str());
    notcurses_render(nc);
}

void NotcursesTUI::render_message(const std::string& message) {
    ncplane_erase(cmd_plane);
    ncplane_printf_yx(cmd_plane, 0, 0, "%s", message.c_str());
    notcurses_render(nc);
}

TermBoundaries NotcursesTUI::get_terminal_size() const {
    return {max_row, max_col};
}

int NotcursesTUI::get_char() {
    ncinput ni;
    return static_cast<int>(notcurses_get(nc, nullptr, &ni));
}

void NotcursesTUI::set_cursor_mode(CursorMode mode) {
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

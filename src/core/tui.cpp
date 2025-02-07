#include "tui.h"
#include <ncurses.h>

Cursey::Cursey() {
    initscr();            // Initialize ncurses
    cbreak();             // Disable line buffering
    noecho();             // Don't echo input
    keypad(stdscr, TRUE); // Enable special keys
    start_color();
    refresh();

    init_pair(1, COLOR_YELLOW, COLOR_BLACK); // line number colors
    getmaxyx(stdscr, max_row, max_col);
    main_win = newwin(max_row - 2, max_col, 0, 4);
    line_win = newwin(max_row - 2, 4, 0, 0);
    tool_win = newwin(1, max_col, max_row - 2, 0);
    cmd_win = newwin(1, max_col, max_row - 1, 0);

    wbkgd(line_win, COLOR_PAIR(1));
    wclear(line_win);
    wrefresh(line_win);
}

Cursey::~Cursey() {
    delwin(main_win);
    delwin(tool_win);
    delwin(cmd_win);
    endwin();
}

void Cursey::render_tool_line(const Cursor& cursor) {
    wclear(tool_win);
    const std::string cords =
        std::to_string(cursor.row) + "," + std::to_string(cursor.col);
    // max_col - cords.size() ensures enough space for cords
    mvwaddstr(tool_win, 0, max_col - cords.size(), cords.c_str());
    wrefresh(tool_win);
}

void Cursey::render_file(const Cursor& cursor, const TextBuffer& buffer,
                         std::size_t view_offset) {
    wclear(main_win);
    wclear(line_win);
    // Render visible lines
    for (std::size_t i = 0; i < max_row - 1; i++) {
        if (i + view_offset >= buffer.lineCount())
            break;

        const std::string line = buffer.getLine(i + view_offset);
        const std::string lineNumber = std::to_string(i + 1);
        mvwaddstr(line_win, i, 3 - lineNumber.size(), lineNumber.c_str());
        mvwaddnstr(main_win, i, 0, line.c_str(), max_col);
    }

    render_tool_line(cursor);

    // Move physical cursor
    wmove(main_win, cursor.row - view_offset, cursor.col);
    wrefresh(line_win);
    wrefresh(main_win);
}

void Cursey::render_command_line(const std::string& command) {
    wclear(cmd_win);
    mvwprintw(cmd_win, 0, 0, ":%s", command.c_str());
    wrefresh(cmd_win);
}

TermBoundaries Cursey::get_terminal_size() {
    return {max_row, max_col};
}

WINDOW* Cursey::get_cmd_win() {
    return cmd_win;
}

#include "tui.h"

Cursey::Cursey() {
    initscr();            // Initialize ncurses
    cbreak();             // Disable line buffering
    noecho();             // Don't echo input
    keypad(stdscr, TRUE); // Enable special keys
    refresh();

    getmaxyx(stdscr, max_row, max_col);
    main_win = newwin(max_row - 1, max_col, 0, 0);
    cmd_win = newwin(1, max_col, max_row - 1, 0);
}

Cursey::~Cursey() {
    delwin(main_win);
    delwin(cmd_win);
    endwin();
}

void Cursey::render_file(const Cursor& cursor, const TextBuffer& buffer,
                         std::size_t view_offset) {
    wclear(main_win);

    // Render visible lines
    for (std::size_t i = 0; i < max_row - 1; i++) {
        if (i + view_offset >= buffer.lineCount())
            break;

        const std::string line = buffer.getLine(i + view_offset);
        mvwaddnstr(main_win, i, 0, line.c_str(), max_col);
    }

    // Move physical cursor
    wmove(main_win, cursor.row - view_offset, cursor.col);
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

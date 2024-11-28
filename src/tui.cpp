#include "tui.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdlib>

// TermManager constructor to initialize raw mode
TermManager::TermManager() {
    enableRawMode();
}

// TermManager destructor to restore terminal settings
TermManager::~TermManager() {
    disableRawMode();
}

// Enables raw mode in the terminal
void TermManager::enableRawMode() {
    tcgetattr(STDIN_FILENO, &origTermios);  // Get current terminal attributes
    struct termios raw = origTermios;

    // Disable canonical mode, echo, and signal generation
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    // Disable Ctrl-S/Ctrl-Q flow control
    raw.c_iflag &= ~(IXON);
    // Disable automatic carriage returns
    raw.c_oflag &= ~(OPOST);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);  // Apply new terminal attributes
}

// Disables raw mode and restores original terminal settings
void TermManager::disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);  // Restore original attributes
}

// Gets the current terminal size
Position TermManager::get_terminal_size() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return {w.ws_row, w.ws_col};
}

Cursey::Cursey(const std::string& filepath)
    : buffer(filepath), cursor({1, 1}), view_offset(0) {
    // Now, after 'tm' is fully initialized, get the terminal size
    boundary = tm.get_terminal_size();
    max_row = boundary.row - 1;
    max_col = boundary.col - 1;
}

// Clears the terminal screen
void Cursey::clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clear entire screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // Move cursor to top-left
}

// Moves the cursor to the specified position
void Cursey::move_cursor(const Position pos) {
    std::string seq = "\x1b[" + std::to_string(pos.row) + ";" + std::to_string(pos.col) + "H";
    write(STDOUT_FILENO, seq.c_str(), seq.size());
}

// Moves the cursor in the specified direction
void Cursey::move(Direction direction) {
    switch (direction) {
    case Direction::Up:
        if (cursor.row > 1)
            --cursor.row;
        else if (view_offset > 0)
            --view_offset;  // Scroll up if at the top
        break;

    case Direction::Down:
        if (cursor.row < max_row)
            ++cursor.row;
        else if (view_offset + max_row < buffer.lineCount())
            ++view_offset;  // Scroll down if at the bottom
        break;

    case Direction::Left:
        if (cursor.col > 1)
            --cursor.col;
        break;

    case Direction::Right:
        if (cursor.col < max_col)
            ++cursor.col;
        break;
    }

    refresh_screen();
}

// Renders the content of the file on the screen
void Cursey::render_file() {
    clear_screen();

    for (std::size_t i = 0; i < max_row && i + view_offset < buffer.lineCount(); ++i) {
        const std::string& line = buffer.getLine(i + view_offset);
        write(STDOUT_FILENO, line.c_str(), std::min(line.size(), max_col));
        write(STDOUT_FILENO, "\r\n", 2); // Move to the next line
    }
}

// Refreshes the screen (re-renders the file and moves the cursor)
void Cursey::refresh_screen() {
    render_file();
    move_cursor(cursor);
}

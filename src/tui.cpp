#include "tui.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <ranges>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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
    tcgetattr(STDIN_FILENO, &origTermios); // Get current terminal attributes
    struct termios raw = origTermios;

    // Disable canonical mode, echo, and signal generation
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    // Disable Ctrl-S/Ctrl-Q flow control
    raw.c_iflag &= ~(IXON);
    // Disable automatic carriage returns
    raw.c_oflag &= ~(OPOST);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Apply new terminal attributes
}

// Disables raw mode and restores original terminal settings
void TermManager::disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH,
              &origTermios); // Restore original attributes
}

// Gets the current terminal size
Position TermManager::get_terminal_size() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return {w.ws_row, w.ws_col};
}

Cursey::Cursey(const std::string& filepath)
    : buffer(filepath), cursor({1, 1}), view_offset(0),
      boundary(tm.get_terminal_size()), max_row(boundary.row - 1),
      max_col(boundary.col - 1) {
}

// Clears the terminal screen
void Cursey::clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clear entire screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // Move cursor to top-left
}

// Moves the cursor to the specified position
void Cursey::move_cursor(const Position pos) {
    std::string seq =
        "\x1b[" + std::to_string(pos.row) + ";" + std::to_string(pos.col) + "H";
    write(STDOUT_FILENO, seq.c_str(), seq.size());
}

// Moves the cursor in the specified direction
void Cursey::move(Direction direction) {
    bool needs_refresh =
        false; // flag deciding if full screen refresh is needed
    line_length = buffer.getLineLength(cursor.row - 1); // Adjust the line length for the current row
    switch (direction) {
    case Direction::Up:
        if (cursor.row > 1) {
            --cursor.row;
        } else if (view_offset > 0) {
            --view_offset; // Scroll up if at the top
            needs_refresh = true;
        }
        break;

    case Direction::Down:
        if (cursor.row < max_row) {
            ++cursor.row;
        }

        else if (view_offset + max_row < buffer.lineCount()) {
            ++view_offset; // Scroll down if at the
                           // bottom
            needs_refresh = true;
        }
        break;

    case Direction::Left:
        if (cursor.col > 1)
            --cursor.col;
        break;

    case Direction::Right:
        // Prevent moving right if at the end of the line
        if (cursor.col < line_length) {
            ++cursor.col;
        }
        break;
    }


    if (needs_refresh) {
        render_file();
    }
    move_cursor(cursor);
}

// Renders the content of the file on the screen
// TODO optimize so only renders changed lines and not whole file
void Cursey::render_file() {
    clear_screen();

    for (std::size_t i = 0; i < max_row && i + view_offset < buffer.lineCount();
         ++i) {
        const std::string& line = buffer.getLine(i + view_offset);
        write(STDOUT_FILENO, line.c_str(), std::min(line.size(), max_col));
        write(STDOUT_FILENO, "\r\n", 2); // Move to the next line
        fflush(stdout);
    }
    move_cursor(cursor);
}

Position Cursey::zeroIdxCursor() {
    return {cursor.row - 1, cursor.col - 1};
}

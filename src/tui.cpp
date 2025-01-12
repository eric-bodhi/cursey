#include "tui.h"
#include "defs.h"
#include "textbuffer.h"
#include <algorithm>
#include <cstdlib>
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
Cursor TermManager::get_terminal_size() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return {w.ws_row, w.ws_col};
}

Cursey::Cursey(const std::string& filepath)
    : view_offset(0), boundary(tm.get_terminal_size()),
      max_row(boundary.row - 1), max_col(boundary.col - 1) {
}

// Clears the terminal screen
void Cursey::clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clear entire screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // Move cursor to top-left
}

// Moves the cursor to the specified position
void Cursey::render_cursor(const Cursor& cursor, const TextBuffer& buffer) {
    // Adjust view_offset if the cursor is out of view
    if (cursor.row < 1 && view_offset > 0) {
        --view_offset;               // Scroll up
        render_file(cursor, buffer); // Re-render to adjust the view
    } else if (cursor.row > max_row &&
               view_offset + max_row < buffer.lineCount()) {
        ++view_offset;               // Scroll down
        render_file(cursor, buffer); // Re-render to adjust the view
    }

    std::string seq = "\x1b[" + std::to_string(cursor.row) + ";" +
                      std::to_string(cursor.col) + "H";
    write(STDOUT_FILENO, seq.c_str(), seq.size());
}

void Cursey::render_file(const Cursor& cursor, const TextBuffer& buffer) {
    clear_screen();

    // Render the visible portion of the buffer
    for (std::size_t i = 0; i < max_row && i + view_offset < buffer.lineCount(); ++i) {
        const std::string& line = buffer.getLine(i + view_offset);
        write(STDOUT_FILENO, line.c_str(), std::min(line.size(), max_col));
        write(STDOUT_FILENO, "\r\n", 2); // Move to the next line
    }

    // The cursor will always be placed after rendering
    std::string seq = "\x1b[" + std::to_string(cursor.row) + ";" +
                      std::to_string(cursor.col) + "H";
    write(STDOUT_FILENO, seq.c_str(), seq.size());
}

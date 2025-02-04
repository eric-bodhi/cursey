#include "tui.h"
#include "defs.h"
#include "textbuffer.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
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
    : boundary(tm.get_terminal_size()), max_row(boundary.row - 1),
      max_col(boundary.col - 1) {
}

// Clears the terminal screen
void Cursey::clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clear entire screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // Move cursor to top-left
}

// Moves the cursor to the specified position
void Cursey::render_cursor(const Cursor& cursor) {
    std::string seq = "\x1b[" + std::to_string(cursor.row) + ";" +
                      std::to_string(cursor.col) + "H";
    write(STDOUT_FILENO, seq.c_str(), seq.size());
}

void Cursey::render_file(const Cursor& cursor, const TextBuffer& buffer,
                         std::size_t view_offset) {
    clear_screen();

    // Render the visible portion of the buffer
    for (std::size_t i = 0;
         i < max_row - 1 && i + view_offset < buffer.lineCount(); ++i) {
        const std::string& line = buffer.getLine(i + view_offset);
        write(STDOUT_FILENO, line.c_str(), std::min(line.size(), max_col));
        write(STDOUT_FILENO, "\r\n", 2); // Move to the next line
    }
    render_cursor(cursor);
}

void Cursey::move_cursor_command_line() {
    char move_cursor[32];
    snprintf(move_cursor, sizeof(move_cursor), "\x1b[%zu;1H", max_row);
    write(STDOUT_FILENO, move_cursor, std::strlen(move_cursor));
    write(STDOUT_FILENO, "\x1b[K", 3); // Clear line
}

void Cursey::render_command_line(const std::string& command) {
    // Move to command line row (1-based) and clear line
    char move_cursor[32];
    move_cursor_command_line();

    // Build and write command string
    const std::string colon_command = ":" + command;
    std::size_t write_length = std::min(colon_command.size(), max_col);
    write(STDOUT_FILENO, colon_command.c_str(), write_length);

    write(STDOUT_FILENO, move_cursor, std::strlen(move_cursor));
}

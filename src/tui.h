#pragma once

#include "textbuffer.h"
#include <cstddef>
#include <string>
#include <termios.h>

// Structure to represent a position on the terminal screen
struct Position {
    std::size_t row;
    std::size_t col;
};

// Manages the terminal settings (raw mode, terminal size, etc.)
class TermManager {
public:
    TermManager();
    ~TermManager();

    // Get the current terminal size (rows and columns)
    Position get_terminal_size();

private:
    struct termios origTermios; // Store original terminal settings

    void enableRawMode();  // Enable raw mode to disable terminal line buffering
    void disableRawMode(); // Disable raw mode and restore original settings
};

// Enum to define the possible movement directions for the cursor
enum class Direction {
    Up,
    Down,
    Left,
    Right,
};

// Handles the rendering and cursor movement for the text editor
class Cursey {
private:
    Position cursor;   // Current cursor position
    TermManager tm;    // Terminal manager for terminal operations
    TextBuffer buffer; // Buffer to hold the text content

    const Position boundary;   // Terminal size boundaries
    const std::size_t max_row; // Max rows in the terminal
    const std::size_t max_col; // Max columns in the terminal

    std::size_t view_offset; // Tracks the top line displayed on the screen

    std::size_t line_length; // current line length

    friend class Editor;
public:
    // Constructor to initialize Cursey with a file path
    Cursey(const std::string& filepath);

    // Clears the terminal screen
    void clear_screen();

    // Moves the cursor to the given position
    void move_cursor(const Position pos);

    // Moves the cursor in the specified direction
    void move(Direction direction);

    // Renders the content of the file on the screen
    void render_file();

    // Refreshes the screen (re-renders the file and moves the cursor)
    void refresh_screen();
};

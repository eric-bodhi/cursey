#pragma once

#include "cursor.h"
#include "textbuffer.h"
#include <string>
#include <termios.h>

// Manages the terminal settings (raw mode, terminal size, etc.)
class TermManager {
public:
    TermManager();
    ~TermManager();

    // Get the current terminal size (rows and columns)
    Cursor get_terminal_size();

private:
    struct termios origTermios; // Store original terminal settings

    void enableRawMode();  // Enable raw mode to disable terminal line buffering
    void disableRawMode(); // Disable raw mode and restore original settings
};

// Handles the rendering and cursor movement for the text editor
class Cursey {
private:
    TermManager tm; // Terminal manager for terminal operations

    const Cursor boundary;     // Terminal size boundaries
    const std::size_t max_row; // Max rows in the terminal
    const std::size_t max_col; // Max columns in the terminal

    friend class Editor;

public:
    // Constructor to initialize Cursey with a file path
    explicit Cursey(const std::string& filepath);

    // Clears the terminal screen
    void clear_screen();

    // Moves the cursor to the given position
    void render_cursor(const Cursor& cursor);

    // Renders the content of the file on the screen
    void render_file(const Cursor& cursor, const TextBuffer& buffer, std::size_t view_offset);
};

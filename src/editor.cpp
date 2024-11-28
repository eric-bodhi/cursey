#include "editor.h"
#include "tui.h"
#include <complex>
#include <unistd.h>

Editor::Editor(const std::string& filepath) : cursey(filepath) {
}

void Editor::normalMode(const char input) {
    logger.log("normal mode");
    switch (input) {
    // TODO fix exit
    case 'q':
        break;

    case 'h':
        cursey.move(Direction::Left);
        break;

    case 'j':
        cursey.move(Direction::Down);
        break;

    case 'k':
        cursey.move(Direction::Up);
        break;

    case 'l':
        cursey.move(Direction::Right);
        break;

    case 'i':
        currMode = Mode::Insert;
        break;
        /*
        case ':':
            commandMode(input);
            break;
        */
    }
}

void Editor::insertMode(const char input) {
    std::string log_message =
        std::to_string(cursey.cursor.row) + " " +
        std::to_string(cursey.cursor.col) + " " +
        std::string(1, input); // Convert input to a string
    logger.log(log_message);

    if (input == 27) { // ESC key to switch to normal mode
        logger.log("Switching to Normal mode. Cursor at: " +
                   std::to_string(cursey.cursor.row) + " " +
                   std::to_string(cursey.cursor.col));
        currMode = Mode::Normal;
        return;
    }
    else if (input == 127) { // Delete key (ASCII 127)
        if (cursey.cursor.col < cursey.buffer.lineCount()) {
            // Remove the character at the cursor position
            cursey.buffer.eraseAt(cursey.zeroIdxCursor());
            cursey.render_file(); // Re-render after the change
            cursey.move(Direction::Left);
        }
    } else {
        // Insert the character at the cursor position
        cursey.buffer.insertAt(cursey.zeroIdxCursor(), input);
        cursey.render_file();
        cursey.move(Direction::Right); // Move right after insertion
    }
}

void Editor::run() {
    char c;
    cursey.render_file();
    while (true) {
        read(STDIN_FILENO, &c, 1);
        if (currMode == Mode::Normal) {
            if (c == 'q') {
                break;
            }
            normalMode(c);

        } else if (currMode == Mode::Insert) {
            insertMode(c);
        }
    }
}

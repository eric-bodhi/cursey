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
    if (input == 27) { // ESC key to switch to normal mode
        logger.log("Switching to Normal mode. Cursor at: " +
                   std::to_string(cursey.cursor.row) + " " +
                   std::to_string(cursey.cursor.col));
        currMode = Mode::Normal;
        return;
    } else if (input == 127) { // Delete key (ASCII 127)
        if (cursey.cursor.col <=
            cursey.buffer.getLineLength(cursey.cursor.row - 1)) {
            logger.log("Del " + std::to_string(cursey.cursor.row) + " " +
                       std::to_string(cursey.cursor.col));
            cursey.buffer.eraseAt(cursey.zeroIdxCursor());
            cursey.render_file(); // Re-render after the change
            cursey.move(Direction::Left);
        }
    } else {
        // Insert the character at the cursor position
        std::string log_message = "Ins " + std::to_string(cursey.cursor.row) +
                                  " " + std::to_string(cursey.cursor.col) +
                                  " " + std::string(1, input);
        logger.log(log_message);
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
                for (std::size_t i = 0; i < cursey.buffer.lineCount(); i++) {
                    logger.log(cursey.buffer.getLine(i));
                }
                break;
            }
            normalMode(c);

        } else if (currMode == Mode::Insert) {
            insertMode(c);
        }
    }
}

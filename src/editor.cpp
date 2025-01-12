#include "editor.h"
#include "cursor.h"
#include "tui.h"
#include <string>
#include <unistd.h>

Editor::Editor(const std::string& filepath)
    : cursey(filepath), buffer(filepath), cm(buffer, cursey.max_row),
      viewport(cursey.max_row, cursey.max_col) {
}

void Editor::normalMode(const char input) {
    auto oldCursor = cm.get();

    switch (input) {
    case 'q':
        break;

    case 'h':
        cm.moveDir(Direction::Left);
        break;

    case 'j':
        cm.moveDir(Direction::Down);
        break;

    case 'k':
        cm.moveDir(Direction::Up);
        break;

    case 'l':
        cm.moveDir(Direction::Right);
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

    auto newCursor = cm.get();
    if (oldCursor.row != newCursor.row || oldCursor.col != newCursor.col) {
        updateView();
    }
}

void Editor::insertMode(const char input) {
    auto cursorpos = cm.get();
    if (input == 27) { // ESC key to switch to normal mode
        logger.log("Switching to Normal mode. Cursor at: " +
                   std::to_string(cursorpos.row) + " " +
                   std::to_string(cursorpos.col));
        currMode = Mode::Normal;
        return;
    }
    else if (input == 127) { // Delete key (ASCII 127)
        if (cursorpos.col > 0) {
            logger.log("Del " + std::to_string(cursorpos.row) + " " +
                       std::to_string(cursorpos.col));
            buffer.erase(cm);
            cm.moveDir(Direction::Left);
            updateView();
        }
    } else {
        // Insert the character at the cursor position
        std::string log_message = "Ins " + std::to_string(cm.get().row) + " " +
                                  std::to_string(cm.get().col) + " " +
                                  std::string(1, input);

        logger.log(log_message);
        buffer.insert(cm, input);
        cm.moveDir(Direction::Right);
        updateView();
    }
}

void Editor::run() {
    char c;
    updateView();
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

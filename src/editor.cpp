#include "tui.h"
#include <unistd.h>
#include "editor.h"

Editor::Editor(const std::string& filepath) : cursey(filepath) {}

void Editor::normalMode(const char input) {
    switch (input) {
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
    if (input == 27) { // esc key
        currMode = Mode::Normal;
        return;
    }

    cursey.buffer.insertAt(cursey.zeroIdxCursor(), input);
    cursey.move(Direction::Right); // input char move right one
}

void Editor::run() {
    char c;
    while (true) {
        cursey.refresh_screen();

        read(STDIN_FILENO, &c, 1);
        if (currMode == Mode::Normal) {
            logger.log("normal mode");
            normalMode(c);
        } else if (currMode == Mode::Insert) {
            logger.log("insert mode");
            insertMode(c);
        }

        if (c == 'q') {
            break;
        }
    }
}

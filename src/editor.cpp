#include "tui.h"
#include "editor.h"

Editor::Editor(const std::string& filepath) : cursey(filepath) {}

void Editor::normalMode(const char input) {
    switch (input) {
    case 'q':
        break;

    case 'h':
        cursey.move(Direction::Left);

    case 'j':
        cursey.move(Direction::Down);

    case 'k':
        cursey.move(Direction::Up);

    case 'l':
        cursey.move(Direction::Right);

    case 'i':
        insertMode(input);

    case ':':
        commandMode(input);
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

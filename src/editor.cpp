#include "editor.h"
#include "commands.h"
#include "cursor.h"
#include "textbuffer.h"
#include "tui.h"
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

Editor::Editor(const std::string& filepath)
    : cursey(filepath), buffer(filepath), cm(buffer, cursey.max_row),
      viewport(cursey.max_row, cursey.max_col), m_filepath(filepath) {
}

void Editor::writeFile() {
    std::ofstream file(m_filepath,
                       std::ios::trunc); // `std::ios::trunc` clears the file
    if (!file.is_open()) {
        std::cerr << "Failed to open existing file: " << m_filepath << '\n';
        return;
    }

    for (std::size_t i = 0; i < buffer.lineCount(); i++) {
        file << buffer.getLine(i) << '\n';
    }

    file.close();
}

// returns shouldExit
bool Editor::normalMode(const char input) {
    auto oldCursor = cm.get();

    logger.log("Normal mode: " +
               std::to_string(oldCursor.row) + " " +
               std::to_string(oldCursor.col));
    switch (input) {
    case 'q':
        writeFile();
        return true;

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

    case ':':
        currMode = Mode::Command;
        break;
    }

    auto newCursor = cm.get();
    if (oldCursor.row != newCursor.row || oldCursor.col != newCursor.col) {
        updateView();
    }

    return false;
}

void Editor::insertMode(const char input) {
    auto cursorpos = cm.get();
    if (input == 27) { // ESC key to switch to normal mode
        logger.log("Switching to Normal mode. Cursor at: " +
                   std::to_string(cursorpos.row) + " " +
                   std::to_string(cursorpos.col));
        currMode = Mode::Normal;
        return;
    } else if (input == 127) { // Delete key (ASCII 127)
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

void Editor::commandMode(const std::string& command) {
    // maybe table of "command" -
}

TextBuffer& Editor::getBuffer() {
    return buffer;
}

void Editor::updateView() {
    auto modelCursor = cm.get();
    viewport.adjustViewPort(modelCursor);
    auto screenCursor = viewport.modelToScreen(modelCursor);
    cursey.render_file(screenCursor, buffer, viewport.getViewOffset());
}

void Editor::execute(std::unique_ptr<Command> command) {
    command->execute(*this);
}

void Editor::run() {
    char c;
    updateView();
    bool shouldExit = false;
    while (!shouldExit) {
        read(STDIN_FILENO, &c, 1);
        if (currMode == Mode::Normal) {
            shouldExit = normalMode(c);
        } else if (currMode == Mode::Insert) {
            insertMode(c);
        }
    }
}

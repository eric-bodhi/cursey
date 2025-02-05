#include "editor.h"
#include "commands.h"
#include "keybindings.h"
#include "cursor.h"
#include "textbuffer.h"
#include "tui.h"
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

Editor::Editor(const std::string& filepath)
    : cursey(), buffer(filepath), viewport(cursey.get_terminal_size()),
      cm(buffer, cursey.get_terminal_size().max_row), m_filepath(filepath), shouldExit(false) {
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

void Editor::setMode(Mode mode) {
    currMode = mode;
}

void Editor::insertMode(int input) {
    if (input == 27) { // ESC key
        currMode = Mode::Normal;
        return;
    }

    // Handle special characters
    switch (input) {
    case KEY_BACKSPACE:
        if (cm.get().col > 0) {
            buffer.erase(cm);
            cm.moveDir(Direction::Left);
        }
        break;
    case KEY_ENTER:
    case '\n':
        // Handle newline insertion
        break;
    default:
        buffer.insert(cm, static_cast<char>(input));
        cm.moveDir(Direction::Right);
    }
}

void Editor::commandMode() {
    echo();
    char cmd_buf[256];
    cursey.render_command_line("");
    wgetnstr(cursey.get_cmd_win(), cmd_buf, sizeof(cmd_buf));
    noecho();

    execute(Command::ftable, cmd_buf);
    currMode = Mode::Normal;
}

TextBuffer& Editor::getBuffer() {
    return buffer;
}

CursorManager& Editor::getCm() {
    return cm;
}

void Editor::setShouldExit(bool value) {
    shouldExit = value;
}

void Editor::updateView() {
    auto modelCursor = cm.get();
    viewport.adjustViewPort(modelCursor);
    auto screenCursor = viewport.modelToScreen(modelCursor);
    cursey.render_file(screenCursor, buffer, viewport.getViewOffset());
}

std::string intToString(int value) {
    char c = value;
    return std::string(1, c);
}

void Editor::execute(auto ftable, int key) {
    auto skey = intToString(key);
    logger.log(skey);
    if (ftable.contains(skey)) {
        ftable.at(skey)(*this);
    }
}

void Editor::execute(auto ftable, std::string_view cmd) {
    if (ftable.contains(cmd)) {
        ftable.at(cmd)(*this);
    }
}

void Editor::run() {
    int input;
    int lastInput = 0;
    // Initial render
    updateView();
    /*
    Diff ftable for keybindings
    int lastInput
    check if lastinput & input are in keybinding ftable
    or if input is in keybinding ftable
    i.e. "dd", "x"
    diff ftable for normal and visual
    */
    while (!shouldExit) {
        switch (currMode) {
        case Mode::Normal:
        case Mode::Insert:
        case Mode::Visual:
            input = getch(); // Get single character input
            break;
        case Mode::Command:
            input = 0; // Command mode uses line input
            lastInput = 0;
            break;
        }

        switch (currMode) {
        case Mode::Normal:
            execute(Keybindings::normalkeys, input);
            execute(Keybindings::normalkeys, intToString(lastInput) + intToString(input));
            break;

        case Mode::Insert:
            insertMode(input);
            break;

        case Mode::Command:
            commandMode(); // Handles its own input loop
            break;

        case Mode::Visual:
            break;
        }
        lastInput = input;
        // Always update view after processing input
        if (!shouldExit)
            updateView();
    }
}

void Editor::exit() {

}

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
    : cursey(), buffer(filepath), viewport(cursey.get_terminal_size()),
      cm(buffer, cursey.get_terminal_size().max_row), m_filepath(filepath) {
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
bool Editor::normalMode(int input) {
    switch (input) {
    case 'q':
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

    return false;
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

    execute(cmd_buf);
    currMode = Mode::Normal;
}

TextBuffer& Editor::getBuffer() {
    return buffer;
}

CursorManager& Editor::getCm() {
    return cm;
}

void Editor::updateView() {
    auto modelCursor = cm.get();
    viewport.adjustViewPort(modelCursor);
    auto screenCursor = viewport.modelToScreen(modelCursor);
    cursey.render_file(screenCursor, buffer, viewport.getViewOffset());
}

void Editor::execute(std::string_view command) {
    if (Command::ftable.contains(command)) {
        Command::ftable.at(command)(*this);
    }
}

void Editor::run() {
    int input;
    bool shouldExit = false;

    // Initial render
    updateView();

    while (!shouldExit) {
        switch (currMode) {
        case Mode::Normal:
        case Mode::Insert:
        case Mode::Visual:
            input = getch(); // Get single character input
            break;
        case Mode::Command:
            input = 0; // Command mode uses line input
            break;
        }

        switch (currMode) {
        case Mode::Normal:
            shouldExit = normalMode(input);
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

        // Always update view after processing input
        if (!shouldExit)
            updateView();
    }
}

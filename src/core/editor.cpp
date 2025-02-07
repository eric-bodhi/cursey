#include "editor.h"
#include "../commands/commands.h"
#include "../keybindings/keybindings.h"
#include "cursor.h"
#include "textbuffer.h"
#include "tui.h"
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

Editor::Editor(const std::string& filepath)
    : cursey(), buffer(filepath), viewport(cursey.get_terminal_size()),
      cm(buffer, cursey.get_terminal_size().max_row), m_filepath(filepath),
      shouldExit(false) {
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
        } else {
            buffer.deleteLine(cm);
            cm.moveDir(Direction::Up);
        }
        break;
    case KEY_ENTER:
    case '\n':
        buffer.newLine(cm);
        cm.moveDir(Direction::Down);
        buffer.moveCursor(cm);
        break;
    default:
        buffer.insert(cm, static_cast<char>(input));
        cm.moveDir(Direction::Right);
    }
}

void Editor::commandMode() {
    noecho();

    char cmd_buf[256] = {0}; // Buffer to store the command
    int pos = 0;             // Current position in the buffer

    cursey.render_command_line("");

    WINDOW* cmd_win = cursey.get_cmd_win();
    wmove(cmd_win, 0, 0);

    waddch(cmd_win, ':');
    wrefresh(cmd_win);

    int ch;
    while ((ch = wgetch(cmd_win)) != '\n') {
        if (ch == 27) { // ASCII 27 is the ESC key
            currMode = Mode::Normal;
            return;
        }

        if (ch == KEY_BACKSPACE || ch == 127) {
            if (pos > 0) {
                pos--;
                cmd_buf[pos] = '\0';
                int y, x;
                getyx(cmd_win, y, x);
                if (x > 1) {
                    mvwdelch(cmd_win, y, x - 1);
                }
            }
        } else if (pos < static_cast<int>(sizeof(cmd_buf)) - 1) {
            cmd_buf[pos++] = ch;
            waddch(cmd_win, ch);
        }
        wrefresh(cmd_win);
    }

    cmd_buf[pos] = '\0';
    execute(Command::ftable, cmd_buf);
    currMode = Mode::Normal;
}

TextBuffer& Editor::getBuffer() {
    return buffer;
}

CursorManager& Editor::getCm() {
    return cm;
}

Logger& Editor::getLogger() {
    return logger;
}

const std::string& Editor::getFilePath() {
    return m_filepath;
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
    Mode lastMode = Mode::Normal;
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
            lastInput = 0;
            break;
        }

        switch (currMode) {
        case Mode::Normal:
            cursey.setCursorMode(CursorMode::Block);
            execute(Keybindings::normalkeys, input);
            execute(Keybindings::normalkeys,
                    intToString(lastInput) + intToString(input));
            break;

        case Mode::Insert:
            insertMode(input);
            cursey.setCursorMode(CursorMode::Bar);
            break;

        case Mode::Command:
            commandMode(); // Handles its own input loop
            break;

        case Mode::Visual:
            break;
        }
        lastInput = input;
        // slightly optimized cursor changing
        if (currMode != lastMode) {
            if (currMode == Mode::Insert) {
                cursey.setCursorMode(CursorMode::Bar);
            } else {
                cursey.setCursorMode(CursorMode::Block);
            }
            lastMode = currMode;
        }
        updateView();
    }
}

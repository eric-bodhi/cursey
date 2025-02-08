#include "editor.h"
#include "../commands/commands.h"
#include "../keybindings/keybindings.h"
#include "cursor.h"
#include "textbuffer.h"
#include "tui.h"
#include <fstream>
#include <iostream>
#include <string>

// A helper to convert an integer key to a string.
// (Alternatively, std::to_string could be used directly.)
std::string intToString(int value) {
    return std::to_string(value);
}

Editor::Editor(const std::string& filepath)
    : tui(),
      buffer(filepath),
      viewport(tui.get_terminal_size()),
      cm(buffer, tui.get_terminal_size().max_row),
      m_filepath(filepath),
      shouldExit(false) {
}

void Editor::writeFile() {
    std::ofstream file(m_filepath, std::ios::trunc);
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
    // For our Notcurses version, we assume input is an ASCII code.
    if (input == 27) { // ESC key
        currMode = Mode::Normal;
        return;
    }

    switch (input) {
    case 127: // Backspace (typically 127)
        if (cm.get().col > 0) {
            buffer.erase(cm);
            cm.moveDir(Direction::Left);
        } else {
            buffer.deleteLine(cm);
            cm.moveDir(Direction::Up);
        }
        break;
    case '\n': // Enter key
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
    // Use a simple loop with NotcursesTUI::getch() to collect a command.
    std::string cmd;
    tui.render_command_line(""); // Clear prompt

    int ch;
    while ((ch = tui.getch()) != '\n') {
        if (ch == 27) { // ESC key
            currMode = Mode::Normal;
            return;
        } else if (ch == 127) { // Backspace
            if (!cmd.empty()) {
                cmd.pop_back();
            }
        } else {
            cmd.push_back(static_cast<char>(ch));
        }
        // Update the command prompt (prefix with ':' as in vim)
        tui.render_command_line(":" + cmd);
    }
    // Execute the command if found in our command table.
    execute(Command::ftable, cmd);
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
    tui.render_file(screenCursor, buffer, viewport.getViewOffset());
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
    // Initial render.
    updateView();

    while (!shouldExit) {
        switch (currMode) {
        case Mode::Normal:
        case Mode::Insert:
        case Mode::Visual:
            input = tui.getch(); // Use NotcursesTUI’s blocking input
            break;
        case Mode::Command:
            input = 0; // Command mode uses its own input loop.
            lastInput = 0;
            break;
        }

        switch (currMode) {
        case Mode::Normal:
            tui.setCursorMode(CursorMode::Block);
            execute(Keybindings::normalkeys, input);
            execute(Keybindings::normalkeys, intToString(lastInput) + intToString(input));
            break;
        case Mode::Insert:
            insertMode(input);
            tui.setCursorMode(CursorMode::Bar);
            break;
        case Mode::Command:
            commandMode();
            break;
        case Mode::Visual:
            // Visual mode handling could be added here.
            break;
        }
        lastInput = input;
        // Update the cursor shape if the mode has changed.
        if (currMode != lastMode) {
            if (currMode == Mode::Insert) {
                tui.setCursorMode(CursorMode::Bar);
            } else {
                tui.setCursorMode(CursorMode::Block);
            }
            lastMode = currMode;
        }
        updateView();
    }
}

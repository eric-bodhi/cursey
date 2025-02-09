#include "editor.h"
#include "../commands/commands.h"
#include "../keybindings/keybindings.h"
#include "cursor.h"
#include "textbuffer.h"
#include "tui.h"
#include <fstream>
#include <iostream>
#include <notcurses/notcurses.h>
#include <string>

// A helper to convert an integer key to a string.
// (Alternatively, std::to_string could be used directly.)
std::string intToString(int value) {
    return std::string(1, static_cast<char>(value));
}

Editor::Editor(const std::string& filepath)
    : tui(buffer, filepath), buffer(filepath), viewport({0, 0}),
      cm(buffer, tui.get_terminal_size().max_row), m_filepath(filepath),
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
    buffer.setModified(false);
}

void Editor::setMode(Mode mode) {
    if (currMode == Mode::Visual && mode != Mode::Visual) {
        m_visual_start = std::nullopt;
        m_visual_end = std::nullopt;
    } else if (mode == Mode::Visual) {
        m_visual_start = cm.get();
        m_visual_end = cm.get();
    }
    currMode = mode;
}

void Editor::setVisualEnd(const Cursor& cursor) {
    m_visual_end = cursor;
}

void Editor::insertMode(int input) {
    // For our Notcurses version, we assume input is an ASCII code.
    if (input == NCKEY_ESC) { // ESC key
        currMode = Mode::Normal;
        return;
    }

    switch (input) {
    case NCKEY_BACKSPACE: // Backspace (typically 127)
        if (cm.get().col > 0) {
            buffer.erase(cm);
            cm.moveDir(Direction::Left);
        } else {
            buffer.deleteLine(cm);
            cm.moveDir(Direction::Up);
        }
        break;
    case NCKEY_ENTER: // Enter key
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
    while ((ch = tui.getch()) != NCKEY_ENTER) {
        logger.log(std::to_string(ch));
        if (ch == NCKEY_ESC) { // ESC key
            currMode = Mode::Normal;
            return;
        } else if (ch == NCKEY_BACKSPACE) { // Backspace
            if (cmd.empty()) {
                currMode = Mode::Normal;
                return;
            }
            cmd.pop_back();
        } else {
            cmd.push_back(static_cast<char>(ch));
        }
        tui.render_command_line(cmd);
    }
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

const VisualRange Editor::getVisualRange() {
    return {m_visual_start, m_visual_end};
}

void Editor::setShouldExit(bool value) {
    shouldExit = value;
}

void Editor::updateView() {
    auto modelCursor = cm.get();
    viewport.adjustViewPort(modelCursor);
    auto screenCursor = viewport.modelToScreen(modelCursor);
    tui.render_file(screenCursor, buffer, viewport.getViewOffset(),
                    m_visual_start, m_visual_end);
}

bool Editor::execute(const std::unordered_map<
                         std::string_view, std::function<void(Editor&)>>& table,
                     std::string_view cmd) {
    if (table.contains(cmd)) {
        table.at(cmd)(*this);
        return true;
    }
    return false;
}

void Editor::run() {
    int input;
    int lastInput = 0;
    Mode lastMode = Mode::Normal;
    // Initial render.
    updateView();

    while (true) {
        if (shouldExit) {
            if (buffer.isModified()) {
                tui.render_message("Changes not written, use ':w' or ':q!'");
                shouldExit = false;
            } else {
                break;
            }
        }

        TermBoundaries currentTermSize = tui.get_terminal_size();
        if (currentTermSize.max_row != viewport.getMaxRow() + 2 ||
            currentTermSize.max_col != viewport.getMaxCol()) {
            viewport.updateTerminalSize(currentTermSize);
        }

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
            tui.render_message("");
            if (!execute(Keybindings::normalkeys, intToString(input))) {
                if (execute(Keybindings::normalkeys,
                        intToString(lastInput) + intToString(input))) {
                        lastInput = 0;
                        input = 0;
                    }
            }
            break;
        case Mode::Insert:
            insertMode(input);
            tui.setCursorMode(CursorMode::Bar);
            tui.render_message("-- INSERT --");
            break;
        case Mode::Command:
            commandMode();
            break;
        case Mode::Visual:
            if (!execute(Keybindings::visualkeys, intToString(input))) {
                execute(Keybindings::visualkeys,
                        intToString(lastInput) + intToString(input));
            }
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

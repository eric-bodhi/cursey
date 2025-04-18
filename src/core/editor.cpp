#include "editor.h"
#include "../commands/commands.h"
#include "../keybindings/keybindings.h"
#include "buffer.h"
#include "cursor.h"
#include "tui.h"
#include <fstream>
#include <iostream>
#include <notcurses/notcurses.h>
#include <string>

// A helper to convert an integer key to a string.
std::string int_to_str(const int value) {
    return {static_cast<char>(value)};
}

Editor::Editor(const std::string& filepath)
    : tui(buffer, filepath), cm(buffer), viewport({0, 0}), buffer(filepath),
      m_filepath(filepath), should_exit(false) {
}

void Editor::write_file() {
    std::ofstream file(m_filepath, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to open existing file: " << m_filepath << '\n';
        return;
    }

    for (std::size_t i = 0; i < buffer.line_count(); i++) {
        file << buffer.get_line(i) << '\n';
    }
    file.close();
    buffer.set_modified(false);
}

void Editor::set_mode(const Mode mode) {
    if (curr_mode == Mode::Visual && mode != Mode::Visual) {
        m_visual_start = std::nullopt;
        m_visual_end = std::nullopt;
    } else if (mode == Mode::Visual) {
        m_visual_start = cm.get();
        m_visual_end = cm.get();
    }
    curr_mode = mode;
}

void Editor::set_visual_end(const Cursor& cursor) {
    if (cursor > m_visual_end.value()) {
        m_visual_end = cursor;
    } else {
        m_visual_start = cursor;
    }
}

void Editor::insert_mode(const int input) {
    // For our Notcurses version, we assume input is an ASCII code.
    if (input == NCKEY_ESC) { // ESC key
        curr_mode = Mode::Normal;
        return;
    }

    switch (input) {
    case NCKEY_BACKSPACE: // Backspace (typically 127)
        if (cm.col() > 0) {
            buffer.erase(cm);
            cm.move_dir(Direction::Left);
        } else {
            if (cm.row() > 0) {
                std::size_t prev_line_length{};
                prev_line_length = buffer.get_line_length(cm.row() - 1);
                prev_line_length == 1 ? prev_line_length = 0 : prev_line_length;
                buffer.insert({cm.row() - 1, prev_line_length},
                              buffer.get_line(cm.row()));

                buffer.delete_line(cm);
                cm.move_abs({cm.row() - 1,
                             prev_line_length == 1 ? 0 : prev_line_length});
            }
        }
        break;
    case NCKEY_ENTER: // Enter key
        buffer.new_line(cm);
        cm.move_dir(Direction::Down);
        cm.move_abs({cm.row(), 0});
        buffer.move_cursor(cm);
        break;
    default:
        buffer.insert(cm, static_cast<char>(input));
        cm.move_dir(Direction::Right);
    }
}

void Editor::command_mode() {
    // Use a simple loop with NotcursesTUI::get_char() to collect a command.
    std::string cmd;
    tui.render_command_line(""); // Clear prompt

    int ch;
    while ((ch = tui.get_char()) != NCKEY_ENTER) {
        if (ch == NCKEY_ESC) { // ESC key
            curr_mode = Mode::Normal;
            return;
        }
        if (ch == NCKEY_BACKSPACE) { // Backspace
            if (cmd.empty()) {
                curr_mode = Mode::Normal;
                return;
            }
            cmd.pop_back();
        } else {
            cmd.push_back(static_cast<char>(ch));
        }
        tui.render_command_line(cmd);
    }
    execute(Command::command_table, cmd);
    curr_mode = Mode::Normal;
}

Buffer& Editor::get_buffer() {
    return buffer;
}

CursorManager& Editor::get_cm() {
    return cm;
}

Logger& Editor::get_logger() {
    return logger;
}

[[maybe_unused]] const std::string& Editor::get_filepath() {
    return m_filepath;
}

VisualRange Editor::get_visual_range() const {
    return {m_visual_start, m_visual_end};
}

void Editor::set_should_exit(const bool value) {
    should_exit = value;
}

void Editor::update_view() {
    const auto model_cursor = cm.get();
    viewport.adjust_viewport(model_cursor);
    const auto screen_cursor = viewport.model_to_screen(model_cursor);
    tui.render_file(screen_cursor, buffer, viewport.get_view_offset(),
                    m_visual_start, m_visual_end);
}

bool Editor::execute(
    const std::unordered_map<std::string, std::function<void(Editor&)>>& table,
    const std::string& cmd) {
    if (table.contains(cmd)) {
        table.at(cmd)(*this);
        return true;
    }
    return false;
}

void Editor::run() {
    int input = 0;
    int last_input = 0;
    auto last_mode = Mode::Normal;
    // Initial render.
    update_view();

    while (true) {
        if (should_exit) {
            if (buffer.is_modified()) {
                tui.render_message("Changes not written, use ':w' or ':q!'");
                should_exit = false;
            } else {
                break;
            }
        }

        if (const TermBoundaries curr_term_size = tui.get_terminal_size();
            curr_term_size.max_row != viewport.get_max_row() + 2 ||
            curr_term_size.max_col != viewport.get_max_col()) {
            viewport.update_term_size(curr_term_size);
        }

        switch (curr_mode) {
        case Mode::Normal:
        case Mode::Insert:
        case Mode::Visual:
            input = tui.get_char(); // Use NotcursesTUI’s blocking input
            break;
        case Mode::Command:
            input = 0; // Command mode uses its own input loop.
            last_input = 0;
            break;
        }

        switch (curr_mode) {
        case Mode::Normal:
            NotcursesTUI::set_cursor_mode(CursorMode::Block);
            tui.render_message("");
            if (!execute(Keybindings::normal_keys, int_to_str(input))) {
                if (execute(Keybindings::normal_keys,
                            int_to_str(last_input) + int_to_str(input))) {
                    input = 0;
                    last_input = 0;
                }
            }
            break;
        case Mode::Insert:
            insert_mode(input);
            NotcursesTUI::set_cursor_mode(CursorMode::Bar);
            tui.render_message("-- INSERT --");
            break;
        case Mode::Command:
            command_mode();
            break;
        case Mode::Visual:
            if (!execute(Keybindings::visual_keys, int_to_str(input))) {
                execute(Keybindings::visual_keys,
                        int_to_str(last_input) + int_to_str(input));
            }
            break;
        }
        last_input = input;
        // Update the cursor shape if the mode has changed.
        if (curr_mode != last_mode) {
            if (curr_mode == Mode::Insert) {
                NotcursesTUI::set_cursor_mode(CursorMode::Bar);
            } else {
                NotcursesTUI::set_cursor_mode(CursorMode::Block);
            }
            last_mode = curr_mode;
        }
        update_view();
    }
}

#include "textbuffer.h"
#include "../utils/deque_gb.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

TextBuffer::TextBuffer(const std::string& filepath) {
    load_file(filepath);
    buffer.at(0) = Gb(get_line(0));

    // populate unwritten buffer
    for (const auto& line : buffer) {
        if (std::holds_alternative<std::string>(line)) {
            original_buffer.push_back(std::get<std::string>(line));
        } else {
            original_buffer.push_back(std::get<Gb>(line).to_string());
        }
    }
}

bool TextBuffer::load_file(const std::string& filepath) {
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: unable to open file " << filepath << "\n";
        return false;
    }

    buffer.clear();

    std::string line;
    while (std::getline(file, line)) {
        buffer.emplace_back(line);
    }

    file.close();
    return true;
}

void TextBuffer::revert_buffer(std::vector<std::string> new_buffer) {
    buffer.clear();
    for (const auto& line : new_buffer) {
        buffer.emplace_back(line);
    }
    buffer.at(0) = Gb(get_line(0));
}

[[maybe_unused]] void TextBuffer::revert_buffer() {
    revert_buffer(original_buffer);
}

std::size_t TextBuffer::line_count() const {
    return buffer.size();
}

std::string TextBuffer::get_line(std::size_t index) const {
    auto& line = buffer.at(index);
    if (std::holds_alternative<Gb>(line)) {
        return std::get<Gb>(line).to_string();
    }

    return std::get<std::string>(line);
}

std::size_t TextBuffer::get_line_length(std::size_t index) const {
    return get_line(index).length();
}

bool TextBuffer::is_modified() const {
    return was_modified;
}

void TextBuffer::set_modified(const bool& value) {
    was_modified = value;
}

// turns gapbuffer back to string and new line to gapbuffer (to be edited)
// where cm is the current cursor position
void TextBuffer::switch_line(std::size_t new_line_idx) {
    if (gb_idx != new_line_idx) {
        // Convert current line to string
        if (std::holds_alternative<Gb>(buffer.at(gb_idx))) {
            Gb& gb_line = std::get<Gb>(buffer.at(gb_idx));
            buffer.at(gb_idx) = gb_line.to_string();
        }

        // Update current line index
        gb_idx = new_line_idx;

        // Convert new line to GapBuffer if it's a string
        if (std::holds_alternative<std::string>(buffer.at(new_line_idx))) {
            std::string line_str =
                std::get<std::string>(buffer.at(new_line_idx));
            buffer.at(new_line_idx) = Gb(line_str);
            gb_idx = new_line_idx;
        }
    }
}

void TextBuffer::move_cursor(const CursorManager& new_cm) {
    auto new_cursor = new_cm.get();
    if (gb_idx != new_cursor.row) {
        switch_line(new_cursor.row);
    }

    if (std::holds_alternative<Gb>(buffer.at(gb_idx))) {
        std::get<Gb>(buffer.at(gb_idx)).move_cursor(new_cursor.col);
    }
}

void TextBuffer::move_cursor(const Cursor& cursor) {
    if (gb_idx != cursor.row) {
        switch_line(cursor.row);
    }

    if (std::holds_alternative<Gb>(buffer.at(gb_idx))) {
        std::get<Gb>(buffer.at(gb_idx)).move_cursor(cursor.col);
    }
}

void TextBuffer::insert(const Cursor& cursor, const char c) {
    move_cursor(cursor);
    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        Gb& gb_line = std::get<Gb>(buffer.at(cursor.row));
        gb_line.insert(c);
    }
    was_modified = true;
}

void TextBuffer::insert(const CursorManager& cm, const char c) {
    insert(cm.get(), c);
}

void TextBuffer::erase(const CursorManager& cm) {
    erase(cm.get());
}

void TextBuffer::erase(const Cursor& cursor) {
    move_cursor(cursor);
    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        Gb& gb_line = std::get<Gb>(buffer.at(cursor.row));
        if (gb_line.size() == 0) {
            delete_line(cursor.row);
            return;
        }
        tb_logger.log("1 " + gb_line.string_with_gap());
        gb_line.del();
        tb_logger.log("2 " + gb_line.string_with_gap());
    }
    was_modified = true;
}

void TextBuffer::new_line(const CursorManager& cm) {
    std::size_t line_idx = cm.get().row;
    auto line = get_line(line_idx);
    const std::string new_line =
        std::string(line.begin() + cm.get().col, line.end());
    if (new_line.empty()) {
        buffer.insert(buffer.begin() + line_idx + 1, " ");
    } else {
        buffer.insert(buffer.begin() + line_idx + 1, new_line);
    }

    // set line before accordingly
    if (new_line.size() == line.size()) {
        buffer.at(line_idx) = " ";
    } else {
        buffer.at(line_idx) =
            std::string(line.begin(), line.begin() + cm.get().col);
    }
    was_modified = true;
}

void TextBuffer::delete_line(const CursorManager& cm) {
    const std::size_t line_idx = cm.get().row;
    delete_line(line_idx);
}

void TextBuffer::delete_line(const std::size_t line_idx) {
    tb_logger.log("deleted " + std::to_string(line_idx));
    buffer.erase(buffer.begin() + line_idx);
    if (line_idx == 0 && line_count() == 1) {
        buffer.insert(buffer.begin() + line_idx, "");
    } else if (line_count() - 1 == line_idx && line_idx != 0) {

        switch_line(line_idx - 1);
    }
    was_modified = true;
}

void TextBuffer::delete_range(const Cursor& start, const Cursor& end, Logger& logger) {
    Cursor actual_start = start;
    Cursor actual_end = end;

    // Ensure start is before end
    if (actual_start.row > actual_end.row ||
        (actual_start.row == actual_end.row && actual_start.col > actual_end.col)) {
        std::swap(actual_start, actual_end);
    }

    if (actual_start.row == actual_end.row) {
        // Same line: delete the substring from start.col to end.col inclusive
        std::size_t line_idx = actual_start.row;
        std::string line = get_line(line_idx);
        std::size_t start_col = actual_start.col;
        std::size_t end_col = std::min(actual_end.col, line.size() - 1);

        if (start_col <= end_col) {
            line.erase(start_col, end_col - start_col + 1);
            buffer[line_idx] = line;
            was_modified = true;
        }
    } else {
        // Multi-line deletion
        std::string start_line = get_line(actual_start.row);
        std::size_t start_col = actual_start.col;

        // Truncate the start line to the start column
        if (start_col < start_line.size()) {
            start_line.erase(start_col);
            buffer[actual_start.row] = start_line;
        }

        // Capture the remaining part of the end line
        std::string end_line = get_line(actual_end.row);
        std::size_t end_col_plus1 = actual_end.col + 1;
        std::string remaining = (end_col_plus1 <= end_line.size()) ? end_line.substr(end_col_plus1) : "";

        // Delete lines from start.row +1 to end.row inclusive
        std::size_t lines_to_delete = actual_end.row - actual_start.row;
        for (std::size_t i = 0; i < lines_to_delete; ++i) {
            delete_line(actual_start.row + 1);
        }

        // Append the remaining content to the start line
        buffer[actual_start.row] = get_line(actual_start.row) + remaining;
        was_modified = true;
    }
}

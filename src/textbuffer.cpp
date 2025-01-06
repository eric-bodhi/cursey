#include "textbuffer.h"
#include "deque_gb.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

TextBuffer::TextBuffer(const std::string& filepath) {
    loadFile(filepath);
    // cursor starts at (1,1) [1 indexed] so first line is gapbuffer
    cursor.row = 0;
    buffer.at(cursor.row) = Gb(getLine(0));
}

bool TextBuffer::loadFile(const std::string& filepath) {
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: unable to open file " << filepath << "\n";
        return false;
    }

    buffer.clear();

    std::string line;
    while (std::getline(file, line)) {
        buffer.push_back(line);
    }

    file.close();
    return true;
}

std::size_t TextBuffer::lineCount() const {
    return buffer.size();
}

const std::string TextBuffer::getLine(std::size_t index) const {
    auto& line = buffer.at(index);
    if (std::holds_alternative<Gb>(line)) {
        return std::get<Gb>(line).to_string();
    }

    return std::get<std::string>(line);
}

const std::size_t TextBuffer::getLineLength(std::size_t index) const {
    return getLine(index).length();
}

void TextBuffer::switchLine(std::size_t newLineIdx) {
    if (cursor.row != newLineIdx) {
        // Convert current line to string
        if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
            Gb& gbLine = std::get<Gb>(buffer.at(cursor.row));
            buffer.at(cursor.row) = gbLine.to_string();
        }

        // Update current line index
        cursor.row = newLineIdx;

        // Convert new line to GapBuffer if it's a string
        if (std::holds_alternative<std::string>(buffer.at(cursor.row))) {
            std::string lineString =
                std::get<std::string>(buffer.at(cursor.row));
            buffer.at(cursor.row) = Gb(lineString);
        }
    }
}

void TextBuffer::moveCursor(Position pos) {
    if (pos == cursor) {
        return;
    }

    else if (pos.row != cursor.row) {
        switchLine(pos.row);
    }

    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        std::get<Gb>(buffer.at(cursor.row)).move_cursor(pos.col);
    }
}

void TextBuffer::insert(const char c) {
    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        Gb& gbLine = std::get<Gb>(buffer.at(cursor.row));
        gbLine.insert(c);
    }
}

void TextBuffer::erase() {
    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        Gb& gbLine = std::get<Gb>(buffer.at(cursor.row));
        gbLine.del();
    }
}

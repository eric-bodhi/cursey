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
    lineIdx = 0;
    buffer.at(lineIdx) = Gb(getLine(lineIdx));
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
    if (lineIdx != newLineIdx) {
        // Convert current line to string
        if (std::holds_alternative<Gb>(buffer.at(lineIdx))) {
            Gb& gbLine = std::get<Gb>(buffer.at(lineIdx));
            buffer.at(lineIdx) = gbLine.to_string();
        }

        // Update current line index
        lineIdx = newLineIdx;

        // Convert new line to GapBuffer if it's a string
        if (std::holds_alternative<std::string>(buffer.at(lineIdx))) {
            std::string lineString = std::get<std::string>(buffer.at(lineIdx));
            buffer.at(lineIdx) = Gb(lineString);
        }
    }
}

void TextBuffer::insertAt(Position pos, const char c) {
    // if not editing same line from before, de-gapbuffer original line,
    // and gap buffer new line
    switchLine(pos.row);
    if (std::holds_alternative<Gb>(buffer.at(lineIdx))) {
        Gb& gbLine = std::get<Gb>(buffer.at(lineIdx));
        gbLine.move_cursor(pos.col);
        gbLine.insert(c);
    }
}

void TextBuffer::eraseAt(Position pos) {
    // Check if the position is within bounds of the current line
    if (pos.row >= buffer.size()) {
        return;
    }

    switchLine(pos.row);
    Gb& gbLine = std::get<Gb>(buffer.at(pos.row));

    // Ensure the column is within the bounds of the line
    if (pos.col >= 1 && pos.col < gbLine.size()) {
        gbLine.move_cursor(pos.col);
        gbLine.del();
    } else if (pos.col == 0) {
        gbLine.move_cursor(0);
        gbLine.del();
    }
}

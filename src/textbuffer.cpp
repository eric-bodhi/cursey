#include "textbuffer.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

TextBuffer::TextBuffer(const std::string& filepath) {
    loadFile(filepath);
    // cursor starts at (1,1) [1 indexed] so first line is gapbuffer
    lineIdx = 0;
    buffer.at(lineIdx) = GapBuffer<char>(getLine(lineIdx));
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
    if (std::holds_alternative<GapBuffer<char>>(line)) {
        return std::get<GapBuffer<char>>(line).to_string();
    }

    return std::get<std::string>(line);
}

const std::size_t TextBuffer::getLineLength(std::size_t index) const {
    return getLine(index).length();
}

void TextBuffer::switchLine(Position pos) {
    if (lineIdx != pos.row) {
        auto& line_variant = buffer.at(lineIdx);

        if (std::holds_alternative<GapBuffer<char>>(line_variant)) {
            GapBuffer<char>& gbLine = std::get<GapBuffer<char>>(line_variant);
            std::string line_string = gbLine.to_string();
            buffer.at(lineIdx) = line_string;
        }

        lineIdx = pos.row;
        if (std::holds_alternative<std::string>(buffer.at(lineIdx))) {
            auto newgb =
                GapBuffer<char>(std::get<std::string>(buffer.at(lineIdx)));
            buffer.at(lineIdx) =
                newgb; // convert new line being editted into gb
        }
    }
}

std::string TextBuffer::insertAt(Position pos, const char s) {
    // if not editing same line from before, de-gapbuffer original line,
    // and gap buffer new line
    switchLine(pos);
    GapBuffer<char>& gbLine = std::get<GapBuffer<char>>(buffer.at(lineIdx));
    gbLine.insert(gbLine.begin() + (pos.col), s);

    return gbLine.to_string() + " " + std::to_string(gbLine.gapSize());
}

void TextBuffer::eraseAt(Position pos) {
    // Check if the position is within bounds of the current line
    if (pos.row >= buffer.size()) {
        return;
    }

    switchLine(pos);
    GapBuffer<char>& gbLine = std::get<GapBuffer<char>>(buffer.at(pos.row));

    // Ensure the column is within the bounds of the line
    if (pos.col >= 1 && pos.col < gbLine.size()) {
        gbLine.erase(gbLine.begin() + pos.col -
                     1); // Erase the character at the position
    } else if (pos.col == 0) {
        gbLine.erase(gbLine.begin());
    }
}

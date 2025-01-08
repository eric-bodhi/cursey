#include "textbuffer.h"
#include "deque_gb.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

TextBuffer::TextBuffer(const std::string& filepath) {
    loadFile(filepath);
    buffer.at(0) = Gb(getLine(0));
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

// turns gapbuffer back to string and new line to gapbuffer (to be edited)
// where cm is the current cursor position
void TextBuffer::switchLine(std::size_t newLineIdx) {
    if (gbIndex != newLineIdx) {
        // Convert current line to string
        if (std::holds_alternative<Gb>(buffer.at(gbIndex))) {
            Gb& gbLine = std::get<Gb>(buffer.at(gbIndex));
            buffer.at(gbIndex) = gbLine.to_string();
        }

        // Update current line index
        gbIndex = newLineIdx;

        // Convert new line to GapBuffer if it's a string
        if (std::holds_alternative<std::string>(buffer.at(newLineIdx))) {
            std::string lineString =
                std::get<std::string>(buffer.at(newLineIdx));
            buffer.at(newLineIdx) = Gb(lineString);
            gbIndex = newLineIdx;
        }
    }
}

void TextBuffer::moveCursor(const CursorManager& newCursor) {
    auto newCursorPos = newCursor.get();
    if (gbIndex != newCursorPos.row) {
        switchLine(newCursorPos.row);
    }

    if (std::holds_alternative<Gb>(buffer.at(gbIndex))) {
        std::get<Gb>(buffer.at(gbIndex)).move_cursor(newCursorPos.col);
    }
}

void TextBuffer::insert(const CursorManager& cm, const char c) {
    auto cursor = cm.get();
    moveCursor(cm);
    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        Gb& gbLine = std::get<Gb>(buffer.at(cursor.row));
        gbLine.insert(c);
    }
}

void TextBuffer::erase(const CursorManager& cm) {
    auto cursor = cm.get();
    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        Gb& gbLine = std::get<Gb>(buffer.at(cursor.row));
        gbLine.del();
    }
}

#include "textbuffer.h"
#include "../utils/deque_gb.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

TextBuffer::TextBuffer(const std::string& filepath) {
    loadFile(filepath);
    buffer.at(0) = Gb(getLine(0));

    // populate unwritten buffer
    for (const auto& line : buffer) {
        if (std::holds_alternative<std::string>(line)) {
            originalBuffer.push_back(std::get<std::string>(line));
        } else {
            originalBuffer.push_back(std::get<Gb>(line).to_string());
        }
    }
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

void TextBuffer::revertBuffer(std::vector<std::string> newBuffer) {
    buffer.clear();
    for (const auto& line : newBuffer) {
        buffer.push_back(line);
    }
    buffer.at(0) = Gb(getLine(0));
}

void TextBuffer::revertBuffer() {
    revertBuffer(originalBuffer);
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

bool TextBuffer::isModified() const {
    return wasModified;
}

void TextBuffer::setModified(const bool& value) {
    wasModified = value;
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

void TextBuffer::moveCursor(const Cursor& cursor) {
    if (gbIndex != cursor.row) {
        switchLine(cursor.row);
    }

    if (std::holds_alternative<Gb>(buffer.at(gbIndex))) {
        std::get<Gb>(buffer.at(gbIndex)).move_cursor(cursor.col);
    }
}

void TextBuffer::insert(const CursorManager& cm, const char c) {
    auto cursor = cm.get();
    moveCursor(cm);
    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        Gb& gbLine = std::get<Gb>(buffer.at(cursor.row));
        gbLine.insert(c);
    }
    wasModified = true;
}

void TextBuffer::erase(const CursorManager& cm) {
    erase(cm.get());
}

void TextBuffer::erase(const Cursor& cursor) {
    moveCursor(cursor);
    tblogger.log(std::to_string(cursor.row) + " " + std::to_string(cursor.col));
    if (std::holds_alternative<Gb>(buffer.at(cursor.row))) {
        Gb& gbLine = std::get<Gb>(buffer.at(cursor.row));
        gbLine.del();
    }
    wasModified = true;
}

void TextBuffer::newLine(const CursorManager& cm) {
    std::size_t lineIdx = cm.get().row;
    auto line = getLine(lineIdx);
    const std::string newLine =
        std::string(line.begin() + cm.get().col, line.end());
    if (newLine.size() == 0) {
        buffer.insert(buffer.begin() + lineIdx + 1, " ");
    } else {
        buffer.insert(buffer.begin() + lineIdx + 1, newLine);
    }

    // set line before accordingly
    if (newLine.size() == line.size()) {
        buffer.at(lineIdx) = " ";
    } else {
        buffer.at(lineIdx) =
            std::string(line.begin(), line.begin() + cm.get().col);
    }
    wasModified = true;
}

void TextBuffer::deleteLine(const CursorManager& cm) {
    std::size_t lineIdx = cm.get().row;
    deleteLine(lineIdx);
}

void TextBuffer::deleteLine(const std::size_t lineIdx) {
    buffer.erase(buffer.begin() + lineIdx);
    if (lineIdx == 0 && lineCount() == 1) {
        buffer.insert(buffer.begin() + lineIdx, "");
    } else if (lineCount() - 1 == lineIdx && lineIdx != 0) {

        switchLine(lineIdx - 1);
    }
    wasModified = true;
}

// assumes start is before end
void TextBuffer::deleteRange(const Cursor& start, const Cursor& end) {
    if (start.row == end.row) {
        if (end.col == getLineLength(start.row)) {
            deleteLine(start.row);
            return;
        }
        for (std::size_t i = 0; i <= end.col; i++) {
            tblogger.log(getLine(start.row));
            erase({start.row, start.col}); // buffer shifs left after erase so
                                           // just erase in place i times
        }
    }

    else {
        // delete [start.col, end of line]
        for (std::size_t i = 0; i < getLineLength(start.row); i++) {
            erase({start.row, start.col});
        }
        // delete all lines in between
        for (std::size_t line = start.col + 1; line < end.row; line++) {
            deleteLine(start.row + 1);
        }
        // delete [start of line, end.col]
        for (std::size_t i = 0; i <= end.col; i++) {
            erase({end.row, 0});
        }
    }
}

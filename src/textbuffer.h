#pragma once

#include "gapbuffer.h"
#include <string>
#include <variant>
#include <vector>

struct Position {
    std::size_t row;
    std::size_t col;
};

class TextBuffer {
private:
    std::vector<std::variant<std::string, GapBuffer<char>>> buffer;
    std::size_t lineIdx;

public:
    TextBuffer(const std::string& filepath);

    bool loadFile(const std::string& filepath);

    std::size_t lineCount() const;

    // turns gapbuffer to string if buffer[index] is gb
    const std::string getLine(std::size_t index) const;

    const std::size_t getLineLength(std::size_t index) const;

    void insertAt(Position pos, const char s);
};

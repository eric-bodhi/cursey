#pragma once

#include "deque_gb.h"
#include "log.h"
#include <string>
#include <variant>
#include <vector>

struct Position {
    std::size_t row;
    std::size_t col;
};

class TextBuffer {
private:
    std::vector<std::variant<std::string, Gb>> buffer;
    std::size_t lineIdx;
    Logger tblogger = Logger("../logfile.txt");

public:
    TextBuffer(const std::string& filepath);

    bool loadFile(const std::string& filepath);

    std::size_t lineCount() const;

    // turns gapbuffer to string if buffer[index] is gb
    const std::string getLine(std::size_t index) const;

    const std::size_t getLineLength(std::size_t index) const;

    void switchLine(std::size_t newLineIdx);

    void insertAt(Position pos, const char c);

    void eraseAt(Position pos);
};

#pragma once

#include "deque_gb.h"
#include "log.h"
#include <string>
#include <variant>
#include <vector>
#include "cursor.h"

// forward decl
class CursorManager;

class TextBuffer {
private:
    std::vector<std::variant<std::string, Gb>> buffer;
    CursorManager& cursor;
    Logger tblogger = Logger("../logfile.txt");

public:
    TextBuffer(const std::string& filepath, CursorManager& cm);

    bool loadFile(const std::string& filepath);

    std::size_t lineCount() const;

    // turns gapbuffer to string if buffer[index] is gb
    const std::string getLine(std::size_t index) const;

    const std::size_t getLineLength(std::size_t index) const;

    void switchLine(std::size_t newLineIdx);
    // movees to CursorManager Cursor
    void moveCursor();

    void insert(const char c);

    void erase();
};

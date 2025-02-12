#pragma once

#include "../utils/deque_gb.h"
#include "../utils/log.h"
#include "cursor.h"
#include <string>
#include <variant>
#include <vector>

// forward decl
class CursorManager;

class TextBuffer {
private:
    std::vector<std::variant<std::string, Gb>> buffer;
    std::vector<std::string> originalBuffer;
    std::size_t gbIndex = 0;
    Logger tblogger = Logger("../logfile.txt");
    bool wasModified = false;

public:
    TextBuffer(const std::string& filepath);

    bool loadFile(const std::string& filepath);

    void revertBuffer();
    void revertBuffer(std::vector<std::string> newBuffer);

    std::size_t lineCount() const;

    // turns gapbuffer to string if buffer[index] is gb
    const std::string getLine(std::size_t index) const;

    const std::size_t getLineLength(std::size_t index) const;

    bool isModified() const;
    void setModified(const bool& value);
    // has to make original editted line a string and new line a gapbuffer
    void switchLine(std::size_t newLineIdx);

    void moveCursor(const Cursor& cursor);
    void moveCursor(const CursorManager& newCursor);

    void insert(const Cursor& cursor, const char c);
    void insert(const CursorManager& cm, const char c);

    void erase(const Cursor& cursor);
    void erase(const CursorManager& cm);
    void deleteRange(const Cursor& start, const Cursor& end, Logger& logger);

    // newLine at index cm.row + 1
    void newLine(const CursorManager& cm);
    void deleteLine(const CursorManager& cm);
    void deleteLine(const std::size_t lineIdx);
};

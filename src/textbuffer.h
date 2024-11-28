#pragma once

#include "gapbuffer.h"
#include <string>
#include <variant>
#include <vector>

class TextBuffer {
private:
    std::vector<std::variant<std::string, GapBuffer<char>>> buffer;

public:
    TextBuffer(const std::string& filepath);

    bool loadFile(const std::string& filepath);

    std::size_t lineCount() const;

    const std::string getLine(std::size_t index) const;

    const std::size_t getLineLength(std::size_t index) const;
};

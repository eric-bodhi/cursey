#include "textbuffer.h"
#include <fstream>
#include <iostream>
#include <string>

TextBuffer::TextBuffer(const std::string& filepath) {
    loadFile(filepath);
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
    return this->getLine(index).length();
}

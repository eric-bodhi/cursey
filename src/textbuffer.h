#include "gapbuffer.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <variant>

class TextBuffer {
private:
    std::vector<std::variant<std::string, GapBuffer<char>>> buffer;

public:
    TextBuffer(const std::string& filepath) {
        loadFile(filepath);
    }

    bool loadFile(const std::string& filepath) {
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

    std::size_t lineCount() const {
        return buffer.size();
    }

    const std::string getLine(std::size_t index) const {
        auto& line = buffer.at(index);
        if (std::holds_alternative<GapBuffer<char>>(line)) {
            return std::get<GapBuffer<char>>(line).to_string();
        }

        return std::get<std::string>(line);
    }
 };

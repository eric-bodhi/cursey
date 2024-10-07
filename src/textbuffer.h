#include "gapbuffer.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class TextBuffer {
private:
    std::vector<std::string> buffer;
    size_t viewStart, viewEnd, buffStart,
        buffEnd; // line numbers of visible buffer and whole buffer
    struct CurrLine {
        GapBuffer<char> lineBuffer; // GapBuffer for the current line
        size_t index;               // Index of the line in the vector
        TextBuffer& textBuffer;     // Reference to the TextBuffer

        CurrLine(size_t idx, TextBuffer& tb)
            : index(idx), textBuffer(tb), lineBuffer() {
            // Initialize the GapBuffer with the current line
            lineBuffer =
                GapBuffer<char>(); // Add initialization logic if necessary
        }
    };

    CurrLine currLine;

public:
    TextBuffer(std::ifstream& file) : viewStart(0), buffStart(0) {
        if (!file.is_open()) {
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            buffer.push_back(line);
        }

        file.close();

        buffEnd = buffer.size();
        viewEnd = buffEnd;
    }
};

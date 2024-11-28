#pragma once

#include "log.h"
#include "tui.h"

class Editor {
private:
    enum class Mode {
        Normal,
        Insert,
        Visual,
    };

    Mode currMode = Mode::Normal;
    Cursey cursey;
    Logger logger = Logger("../logfile.txt");

public:
    explicit Editor(const std::string& filepath);

    // TODO: Each respective mode will handle what happens in that given mode
    // normal mode will interpret input char as switching to new mode etc
    void normalMode(const char input);
    void insertMode(const char input);
    // void commandMode(const char input);

    // main loop
    void run();
};

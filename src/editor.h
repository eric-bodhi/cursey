#pragma once

#include "cursor.h"
#include "log.h"
#include "textbuffer.h"
#include "tui.h"
#include "viewportmanager.h"
#include <string>

class Editor {
private:
    enum class Mode {
        Normal,
        Insert,
        Visual,
        Command,
    };

    Mode currMode = Mode::Normal;
    Logger logger = Logger("../logfile.txt");
    Cursey cursey;
    CursorManager cm;
    ViewportManager viewport;
    TextBuffer buffer;
    std::string m_filepath;

public:
    explicit Editor(const std::string& filepath);

    // TODO: Each respective mode will handle what happens in that given mode
    // normal mode will interpret input char as switching to new mode etc
    bool normalMode(const char input);
    void insertMode(const char input);
    void commandMode();
    void handleEscapeSequence(Gb& command);

    void writeFile();

    void run();

    void execute(std::string_view command);

    void updateView();

    // getters
    TextBuffer& getBuffer();
    CursorManager& getCm();
};

#pragma once

#include "../utils/log.h"
#include "cursor.h"
#include "textbuffer.h"
#include "viewportmanager.h"
#include "tui.h"
#include <string>

enum class Mode {
    Normal,
    Insert,
    Visual,
    Command,
};

class Editor {
private:
    Mode currMode = Mode::Normal;
    Logger logger = Logger("../logfile.txt");
    NotcursesTUI tui;        // Use NotcursesTUI instead of Cursey
    CursorManager cm;
    ViewportManager viewport;
    TextBuffer buffer;
    std::string m_filepath;
    bool shouldExit;

public:
    explicit Editor(const std::string& filepath);

    // Mode-handling methods:
    void setMode(Mode mode);
    void setShouldExit(bool value);
    void insertMode(int input);
    void commandMode();

    void writeFile();

    void run();

    // Command execution helpers:
    void execute(auto ftable, int key);
    void execute(auto ftable, std::string_view cmd);

    void updateView();

    TextBuffer& getBuffer();
    CursorManager& getCm();
    Logger& getLogger();
    const std::string& getFilePath();
};

#pragma once

#include "cursor.h"
#include "log.h"
#include "textbuffer.h"
#include "tui.h"
#include "viewportmanager.h"
#include <string>
#include <memory>

// forward decl for execute()
class Command;

class Editor {
private:
    enum class Mode {
        Normal,
        Insert,
        Visual,
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

    void writeFile();

    void run();

    void execute(std::unique_ptr<Command> command);

    void updateView();

    // getters
    TextBuffer& getBuffer();
};

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(Editor& editor) = 0;
};
// derived classes in commands.h

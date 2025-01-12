#pragma once

#include "cursor.h"
#include "log.h"
#include "textbuffer.h"
#include "tui.h"
#include "viewportmanager.h"

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

    void updateView() {
        auto modelCursor = cm.get();
        viewport.adjustViewPort(modelCursor);
        auto screenCursor = viewport.modelToScreen(modelCursor);
        cursey.render_file(screenCursor, buffer, viewport.getViewOffset());
    }

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

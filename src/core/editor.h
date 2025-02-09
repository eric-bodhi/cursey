#pragma once

#include "../utils/log.h"
#include "cursor.h"
#include "editor.h"
#include "textbuffer.h"
#include "tui.h"
#include "viewportmanager.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <optional>

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
    NotcursesTUI tui; // Use NotcursesTUI instead of Cursey
    CursorManager cm;
    ViewportManager viewport;
    TextBuffer buffer;
    std::string m_filepath;
    bool shouldExit;

    std::optional<Cursor> m_visual_start;
    std::optional<Cursor> m_visual_end;

public:
    explicit Editor(const std::string& filepath);

    // Mode-handling methods:
    void setMode(Mode mode);
    void setShouldExit(bool value);
    void setVisualEnd(const Cursor& cursor);
    void insertMode(int input);
    void commandMode();

    void writeFile();

    void run();

    bool execute(const std::unordered_map<std::string_view,
                                          std::function<void(Editor&)>>& table,
                 std::string_view cmd);
    void updateView();

    TextBuffer& getBuffer();
    CursorManager& getCm();
    Logger& getLogger();
    const std::string& getFilePath();
    const VisualRange getVisualRange();
};

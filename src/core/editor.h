#pragma once

#include "../utils/log.h"
#include "cursor.h"
#include "editor.h"
#include "textbuffer.h"
#include "tui.h"
#include "viewportmanager.h"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

enum class Mode {
    Normal,
    Insert,
    Visual,
    Command,
};

class Editor {
private:
    Mode curr_mode = Mode::Normal;
    Logger logger = Logger("../logfile.txt");
    NotcursesTUI tui;
    CursorManager cm;
    ViewportManager viewport;
    TextBuffer buffer;
    std::string m_filepath;
    bool should_exit;

    std::optional<Cursor> m_visual_start;
    std::optional<Cursor> m_visual_end;

public:
    explicit Editor(const std::string& filepath);

    // Mode-handling methods:
    void set_mode(Mode mode);
    void set_should_exit(bool value);
    void set_visual_end(const Cursor& cursor);
    void insert_mode(int input);
    void command_mode();

    void write_file();

    void run();

    bool execute(const std::unordered_map<std::string,
                                          std::function<void(Editor&)>>& table,
                 const std::string& cmd);
    void update_view();

    TextBuffer& get_buffer();
    CursorManager& get_cm();
    Logger& get_logger();
    const std::string& get_filepath();
    VisualRange get_visual_range();
};

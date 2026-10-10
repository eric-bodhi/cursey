#pragma once

#include "../core/types.h"
#include <cstddef>
#include <optional>

class Editor;
struct notcurses;
struct ncplane;

// Terminal front end built on notcurses. It draws the editor's state and
// turns terminal input into editor keys (see core/keys.h). All editing logic
// lives in Editor; this class holds no editor state of its own.
//
// Screen layout:
//   rows 0 .. n-3   text, with a line-number gutter on the left
//   row  n-2        status line (file name, modified flag, cursor position)
//   row  n-1        command line (messages, mode, ":" commands)
class Tui {
public:
    Tui();
    ~Tui();
    Tui(const Tui&) = delete;
    Tui& operator=(const Tui&) = delete;

    void render(Editor& editor);

    // Blocks until a key is pressed. Returns nullopt for events that are not
    // keys (resize, mouse, key release).
    std::optional<int> read_key();

    // The terminal went away (EOF on input).
    bool closed() const;

private:
    void draw_text(const Editor& editor, std::size_t gutter);
    void draw_status_line(const Editor& editor);
    void draw_command_line(const Editor& editor);
    void place_cursor(const Editor& editor, std::size_t gutter);
    void update_cursor_shape(Mode mode);

    notcurses* m_nc = nullptr;
    ncplane* m_plane = nullptr;
    unsigned m_rows = 0;
    unsigned m_cols = 0;
    std::optional<Mode> m_shape_mode;
    bool m_closed = false;
};

#pragma once

#include "buffer.h"
#include "options.h"
#include "registers.h"
#include "types.h"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// The whole editor state machine. It knows nothing about the terminal: the UI
// feeds it keys with handle_key() and draws whatever the accessors report.
// That keeps every editing feature testable without a screen (see tests/).
class Editor {
public:
    struct Selection {
        Position start; // inclusive
        Position end;   // inclusive
        bool linewise = false;
    };

    Editor();

    // Load `path` into the buffer, or start a new empty file if it does not
    // exist yet.
    bool open(const std::string& path);

    void handle_key(int input);

    // ---- State for the UI ------------------------------------------------
    const Buffer& buffer() const;
    Buffer& buffer();
    Position cursor() const;
    Mode mode() const;
    std::optional<Selection> selection() const;
    const std::string& filepath() const;
    const Options& options() const;
    Options& options();
    bool should_quit() const;

    const std::string& message() const;
    bool message_is_error() const;
    // ":wq" / "/pattern" while typing a command line, empty otherwise.
    std::string command_line() const;
    // Keys typed so far for an unfinished normal-mode command (e.g. "2d").
    std::string pending_keys() const;

    // The UI reports how many text rows/columns it can show; the editor keeps
    // the cursor inside that window.
    void set_view_size(std::size_t rows, std::size_t cols);
    std::size_t view_top() const;
    std::size_t view_left() const;

    // ---- Operations used by ex commands (src/commands) ---------------------
    // An empty path means the current file.
    bool write(const std::string& path);
    void quit(bool force);
    bool edit(const std::string& path, bool force);
    void goto_line(std::size_t row);
    void show_message(std::string text);
    void show_error(std::string text);

private:
    // What a motion evaluates to. `pos` is where the cursor goes; `type`
    // decides which text an operator acts on.
    enum class MotionType { Exclusive, Inclusive, Linewise };
    struct MotionTarget {
        Position pos;
        MotionType type = MotionType::Exclusive;
        bool vertical = false; // j/k: keep the desired column
        bool to_eol = false;   // $: j/k then stay at the end of the line
    };
    // The keys naming a motion: 'w', or 'f' + arg 'x', or 'g' + arg 'g'.
    struct MotionKeys {
        int key = 0;
        int arg = 0;
    };
    enum class ParseResult { Incomplete, Invalid, Done };

    // Mode handlers.
    void handle_normal_key(int input);
    void handle_insert_key(int input);
    void handle_visual_key(int input);
    void handle_command_line_key(int input);

    // Normal/visual command parsing: pending keys -> command.
    ParseResult run_normal_command(const std::vector<int>& keys);
    ParseResult run_visual_command(const std::vector<int>& keys);
    static ParseResult parse_motion(const std::vector<int>& keys,
                                    std::size_t& i, MotionKeys& out);

    // Motions and operators.
    std::optional<MotionTarget> eval_motion(const MotionKeys& spec, int count,
                                            bool has_count, bool for_operator);
    void move_to(const MotionTarget& target);
    void apply_operator(int op, Position from, MotionTarget target, char reg);
    MotionTarget change_word_target(int count, bool bigword) const;

    // Commands.
    void enter_insert(Position pos);
    void open_line(bool below);
    void paste(char reg, bool before, int count);
    void join_lines(int count);
    bool replace_chars(int count, int ch);
    void undo();
    void redo();
    void scroll_half_page(bool down, int count);
    void begin_command_line(char type);
    void execute_command_line();
    bool search(bool forward, int count);
    std::optional<Position> find_match(const std::string& pattern, bool forward,
                                       Position from, bool& wrapped);

    // Cursor helpers.
    void clamp_cursor();
    void update_want_col();
    std::size_t col_for_want(std::size_t row) const;
    std::string indent_of(std::size_t row) const;
    void scroll_to_cursor();

    Buffer m_buffer;
    Registers m_registers;
    Options m_options;
    std::string m_filepath;

    Mode m_mode = Mode::Normal;
    Position m_cursor;
    // Desired display column for j/k (vim's "curswant"); npos means "end of
    // line", set by $.
    std::size_t m_want_col = 0;
    Position m_visual_anchor;

    std::vector<int> m_pending; // keys of the unfinished normal command
    char m_cmdline_type = ':';
    std::string m_cmdline;

    MotionKeys m_last_find;    // for ; and ,
    std::string m_last_search; // for n and N
    bool m_last_search_forward = true;

    std::string m_message;
    bool m_message_error = false;
    bool m_should_quit = false;

    std::size_t m_view_top = 0;
    std::size_t m_view_left = 0;
    std::size_t m_view_rows = 24;
    std::size_t m_view_cols = 80;
};

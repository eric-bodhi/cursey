#pragma once

#include "types.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The text of one file plus its undo history.
//
// Text is a vector of lines (without '\n'); there is always at least one line.
// Every modification goes through the two primitives insert() and erase().
// Each call is recorded as an Edit, so undo/redo is just "apply the inverse
// edits" - any new editing command gets undo support for free.
class Buffer {
public:
    Buffer();

    // Replace the whole text (and forget undo history).
    void set_text(std::string_view text);
    std::string text() const;

    // File I/O. On failure, `error` receives a vim-style message.
    bool load(const std::string& path, std::string& error);
    bool save(const std::string& path, std::string& error);

    std::size_t line_count() const;
    const std::string& line(std::size_t row) const;
    std::size_t line_length(std::size_t row) const;
    std::size_t last_row() const;
    Position end_position() const; // one past the last character

    // Byte offset of the first non-blank character (or the line length).
    std::size_t first_non_blank(std::size_t row) const;

    // Text in [start, end); lines are joined with '\n'.
    std::string get_text(Position start, Position end) const;

    // ---- Primitive edits -------------------------------------------------
    // Insert `text` (may contain '\n') at `pos`. Returns the position right
    // after the inserted text.
    Position insert(Position pos, std::string_view text);
    // Remove the text in [start, end) and return it. To remove a line break,
    // `end` must be at column 0 of the next line.
    std::string erase(Position start, Position end);

    // ---- Convenience edits built on the primitives -----------------------
    void replace(Position start, Position end, std::string_view text);
    void replace_line(std::size_t row, std::string_view text);
    // Delete whole lines [first, last] (keeps at least one empty line).
    std::string delete_lines(std::size_t first, std::size_t last);
    // Insert whole lines before `row` (row == line_count() appends).
    void insert_lines(std::size_t row, std::string_view text);

    // ---- Undo --------------------------------------------------------------
    // Edits made between begin/end form one undo step. Nesting is allowed;
    // only the outermost end_undo_group() commits.
    void begin_undo_group(Position cursor);
    void end_undo_group();
    // Return the cursor position to restore, or nullopt if nothing to undo.
    std::optional<Position> undo();
    std::optional<Position> redo();
    bool can_undo() const;
    bool can_redo() const;

    // Modified relative to the last load/save (undoing back to the saved
    // state makes the buffer unmodified again).
    bool is_modified() const;
    void mark_saved();

    // Incremented on every edit; cheap way for others to notice changes.
    std::uint64_t revision() const;

private:
    struct Edit {
        enum class Kind { Insert, Erase } kind;
        Position pos;
        std::string text;
    };
    struct UndoGroup {
        std::vector<Edit> edits;
        Position cursor_before;
        std::uint64_t id = 0;
    };

    Position raw_insert(Position pos, std::string_view text);
    std::string raw_erase(Position start, Position end);
    void record(Edit edit);
    std::uint64_t state_id() const;
    Position clamp(Position pos) const;

    std::vector<std::string> m_lines;

    std::vector<UndoGroup> m_undo_stack;
    std::vector<UndoGroup> m_redo_stack;
    std::optional<UndoGroup> m_open_group;
    int m_group_depth = 0;
    std::uint64_t m_next_group_id = 1;
    std::uint64_t m_saved_state = 0;
    std::uint64_t m_revision = 0;

    bool m_crlf = false; // file used "\r\n" line endings
};

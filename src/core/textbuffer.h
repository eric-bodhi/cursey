#pragma once

#include "../utils/deque_gb.h"
#include "../utils/log.h"
#include "cursor.h"
#include <string>
#include <variant>
#include <vector>

// forward decl
class CursorManager;

class TextBuffer {
private:
    std::vector<std::variant<std::string, GapBuffer>> buffer;
    std::vector<std::string> original_buffer;
    std::size_t gb_idx = 0;
    Logger tb_logger = Logger("../logfile.txt");
    bool was_modified = false;

public:
    explicit TextBuffer(const std::string& filepath);

    bool load_file(const std::string& filepath);

    [[maybe_unused]] void revert_buffer();
    void revert_buffer(std::vector<std::string> new_buffer);

    std::size_t line_count() const;

    // turns gapbuffer to string if buffer[index] is gb
    std::string get_line(std::size_t index) const;

    std::size_t get_line_length(std::size_t index) const;

    bool is_modified() const;
    void set_modified(const bool& value);

    // has to make original edited line a string and new line a gapbuffer
    void switch_line(std::size_t new_line_idx);

    void move_cursor(const Cursor& cursor);
    void move_cursor(const CursorManager& new_cm);

    void insert(const Cursor& cursor, char c);
    void insert(const CursorManager& cm, char c);

    void erase(const Cursor& cursor);
    void erase(const CursorManager& cm);
    void delete_range(const Cursor& start, const Cursor& end, Logger& logger);

    // new_line at index cm.row + 1
    void new_line(const CursorManager& cm);
    void delete_line(const CursorManager& cm);
    void delete_line(std::size_t line_idx);
};

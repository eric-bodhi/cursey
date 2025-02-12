#pragma once

#include <deque>
#include <string>

class GapBuffer {
private:
    std::deque<char> left;
    std::deque<char> right;

public:
    explicit GapBuffer(const std::string& string = "", const size_t& cursor = 0);

    void move_cursor(size_t index);
    void move_left();
    void move_right();
    void insert(char c);
    void del();

    std::string string_with_gap();
    std::string string_with_gap() const;

    std::string to_string();
    std::string to_string() const;

    size_t size();
    size_t size() const;
};

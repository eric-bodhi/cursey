#include "deque_gb.h"
#include <deque>
#include <string>

GapBuffer::GapBuffer(const std::string& string, const size_t& cursor) {
    for (std::size_t i = 0; i < cursor; i++) {
        left.push_back(string.at(i));
    }
    for (char i : string) {
        right.push_back(i);
    }
    move_cursor(cursor);
}

void GapBuffer::move_cursor(size_t index) {
    // if cursor in same position
    if (index == left.size()) {
        return;
    }

    if (index < left.size()) {
        while (left.size() != index) {
            move_left();
        }
    }

    else {
        while (left.size() != index) {
            move_right();
        }
    }
}

// (in)(de)crement cursor
void GapBuffer::move_left() {
    if (left.empty()) {
        return;
    }

    right.push_front(left.back());
    left.pop_back();
}

void GapBuffer::move_right() {
    if (right.empty()) {
        return;
    }

    left.push_back(right.front());
    right.pop_front();
}

void GapBuffer::insert(const char c) {
    left.push_back(c);
}

void GapBuffer::del() {
    if (left.empty()) {
        if (right.empty()) {
            return;
        }
        right.pop_front();
    } else {
        left.pop_back();
    }
}

std::string GapBuffer::string_with_gap() {
    return std::string(left.begin(), left.end()) + "_" +
           std::string(right.begin(), right.end());
}

std::string GapBuffer::string_with_gap() const {
    return std::string(left.begin(), left.end()) + "_" +
           std::string(right.begin(), right.end());
}

std::string GapBuffer::to_string() {
    return std::string(left.begin(), left.end()) +
           std::string(right.begin(), right.end());
}

std::string GapBuffer::to_string() const {
    return std::string(left.begin(), left.end()) +
           std::string(right.begin(), right.end());
}

size_t GapBuffer::size() {
    return left.size() + right.size();
}

size_t GapBuffer::size() const {
    return left.size() + right.size();
}

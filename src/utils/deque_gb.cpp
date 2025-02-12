#include "deque_gb.h"
#include <deque>
#include <stdexcept>
#include <string>

Gb::Gb(const std::string& string, const size_t& cursor) {
    for (std::size_t i = 0; i < cursor; i++) {
        left.push_back(string.at(i));
    }
    for (std::size_t i = 0; i < string.size(); i++) {
        right.push_back(string.at(i));
    }
    move_cursor(cursor);
}

void Gb::move_cursor(size_t index) {
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
void Gb::move_left() {
    if (left.size() == 0) {
        return;
    }

    right.push_front(left.back());
    left.pop_back();
}

void Gb::move_right() {
    if (right.size() == 0) {
        return;
    }

    left.push_back(right.front());
    right.pop_front();
}

void Gb::insert(const char c) {
    left.push_back(c);
}

void Gb::del() {
    if (left.size() == 0) {
        if (right.size() == 0) {
            return;
        }
        right.pop_front();
    } else {
        left.pop_back();
    }
}

std::string Gb::string_with_gap() {
    return std::string(left.begin(), left.end()) + "_" +
           std::string(right.begin(), right.end());
}

std::string Gb::string_with_gap() const {
    return std::string(left.begin(), left.end()) + "_" +
           std::string(right.begin(), right.end());
}

std::string Gb::to_string() {
    return std::string(left.begin(), left.end()) +
           std::string(right.begin(), right.end());
}

std::string Gb::to_string() const {
    return std::string(left.begin(), left.end()) +
           std::string(right.begin(), right.end());
}

size_t Gb::size() {
    return left.size() + right.size();
}

size_t Gb::size() const {
    return left.size() + right.size();
}

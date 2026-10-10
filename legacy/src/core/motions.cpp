#include "motions.h"
#include "../utils/utf8.h"

#include <cwctype>

namespace motion {

namespace {

bool is_blank(const char32_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == 0;
}

// Character at `p`; the end of a line reads as a virtual '\n'.
char32_t char_at(const Buffer& buf, const Position p) {
    const auto& line = buf.line(p.row);
    return p.col < line.size() ? utf8::decode(line, p.col) : U'\n';
}

// Step to the next/previous character position. Empty lines have exactly one
// position (column 0); non-empty lines have one per character.
bool next_pos(const Buffer& buf, Position& p) {
    const auto& line = buf.line(p.row);
    if (p.col < line.size()) {
        if (const std::size_t n = utf8::next(line, p.col); n < line.size()) {
            p.col = n;
            return true;
        }
    }
    if (p.row >= buf.last_row()) {
        return false;
    }
    p = {p.row + 1, 0};
    return true;
}

bool prev_pos(const Buffer& buf, Position& p) {
    if (p.col > 0) {
        p.col = utf8::prev(buf.line(p.row), p.col);
        return true;
    }
    if (p.row == 0) {
        return false;
    }
    --p.row;
    const auto& line = buf.line(p.row);
    p.col = line.empty() ? 0 : utf8::prev(line, line.size());
    return true;
}

bool is_empty_line(const Buffer& buf, const Position p) {
    return buf.line_length(p.row) == 0;
}

CharClass class_at(const Buffer& buf, const Position p, const bool big) {
    return char_class(char_at(buf, p), big);
}

} // namespace

CharClass char_class(const char32_t c, const bool bigword) {
    if (is_blank(c)) {
        return CharClass::Blank;
    }
    if (bigword) {
        return CharClass::Word;
    }
    if (c == '_' || c >= 0x80 || std::iswalnum(static_cast<wint_t>(c))) {
        return CharClass::Word;
    }
    return CharClass::Punct;
}

Position word_forward(const Buffer& buf, Position p, const bool big) {
    const auto& start_line = buf.line(p.row);
    if (p.col < start_line.size()) {
        // Skip the rest of the current word.
        if (const CharClass c0 = class_at(buf, p, big);
            c0 != CharClass::Blank) {
            while (p.col < start_line.size() && class_at(buf, p, big) == c0) {
                p.col = utf8::next(start_line, p.col);
            }
        }
    }
    // Skip whitespace and line breaks; an empty line counts as a word.
    while (true) {
        const auto& line = buf.line(p.row);
        if (p.col >= line.size()) {
            if (p.row >= buf.last_row()) {
                return {p.row, line.size()};
            }
            p = {p.row + 1, 0};
            if (buf.line_length(p.row) == 0) {
                return p;
            }
            continue;
        }
        if (class_at(buf, p, big) != CharClass::Blank) {
            return p;
        }
        p.col = utf8::next(line, p.col);
    }
}

Position word_end(const Buffer& buf, Position p, const bool big) {
    if (!next_pos(buf, p)) {
        return p;
    }
    while (class_at(buf, p, big) == CharClass::Blank) {
        if (!next_pos(buf, p)) {
            return p;
        }
    }
    const CharClass c0 = class_at(buf, p, big);
    while (true) {
        Position q = p;
        if (!next_pos(buf, q) || q.row != p.row ||
            class_at(buf, q, big) != c0) {
            return p;
        }
        p = q;
    }
}

Position word_backward(const Buffer& buf, Position p, const bool big) {
    if (!prev_pos(buf, p)) {
        return p;
    }
    while (class_at(buf, p, big) == CharClass::Blank) {
        if (is_empty_line(buf, p) || !prev_pos(buf, p)) {
            return p;
        }
    }
    const CharClass c0 = class_at(buf, p, big);
    while (true) {
        Position q = p;
        if (!prev_pos(buf, q) || q.row != p.row ||
            class_at(buf, q, big) != c0) {
            return p;
        }
        p = q;
    }
}

Position word_end_backward(const Buffer& buf, Position p, const bool big) {
    // Move to the start of the current word first.
    if (const CharClass c0 = class_at(buf, p, big); c0 != CharClass::Blank) {
        while (true) {
            Position q = p;
            if (!prev_pos(buf, q) || q.row != p.row ||
                class_at(buf, q, big) != c0) {
                break;
            }
            p = q;
        }
    }
    if (!prev_pos(buf, p)) {
        return {0, 0};
    }
    while (class_at(buf, p, big) == CharClass::Blank &&
           !is_empty_line(buf, p)) {
        if (!prev_pos(buf, p)) {
            return p;
        }
    }
    return p;
}

std::optional<std::size_t>
find_in_line(const std::string_view line, const std::size_t col,
             const std::string_view target, const bool forward, const bool till,
             const int count, const bool skip_adjacent) {
    if (target.empty()) {
        return std::nullopt;
    }
    std::size_t pos = col;
    for (int n = 0; n < count; ++n) {
        std::size_t from = pos;
        if (n == 0 && till && skip_adjacent) {
            from = forward ? utf8::next(line, from) : utf8::prev(line, from);
        }
        std::size_t found;
        if (forward) {
            found = line.find(target, utf8::next(line, from));
        } else {
            found = from == 0 ? std::string_view::npos
                              : line.rfind(target, utf8::prev(line, from));
        }
        if (found == std::string_view::npos) {
            return std::nullopt;
        }
        pos = found;
    }
    if (till) {
        pos = forward ? utf8::prev(line, pos) : utf8::next(line, pos);
    }
    return pos;
}

} // namespace motion

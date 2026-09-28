#include "utf8.h"

#include <cwchar>

namespace utf8 {

std::size_t char_len(const unsigned char lead) {
    if (lead < 0x80) {
        return 1;
    }
    if ((lead >> 5) == 0x6) {
        return 2;
    }
    if ((lead >> 4) == 0xE) {
        return 3;
    }
    if ((lead >> 3) == 0x1E) {
        return 4;
    }
    return 1;
}

std::size_t next(const std::string_view s, const std::size_t i) {
    if (i >= s.size()) {
        return s.size();
    }
    const std::size_t len = char_len(static_cast<unsigned char>(s[i]));
    return i + len > s.size() ? s.size() : i + len;
}

std::size_t prev(const std::string_view s, std::size_t i) {
    if (i == 0) {
        return 0;
    }
    if (i > s.size()) {
        i = s.size();
    }
    --i;
    // Walk back over continuation bytes (10xxxxxx).
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) {
        --i;
    }
    return i;
}

char32_t decode(const std::string_view s, const std::size_t i) {
    if (i >= s.size()) {
        return 0;
    }
    const auto lead = static_cast<unsigned char>(s[i]);
    const std::size_t len = char_len(lead);
    if (len == 1 || i + len > s.size()) {
        return lead;
    }
    char32_t cp = lead & (0x7F >> len);
    for (std::size_t k = 1; k < len; ++k) {
        cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
    }
    return cp;
}

std::u32string decode_all(const std::string_view s) {
    std::u32string out;
    for (std::size_t i = 0; i < s.size(); i = next(s, i)) {
        out.push_back(decode(s, i));
    }
    return out;
}

std::string encode(const char32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

int width(const char32_t cp) {
    if (cp < 0x80) {
        return cp < 0x20 || cp == 0x7F ? 2 : 1; // control chars shown as ^X
    }
    const int w = wcwidth(static_cast<wchar_t>(cp));
    return w < 0 ? 1 : w;
}

std::size_t display_col(const std::string_view line, const std::size_t byte_col,
                        const std::size_t tabstop) {
    std::size_t col = 0;
    for (std::size_t i = 0; i < line.size() && i < byte_col;
         i = next(line, i)) {
        if (line[i] == '\t') {
            col += tabstop - (col % tabstop);
        } else {
            col += static_cast<std::size_t>(width(decode(line, i)));
        }
    }
    return col;
}

std::size_t byte_col_at(const std::string_view line, const std::size_t target,
                        const std::size_t tabstop) {
    std::size_t col = 0;
    std::size_t i = 0;
    while (i < line.size()) {
        const std::size_t w =
            line[i] == '\t' ? tabstop - (col % tabstop)
                            : static_cast<std::size_t>(width(decode(line, i)));
        if (col + w > target) {
            return i;
        }
        col += w;
        i = next(line, i);
    }
    return i;
}

} // namespace utf8

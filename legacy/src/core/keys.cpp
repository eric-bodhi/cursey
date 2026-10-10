#include "keys.h"
#include "../utils/utf8.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace key {

namespace {

constexpr std::array<std::pair<std::string_view, int>, 16> named_keys = {{
    {"esc", Esc},
    {"cr", Enter},
    {"enter", Enter},
    {"return", Enter},
    {"bs", Backspace},
    {"tab", Tab},
    {"space", ' '},
    {"lt", '<'},
    {"up", Up},
    {"down", Down},
    {"left", Left},
    {"right", Right},
    {"del", Delete},
    {"home", Home},
    {"end", End},
    {"pageup", PageUp},
}};

std::string lower(std::string_view s) {
    std::string out(s);
    std::ranges::transform(out, out.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

} // namespace

std::vector<int> parse(const std::string_view notation) {
    std::vector<int> keys;
    std::size_t i = 0;
    while (i < notation.size()) {
        if (notation[i] == '<') {
            if (const std::size_t close = notation.find('>', i + 1);
                close != std::string_view::npos) {
                const std::string name =
                    lower(notation.substr(i + 1, close - i - 1));
                int found = -1;
                if (name.size() == 3 && name.starts_with("c-")) {
                    found = ctrl(name[2]);
                } else if (name == "pagedown") {
                    found = PageDown;
                } else {
                    for (const auto& [n, k] : named_keys) {
                        if (n == name) {
                            found = k;
                        }
                    }
                }
                if (found >= 0) {
                    keys.push_back(found);
                    i = close + 1;
                    continue;
                }
            }
        }
        keys.push_back(static_cast<int>(utf8::decode(notation, i)));
        i = utf8::next(notation, i);
    }
    return keys;
}

std::string to_notation(const int k) {
    switch (k) {
    case Esc:
        return "<Esc>";
    case Enter:
        return "<CR>";
    case Backspace:
        return "<BS>";
    case Tab:
        return "<Tab>";
    case Up:
        return "<Up>";
    case Down:
        return "<Down>";
    case Left:
        return "<Left>";
    case Right:
        return "<Right>";
    case Delete:
        return "<Del>";
    case Home:
        return "<Home>";
    case End:
        return "<End>";
    case PageUp:
        return "<PageUp>";
    case PageDown:
        return "<PageDown>";
    default:
        break;
    }
    if (k > 0 && k < 0x20) {
        return std::string("^") + static_cast<char>(k + '@');
    }
    return utf8::encode(static_cast<char32_t>(k));
}

std::string to_text(const std::vector<int>& keys) {
    std::string out;
    for (const int k : keys) {
        out += utf8::encode(static_cast<char32_t>(k));
    }
    return out;
}

std::vector<int> from_text(const std::string_view text) {
    std::vector<int> keys;
    for (const char32_t cp : utf8::decode_all(text)) {
        keys.push_back(static_cast<int>(cp));
    }
    return keys;
}

} // namespace key

#pragma once

#include <string>
#include <string_view>
#include <vector>

// Keys are plain ints holding a Unicode codepoint. Control characters use their
// ASCII codes (Ctrl-R == 0x12) and non-character keys (arrows, ...) live in the
// Unicode private use area, so every key sequence can be stored as UTF-8 text
// (that is how macros are kept in registers).
namespace key {

constexpr int Tab = '\t';
constexpr int Enter = '\r';
constexpr int Esc = 0x1B;
constexpr int Backspace = 0x7F;

constexpr int SpecialBase = 0xF0000;
constexpr int Up = SpecialBase + 1;
constexpr int Down = SpecialBase + 2;
constexpr int Left = SpecialBase + 3;
constexpr int Right = SpecialBase + 4;
constexpr int Delete = SpecialBase + 5;
constexpr int Home = SpecialBase + 6;
constexpr int End = SpecialBase + 7;
constexpr int PageUp = SpecialBase + 8;
constexpr int PageDown = SpecialBase + 9;

constexpr int ctrl(const char c) {
    return c & 0x1F;
}

// Parse vim key notation: "dw", "ihello<Esc>", "<C-r>", "<CR>", "<lt>".
// Unknown <...> sequences are taken literally.
std::vector<int> parse(std::string_view notation);

// Inverse of parse(), for showing pending keys / debugging.
std::string to_notation(int key);

// Raw UTF-8 round trip (used for macro registers).
std::string to_text(const std::vector<int>& keys);
std::vector<int> from_text(std::string_view text);

} // namespace key

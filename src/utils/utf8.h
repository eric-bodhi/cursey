#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// Minimal UTF-8 helpers. Lines are stored as raw UTF-8 bytes; these functions
// make sure the cursor always lands on the start of a character.
namespace utf8 {

// Length in bytes of the character starting with `lead` (1 for invalid bytes).
std::size_t char_len(unsigned char lead);

// Byte index of the character after the one at `i` (clamped to s.size()).
std::size_t next(std::string_view s, std::size_t i);

// Byte index of the character before `i` (0 if already at the start).
std::size_t prev(std::string_view s, std::size_t i);

// Decode the codepoint starting at byte `i`.
char32_t decode(std::string_view s, std::size_t i);

// Decode a whole string into codepoints (invalid bytes decode as themselves).
std::u32string decode_all(std::string_view s);

std::string encode(char32_t cp);

// Terminal cell width of a codepoint (0, 1 or 2). Tabs are handled by callers.
int width(char32_t cp);

// Display column of byte offset `byte_col`, expanding tabs to `tabstop`.
std::size_t display_col(std::string_view line, std::size_t byte_col,
                        std::size_t tabstop);

// Byte offset of the character covering display column `target` (or the end
// of the line if the line is shorter).
std::size_t byte_col_at(std::string_view line, std::size_t target,
                        std::size_t tabstop);

} // namespace utf8

#pragma once

#include "buffer.h"
#include "types.h"
#include <optional>
#include <string_view>

// Cursor motions as pure functions of (buffer, position), so they are easy to
// unit test. Motions that depend on editor state (j/k with the desired column,
// searches) live in the Editor.
namespace motion {

enum class CharClass { Blank, Punct, Word };

// `bigword` (W/B/E) treats every non-blank character as part of a word.
CharClass char_class(char32_t c, bool bigword);

// One step of w / e / b / ge. Callers loop for counts.
Position word_forward(const Buffer& buf, Position pos, bool bigword);
Position word_end(const Buffer& buf, Position pos, bool bigword);
Position word_backward(const Buffer& buf, Position pos, bool bigword);
Position word_end_backward(const Buffer& buf, Position pos, bool bigword);

// f/F/t/T: search `line` for the count-th `target` (UTF-8 encoded character)
// after (forward) or before `col`. `till` stops one character short.
// `skip_adjacent` is used when repeating t/T with ; so it does not get stuck.
std::optional<std::size_t> find_in_line(std::string_view line, std::size_t col,
                                        std::string_view target, bool forward,
                                        bool till, int count,
                                        bool skip_adjacent = false);

} // namespace motion

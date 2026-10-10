#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Very small single-line C/C++ highlighter. Each line is tokenized on its own,
// so multi-line constructs (/* ... */, raw strings) are not tracked.
enum class TokenType {
    Keyword,
    Literal,
    Operator,
    Preprocessor,
    Comment,
    Identifier,
    Function,
    Type,
    Space,
};

namespace lex {

std::vector<std::string> tokenize(std::string_view line);
TokenType classify_token(std::string_view token);
bool is_operator(std::string_view str);

// One TokenType per byte of `line`.
std::vector<TokenType> highlight_line(std::string_view line);

// Whether the highlighter should be used for this file name.
bool supports_file(std::string_view path);

uint32_t color_of(TokenType type);

} // namespace lex

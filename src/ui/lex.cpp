#include "lex.h"

#include <array>
#include <cctype>
#include <unordered_set>

namespace {

const std::unordered_set<std::string_view> keywords = {
    "if",       "else",       "for",       "while",    "do",        "return",
    "auto",     "const",      "constexpr", "static",   "class",     "struct",
    "enum",     "template",   "typename",  "using",    "namespace", "public",
    "private",  "protected",  "virtual",   "override", "switch",    "case",
    "default",  "break",      "continue",  "new",      "delete",    "true",
    "false",    "nullptr",    "this",      "inline",   "noexcept",  "operator",
    "explicit", "static_cast"};

const std::unordered_set<std::string_view> types = {
    "int",    "void",   "float",    "double", "char",          "bool",
    "size_t", "string", "vector",   "map",    "unordered_map", "unique_ptr",
    "long",   "short",  "unsigned", "signed", "uint32_t",      "int64_t"};

bool is_space(const char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool is_punct(const char c) {
    return std::ispunct(static_cast<unsigned char>(c)) != 0;
}

bool is_number(const std::string_view token) {
    if (token.empty() || !std::isdigit(static_cast<unsigned char>(token[0]))) {
        return false;
    }
    for (const char c : token) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' &&
            c != '\'') {
            return false;
        }
    }
    return true;
}

bool is_identifier(const std::string_view token) {
    if (token.empty() || std::isdigit(static_cast<unsigned char>(token[0]))) {
        return false;
    }
    for (const char c : token) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' &&
            static_cast<unsigned char>(c) < 0x80) {
            return false;
        }
    }
    return true;
}

} // namespace

namespace lex {

std::vector<std::string> tokenize(const std::string_view line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_string = false, in_char = false, in_comment = false;
    bool in_preprocessor = false;

    auto flush = [&] {
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
    };

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];

        if (in_comment) {
            current += c; // a // comment runs to the end of the line
            continue;
        }

        if (in_preprocessor) {
            current += c;
            if (is_space(c)) {
                flush();
                in_preprocessor = false;
            }
            continue;
        }

        if (in_string || in_char) {
            current += c;
            if (c == '\\' && i + 1 < line.size()) {
                current += line[++i]; // escape sequence
                continue;
            }
            if ((in_string && c == '"') || (in_char && c == '\'')) {
                flush();
                in_string = in_char = false;
            }
            continue;
        }

        if (c == '#') {
            flush();
            in_preprocessor = true;
            current += c;
            continue;
        }

        if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            flush();
            current = "//";
            in_comment = true;
            ++i;
            continue;
        }

        if (c == '"' || c == '\'') {
            flush();
            current += c;
            in_string = c == '"';
            in_char = c == '\'';
            continue;
        }

        if (is_space(c)) {
            flush();
            tokens.emplace_back(1, c);
            continue;
        }

        if (is_punct(c) && c != '_') {
            // Grow multi-character operators such as "->" or "<<=".
            if (!current.empty() && is_punct(current[0]) &&
                is_operator(current + c)) {
                current += c;
                continue;
            }
            flush();
            current += c;
            continue;
        }

        if (!current.empty() && is_punct(current[0]) && current[0] != '_') {
            flush();
        }
        current += c;
    }
    flush();
    return tokens;
}

bool is_operator(const std::string_view str) {
    static const std::unordered_set<std::string_view> operators = {
        "+",  "-",  "*",   "/",   "%",   "=",  "<",  ">",  "!",  "&",
        "|",  "^",  "~",   "?",   ":",   ",",  ".",  ";",  "(",  ")",
        "{",  "}",  "[",   "]",   "++",  "--", "->", "<<", ">>", "==",
        "!=", "<=", ">=",  "+=",  "-=",  "*=", "/=", "%=", "&&", "||",
        "::", ".*", "->*", "...", "<<=", ">>="};
    return operators.contains(str);
}

TokenType classify_token(const std::string_view token) {
    if (token.empty() || is_space(token[0])) {
        return TokenType::Space;
    }
    if (token.starts_with("//")) {
        return TokenType::Comment;
    }
    if (token[0] == '#') {
        return TokenType::Preprocessor;
    }
    if (token[0] == '"' || token[0] == '\'' || is_number(token)) {
        return TokenType::Literal;
    }
    if (keywords.contains(token)) {
        return TokenType::Keyword;
    }
    if (types.contains(token)) {
        return TokenType::Type;
    }
    if (is_identifier(token)) {
        return TokenType::Identifier;
    }
    return TokenType::Operator;
}

std::vector<TokenType> highlight_line(const std::string_view line) {
    const auto tokens = tokenize(line);
    std::vector<TokenType> out;
    out.reserve(line.size());
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        TokenType type = classify_token(tokens[i]);
        // Function heuristic: identifier followed by '('.
        if (type == TokenType::Identifier && i + 1 < tokens.size() &&
            tokens[i + 1] == "(") {
            type = TokenType::Function;
        }
        out.insert(out.end(), tokens[i].size(), type);
    }
    return out;
}

bool supports_file(const std::string_view path) {
    constexpr std::array<std::string_view, 7> extensions = {
        ".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".hh"};
    for (const auto ext : extensions) {
        if (path.ends_with(ext)) {
            return true;
        }
    }
    return false;
}

uint32_t color_of(const TokenType type) {
    switch (type) {
    case TokenType::Keyword:
        return 0xE06C75; // red
    case TokenType::Literal:
        return 0x98C379; // green
    case TokenType::Operator:
        return 0x56B6C2; // cyan
    case TokenType::Preprocessor:
        return 0xC678DD; // purple
    case TokenType::Comment:
        return 0x5C6370; // gray
    case TokenType::Function:
        return 0x61AFEF; // blue
    case TokenType::Type:
        return 0xE5C07B; // yellow
    case TokenType::Identifier:
    case TokenType::Space:
        break;
    }
    return 0xABB2BF; // default foreground
}

} // namespace lex

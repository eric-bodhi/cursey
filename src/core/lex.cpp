#include "lex.h"
#include "../utils/log.h"
#include <iostream>
#include <regex>
#include <unordered_map>
#include <unordered_set>

static const std::unordered_set<std::string> keywords = {
    "if",     "else",  "for",    "while",    "return",   "auto",  "const",
    "static", "class", "struct", "template", "typename", "using", "namespace"};

static const std::unordered_set<std::string> types = {
    "int",    "void",   "float",  "double", "char",          "bool",
    "size_t", "string", "vector", "map",    "unordered_map", "unique_ptr"};

namespace lex {

const uint32_t bg_rgb = 0x282C34;
const uint32_t selection_bg = 0xADD8E6;
std::unordered_map<TokenType, uint32_t> color_map = {
    {TokenType::Keyword, 0xE06C75},      // red
    {TokenType::Literal, 0x98C379},      // green
    {TokenType::Operator, 0x56B6C2},     // cyan
    {TokenType::Preprocessor, 0xC678DD}, // purple
    {TokenType::Comment, 0x5C6370},      // gray
    {TokenType::Identifier, 0xABB2BF},   // default
    {TokenType::Function, 0x61AFEF},     // blue
    {TokenType::Type, 0xE5C07B},         // yellow
    {TokenType::Space, 0xABB2BF}         // default
};

    std::vector<std::string> tokenize(const std::string& line) {
        std::vector<std::string> tokens;
        std::string current;
        bool in_string = false, in_char = false, in_comment = false;
        bool in_preprocessor = false;

        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];

            if (in_comment) {
                current += c;
                // Check for end of line comment (no ending for //)
                continue;
            }

            if (in_preprocessor) {
                current += c;
                // Preprocessor continues until end of line
                if (c == '\\') { // Handle line continuation
                    if (i+1 < line.size() && line[i+1] == '\n') {
                        current += '\n';
                        i++;
                    }
                } else if (c == '\n' || isspace(c)) {
                    tokens.push_back(current);
                    current.clear();
                    in_preprocessor = false;
                }
                continue;
            }

            if (in_string || in_char) {
                current += c;
                // Handle escape sequences
                if (c == '\\' && i+1 < line.size()) {
                    current += line[++i];
                    continue;
                }
                if ((in_string && c == '"') || (in_char && c == '\'')) {
                    tokens.push_back(current);
                    current.clear();
                    in_string = in_char = false;
                }
                continue;
            }

            if (c == '#') {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                in_preprocessor = true;
                current += c;
                continue;
            }

            if (c == '/' && i+1 < line.size() && line[i+1] == '/') {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                current = "//";
                in_comment = true;
                i++; // Skip next slash
                continue;
            }

            if (c == '"' || c == '\'') {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                current += c;
                in_string = (c == '"');
                in_char = (c == '\'');
                continue;
            }

            if (isspace(c)) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                current += c;
                tokens.push_back(current);
                current.clear();
                continue;
            }

            if (ispunct(c)) {
                // Handle multi-character operators
                if (!current.empty() && ispunct(current[0])) {
                    // Check if combined with previous punctuation forms a known operator
                    std::string combined = current + c;
                    if (is_operator(combined)) { // You'll need to implement is_operator
                        current += c;
                        continue;
                    }
                }

                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                current += c;
                tokens.push_back(current);
                current.clear();
                continue;
            }

            current += c;
        }

        if (!current.empty()) {
            tokens.push_back(current);
        }

        return tokens;
    }

    bool is_operator(const std::string& str) {
        static const std::unordered_set<std::string> operators = {
                // Single-character
                "+", "-", "*", "/", "%", "=", "<", ">", "!", "&", "|", "^", "~", "?",
                ":", ",", ".", ";", "(", ")", "{", "}", "[", "]",

                // Double-character
                "++", "--", "->", "<<", ">>", "==", "!=", "<=", ">=", "+=", "-=",
                "*=", "/=", "%=", "&&", "||", "::", ".*", "->*",

                // Triple-character (C++ specific)
                "...", "<<=", ">>="
        };

        return operators.count(str) > 0;
    }
    TokenType classify_token(const std::string& token) {
        if (token.empty()) return TokenType::Space;

        // 1. Whitespace detection
        if (token == " " || token == "\t") return TokenType::Space;

        // 2. Preprocessor directives
        if (token[0] == '#') return TokenType::Preprocessor;

        // 3. String/character literals
        if (token.front() == '"' || token.front() == '\'') {
            return TokenType::Literal;
        }

        // 4. Numeric literals (including hex/octal/binary)
        static const std::regex number_re{
                "^[+-]?(?:0[xX][0-9a-fA-F]+|0[bB][01]+|0[0-7]*|[1-9][0-9]*|0)"
                "(?:\\.[0-9]*)?(?:[eE][+-]?[0-9]+)?(?:u|U|l|L|ll|LL|f|F)?$"
        };
        if (std::regex_match(token, number_re)) {
            return TokenType::Literal;
        }

        // 5. Keywords
        if (keywords.count(token)) return TokenType::Keyword;

        // 6. Type names
        if (types.count(token)) return TokenType::Type;

        // 7. Operators and punctuation
        if (is_operator(token)) return TokenType::Operator;

        // 8. Identifiers (potential functions)
        static const std::regex identifier_re("^[a-zA-Z_][a-zA-Z0-9_]*$");
        if (std::regex_match(token, identifier_re)) {
            return TokenType::Identifier;
        }

        // 9. Comments (fallthrough from tokenizer)
        if (token.find("//") == 0 || token.find("/*") == 0) {
            return TokenType::Comment;
        }

        // Default to operator for unknown punctuation
        return TokenType::Operator;
    }

void highlight_line(const std::string& line,
                    const std::function<void(int, TokenType, char)>& callback) {
    auto tokens = tokenize(line);
    int x = 0;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        TokenType type = classify_token(token);

        // Function detection heuristic
        if (type == TokenType::Identifier && i + 1 < tokens.size() &&
            tokens[i + 1] == "(") {
            type = TokenType::Function;
        }

        for (size_t j = 0; j < token.size(); ++j) {
            callback(x + j, type, token[j]);
        }
        x += token.size();
    }
}

} // namespace lex

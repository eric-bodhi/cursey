#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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
extern std::unordered_map<TokenType, uint32_t> color_map;
extern const uint32_t bg_rgb;
extern const uint32_t selection_bg;
std::vector<std::string> tokenize(const std::string& line);
TokenType classify_token(const std::string& token);
void highlight_line(const std::string& line,
                    const std::function<void(int, TokenType, char)>& callback);
bool is_operator(const std::string& str);

} // namespace lex

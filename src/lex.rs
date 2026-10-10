//! Very small single-line C/C++ highlighter. Each line is tokenized on its
//! own, so multi-line constructs (/* ... */, raw strings) are not tracked.

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TokenType {
    Keyword,
    Literal,
    Operator,
    Preprocessor,
    Comment,
    Identifier,
    Function,
    Type,
    Space,
}

#[rustfmt::skip]
const KEYWORDS: &[&str] = &[
    "if", "else", "for", "while", "do", "return", "auto", "const",
    "constexpr", "static", "class", "struct", "enum", "template", "typename",
    "using", "namespace", "public", "private", "protected", "virtual",
    "override", "switch", "case", "default", "break", "continue", "new",
    "delete", "true", "false", "nullptr", "this", "inline", "noexcept",
    "operator", "explicit", "static_cast",
];

#[rustfmt::skip]
const TYPES: &[&str] = &[
    "int", "void", "float", "double", "char", "bool", "size_t", "string",
    "vector", "map", "unordered_map", "unique_ptr", "long", "short",
    "unsigned", "signed", "uint32_t", "int64_t",
];

#[rustfmt::skip]
const OPERATORS: &[&str] = &[
    "+", "-", "*", "/", "%", "=", "<", ">", "!", "&", "|", "^", "~", "?", ":",
    ",", ".", ";", "(", ")", "{", "}", "[", "]", "++", "--", "->", "<<", ">>",
    "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "&&", "||", "::",
    ".*", "->*", "...", "<<=", ">>=",
];

/// C's isspace().
fn is_space(c: char) -> bool {
    c.is_ascii_whitespace() || c == '\x0b'
}

fn is_number(token: &str) -> bool {
    token.starts_with(|c: char| c.is_ascii_digit())
        && token
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || c == '.' || c == '\'')
}

fn is_identifier(token: &str) -> bool {
    !token.is_empty()
        && !token.starts_with(|c: char| c.is_ascii_digit())
        && token
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || c == '_' || !c.is_ascii())
}

pub fn is_operator(s: &str) -> bool {
    OPERATORS.contains(&s)
}

/// End the token being built (`line[*start..i]`) and start a new one at `i`.
fn flush<'a>(
    tokens: &mut Vec<&'a str>,
    line: &'a str,
    start: &mut usize,
    i: usize,
) {
    if *start < i {
        tokens.push(&line[*start..i]);
    }
    *start = i;
}

/// Split `line` into tokens. The tokens cover the whole line, in order.
pub fn tokenize(line: &str) -> Vec<&str> {
    let mut tokens = Vec::new();
    // The token being built is always `line[start..i]`.
    let mut start = 0;
    let (mut in_string, mut in_char) = (false, false);
    let (mut in_comment, mut in_preprocessor) = (false, false);
    let mut chars = line.char_indices().peekable();

    while let Some((i, c)) = chars.next() {
        let end = i + c.len_utf8();

        if in_comment {
            continue; // a // comment runs to the end of the line
        }

        if in_preprocessor {
            if is_space(c) {
                flush(&mut tokens, line, &mut start, end);
                in_preprocessor = false;
            }
            continue;
        }

        if in_string || in_char {
            if c == '\\' {
                chars.next(); // escape sequence
                continue;
            }
            if (in_string && c == '"') || (in_char && c == '\'') {
                flush(&mut tokens, line, &mut start, end);
                in_string = false;
                in_char = false;
            }
            continue;
        }

        if c == '#' {
            flush(&mut tokens, line, &mut start, i);
            in_preprocessor = true;
            continue;
        }

        if c == '/' && chars.peek().is_some_and(|&(_, d)| d == '/') {
            flush(&mut tokens, line, &mut start, i);
            chars.next();
            in_comment = true;
            continue;
        }

        if c == '"' || c == '\'' {
            flush(&mut tokens, line, &mut start, i);
            in_string = c == '"';
            in_char = c == '\'';
            continue;
        }

        if is_space(c) {
            flush(&mut tokens, line, &mut start, i);
            flush(&mut tokens, line, &mut start, end);
            continue;
        }

        let current_is_punct = start < i
            && line[start..].starts_with(|d: char| d.is_ascii_punctuation());
        if c.is_ascii_punctuation() && c != '_' {
            // Grow multi-character operators such as "->" or "<<=".
            if current_is_punct && is_operator(&line[start..end]) {
                continue;
            }
            flush(&mut tokens, line, &mut start, i);
            continue;
        }

        if current_is_punct && !line[start..].starts_with('_') {
            flush(&mut tokens, line, &mut start, i);
        }
    }
    flush(&mut tokens, line, &mut start, line.len());
    tokens
}

pub fn classify_token(token: &str) -> TokenType {
    let Some(first) = token.chars().next() else {
        return TokenType::Space;
    };
    if is_space(first) {
        TokenType::Space
    } else if token.starts_with("//") {
        TokenType::Comment
    } else if first == '#' {
        TokenType::Preprocessor
    } else if first == '"' || first == '\'' || is_number(token) {
        TokenType::Literal
    } else if KEYWORDS.contains(&token) {
        TokenType::Keyword
    } else if TYPES.contains(&token) {
        TokenType::Type
    } else if is_identifier(token) {
        TokenType::Identifier
    } else {
        TokenType::Operator
    }
}

/// One TokenType per byte of `line`.
pub fn highlight_line(line: &str) -> Vec<TokenType> {
    let tokens = tokenize(line);
    let mut out = Vec::with_capacity(line.len());
    for (i, token) in tokens.iter().enumerate() {
        let mut kind = classify_token(token);
        // Function heuristic: identifier followed by '('.
        if kind == TokenType::Identifier && tokens.get(i + 1) == Some(&"(") {
            kind = TokenType::Function;
        }
        out.extend(std::iter::repeat_n(kind, token.len()));
    }
    out
}

/// Whether the highlighter should be used for this file name.
pub fn supports_file(path: &str) -> bool {
    const EXTENSIONS: [&str; 7] =
        [".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".hh"];
    EXTENSIONS.iter().any(|ext| path.ends_with(ext))
}

pub fn color_of(kind: TokenType) -> u32 {
    match kind {
        TokenType::Keyword => 0xE06C75,      // red
        TokenType::Literal => 0x98C379,      // green
        TokenType::Operator => 0x56B6C2,     // cyan
        TokenType::Preprocessor => 0xC678DD, // purple
        TokenType::Comment => 0x5C6370,      // gray
        TokenType::Function => 0x61AFEF,     // blue
        TokenType::Type => 0xE5C07B,         // yellow
        TokenType::Identifier | TokenType::Space => 0xABB2BF, // foreground
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tokens_cover_the_line() {
        let line = "#include <x> int main() { return a->b; } // done";
        assert_eq!(tokenize(line).concat(), line);
    }

    #[test]
    fn tokenize_splits_like_the_cpp_version() {
        assert_eq!(
            tokenize("x->y += \"a\\\"b\";"),
            ["x", "->", "y", " ", "+=", " ", "\"a\\\"b\"", ";"]
        );
        assert_eq!(tokenize("#define X 1"), ["#define ", "X", " ", "1"]);
        assert_eq!(tokenize("a // b c"), ["a", " ", "// b c"]);
        assert_eq!(tokenize("_foo"), ["_foo"]);
    }

    #[test]
    fn highlight_marks_functions_keywords_and_types() {
        let types = highlight_line("int f(x)");
        assert_eq!(types[0], TokenType::Type);
        assert_eq!(types[4], TokenType::Function);
        assert_eq!(types[5], TokenType::Operator);
        assert_eq!(highlight_line("return")[0], TokenType::Keyword);
    }
}

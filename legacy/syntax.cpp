#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Token.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include <iostream>
#include <vector>
#include <string>

using namespace clang;
using namespace llvm;

enum HighlightStyle {
    HS_Keyword,
    HS_Literal,
    HS_Comment,
    HS_Identifier,
    HS_Preprocessor,
    HS_Default,
    HS_Namespace,
    HS_Type
};

HighlightStyle getStyleForToken(const Token& token, StringRef tokenText,
                                const LangOptions& langOpts) {
    switch (token.getKind()) {
        case tok::kw_int: case tok::kw_char: case tok::kw_bool:
        case tok::kw_float: case tok::kw_double: case tok::kw_void:
        case tok::kw_short: case tok::kw_long: case tok::kw_signed:
        case tok::kw_unsigned: case tok::kw_wchar_t: case tok::kw_char16_t:
        case tok::kw_char32_t: case tok::kw_char8_t:
            return HS_Type;

        case tok::kw_class: case tok::kw_return: case tok::kw_if:
        case tok::kw_else: case tok::kw_for: case tok::kw_while:
        case tok::kw_switch: case tok::kw_case: case tok::kw_default:
        case tok::kw_do: case tok::kw_try: case tok::kw_catch:
        case tok::kw_public: case tok::kw_private: case tok::kw_protected:
        case tok::kw_struct: case tok::kw_union: case tok::kw_enum:
        case tok::kw_template: case tok::kw_typename: case tok::kw_const:
        case tok::kw_volatile: case tok::kw_static: case tok::kw_extern:
        case tok::kw_auto: case tok::kw_using: case tok::kw_virtual:
            return HS_Keyword;

        case tok::kw_namespace:
            return HS_Namespace;

        case tok::numeric_constant: case tok::string_literal:
        case tok::char_constant: case tok::wide_char_constant:
        case tok::utf8_char_constant: case tok::utf16_char_constant:
        case tok::utf32_char_constant: case tok::wide_string_literal:
        case tok::utf8_string_literal: case tok::utf16_string_literal:
        case tok::utf32_string_literal:
            return HS_Literal;

        case tok::comment:
            return HS_Comment;

        case tok::hash: case tok::hashhash:
            return HS_Preprocessor;

        case tok::identifier: {
            if (IdentifierInfo* ii = token.getIdentifierInfo()) {
                if (ii->isKeyword(langOpts)) {
                    return ii->getTokenID() == tok::kw_namespace ? HS_Namespace : HS_Keyword;
                }
            }
            return HS_Identifier;
        }

        case tok::raw_identifier: {
            static const StringSet<> keywords = {
                    "int", "return", "if", "else", "for", "while", "class",
                    "char", "float", "double", "void", "short", "long", "signed",
                    "unsigned", "namespace", "struct", "union", "enum", "template",
                    "typename", "const", "volatile", "static", "extern", "auto"
            };

            if (keywords.contains(tokenText))
                return tokenText == "namespace" ? HS_Namespace :
                       (tokenText == "int" || tokenText == "char" ||
                        tokenText == "float" || tokenText == "double" ||
                        tokenText == "void" || tokenText == "short" ||
                        tokenText == "long" || tokenText == "signed" ||
                        tokenText == "unsigned" || tokenText == "string") ? HS_Type : HS_Keyword;

            return HS_Identifier;
        }

        default:
            return HS_Default;
    }
}

void applyHighlighting(const std::string& line, unsigned start, unsigned end,
                       HighlightStyle style) {
    if (start >= line.size() || end > line.size() || start >= end) return;
    end = std::min(end, static_cast<unsigned>(line.size()));

    const char* color_code;
    switch (style) {
        case HS_Type:        color_code = "\033[1;36m"; break;
        case HS_Keyword:     color_code = "\033[1;34m"; break;
        case HS_Literal:    color_code = "\033[1;32m"; break;
        case HS_Comment:    color_code = "\033[1;30m"; break;
        case HS_Preprocessor: color_code = "\033[1;35m"; break;
        case HS_Namespace:  color_code = "\033[1;33m"; break;
        default:            color_code = "\033[0m";
    }

    std::cout << color_code << line.substr(start, end - start) << "\033[0m";
}

int main() {
    std::vector<std::string> codeLines = {
            "#include <iostream>",
            "int main() {",
            "    // comment",
            "    std::cout << \"Hello World!\";",
            "    return 0;",
            "}"
    };

    // Combine lines into single buffer
    std::string combinedCode;
    for (const auto& line : codeLines) {
        combinedCode += line + "\n";
    }

    // Minimal diagnostic setup
    IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
    auto DiagOpts = new DiagnosticOptions();
    IntrusiveRefCntPtr<DiagnosticsEngine> Diags(
            new DiagnosticsEngine(DiagID, DiagOpts));

    FileManager FM((FileSystemOptions()));
    SourceManager SM(*Diags, reinterpret_cast<FileManager &>(FM));
    LangOptions langOpts;
    langOpts.CPlusPlus = true;

    // Create memory buffer from combined code
    auto buffer = MemoryBuffer::getMemBuffer(combinedCode);
    FileID FID = SM.createFileID(std::move(buffer));
    const char* bufStart = SM.getBufferData(FID).data();

    Lexer lexer(SM.getLocForStartOfFile(FID), langOpts,
                bufStart, bufStart, bufStart + combinedCode.size());

    Token token{};
    while (!lexer.LexFromRawLexer(token)) {
        if (token.is(tok::eof)) break;

        unsigned startOffset = SM.getFileOffset(token.getLocation());
        unsigned endOffset = startOffset + token.getLength();

        StringRef tokenText(bufStart + startOffset, endOffset - startOffset);
        HighlightStyle style = getStyleForToken(token, tokenText, langOpts);
        applyHighlighting(combinedCode, startOffset, endOffset, style);
        std::cout << "\n";
    }

    return 0;
}
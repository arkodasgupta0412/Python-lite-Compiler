#include "compiler/lexer.hpp"

#include <cctype>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "compiler/lex_automata.hpp"
#include "compiler/preprocessor.hpp"

namespace cd {
namespace {

const AutomataEngine& engine() {
    static const AutomataEngine eng(defaultPatterns());
    return eng;
}

TokenType mapKeyword(const std::string& text) {
    static const std::unordered_map<std::string, TokenType> key = {
        {"print", TokenType::KEYWORD},
        {"for", TokenType::KEYWORD},
        {"in", TokenType::KEYWORD},
        {"if", TokenType::KEYWORD},
        {"else", TokenType::KEYWORD},
        {"range", TokenType::KEYWORD},
        {"true", TokenType::BOOLEAN},
        {"false", TokenType::BOOLEAN},
    };

    auto it = key.find(text);
    if (it == key.end()) {
        return TokenType::IDENTIFIER;
    }
    return it->second;
}

Token readString(
    const std::string& lineText,
    std::size_t& idx,
    int line,
    int& col,
    char quote
) {
    int startCol = col;
    ++idx;
    ++col;

    std::string value;
    while (idx < lineText.size() && lineText[idx] != quote) {
        if (lineText[idx] == '\\' && idx + 1 < lineText.size()) {
            char esc = lineText[idx + 1];
            if (esc == 'n') {
                value.push_back('\n');
            } else if (esc == 't') {
                value.push_back('\t');
            } else {
                value.push_back(esc);
            }
            idx += 2;
            col += 2;
            continue;
        }

        value.push_back(lineText[idx]);
        ++idx;
        ++col;
    }

    if (idx >= lineText.size()) {
        throw LexicalError(
            "Unterminated string at line " + std::to_string(line) + ", column " + std::to_string(startCol)
        );
    }

    ++idx;
    ++col;
    return Token{TokenType::STRING, value, line, startCol};
}

}  // namespace

std::vector<Token> Lexer::tokenize() const {
    Preprocessor prep;
    std::string src = prep.run(source_);

    std::vector<Token> tokens;
    std::vector<int> indent;
    indent.push_back(0);

    std::istringstream in(src);
    std::string lineText;
    int line = 0;

    auto emitIndent = [&](int curIndent) {
        if (curIndent > indent.back()) {
            indent.push_back(curIndent);
            tokens.push_back(Token{TokenType::INDENT, "<INDENT>", line, 1});
            return;
        }

        while (curIndent < indent.back()) {
            indent.pop_back();
            tokens.push_back(Token{TokenType::DEDENT, "<DEDENT>", line, 1});
        }

        if (curIndent != indent.back()) {
            throw LexicalError("Inconsistent indentation at line " + std::to_string(line));
        }
    };

    while (std::getline(in, lineText)) {
        ++line;
        if (!lineText.empty() && lineText.back() == '\r') {
            lineText.pop_back();
        }

        std::size_t idx = 0;
        int curIndent = 0;

        while (idx < lineText.size() && (lineText[idx] == ' ' || lineText[idx] == '\t')) {
            curIndent += (lineText[idx] == '\t') ? 4 : 1;
            ++idx;
        }

        if (idx >= lineText.size()) {
            continue;
        }

        emitIndent(curIndent);
        int col = static_cast<int>(idx) + 1;

        while (idx < lineText.size()) {
            char ch = lineText[idx];

            if (ch == ' ' || ch == '\t' || ch == '\r') {
                ++idx;
                ++col;
                continue;
            }

            if (ch == '\'' || ch == '"') {
                tokens.push_back(readString(lineText, idx, line, col, ch));
                continue;
            }

            std::size_t len = 0;
            TokenType type = TokenType::ENDMARKER;
            bool ok = engine().longestMatch(lineText, idx, len, type);

            if (!ok || len == 0) {
                throw LexicalError(
                    "Unexpected character '" + std::string(1, ch) + "' at line " + std::to_string(line) +
                    ", column " + std::to_string(col)
                );
            }

            std::string lexeme = lineText.substr(idx, len);
            if (type == TokenType::IDENTIFIER) {
                type = mapKeyword(lexeme);
            }

            tokens.push_back(Token{type, lexeme, line, col});
            idx += len;
            col += static_cast<int>(len);
        }

        tokens.push_back(Token{TokenType::NEWLINE, "\\n", line, static_cast<int>(lineText.size()) + 1});
    }

    ++line;
    while (indent.size() > 1) {
        indent.pop_back();
        tokens.push_back(Token{TokenType::DEDENT, "<DEDENT>", line, 1});
    }

    tokens.push_back(Token{TokenType::ENDMARKER, "", line, 1});
    return tokens;
}

}  // namespace cd

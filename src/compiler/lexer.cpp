#include "compiler/lexer.hpp"

#include <cctype>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace cd {

std::vector<Token> Lexer::tokenize() const {
  std::vector<Token> tokens;
  std::vector<int> indentStack{0};
  std::istringstream in(source_);
  std::string lineText;
  int line = 0;

  auto emitIndentChanges = [&](int currentIndent) {
    if (currentIndent > indentStack.back()) {
      indentStack.push_back(currentIndent);
      tokens.push_back(Token{TokenType::INDENT, "<INDENT>", line, 1});
      return;
    }
    while (currentIndent < indentStack.back()) {
      indentStack.pop_back();
      tokens.push_back(Token{TokenType::DEDENT, "<DEDENT>", line, 1});
    }
    if (currentIndent != indentStack.back()) {
      throw LexicalError("Inconsistent indentation at line " + std::to_string(line));
    }
  };

  while (std::getline(in, lineText)) {
    ++line;
    if (!lineText.empty() && lineText.back() == '\r') {
      lineText.pop_back();
    }
    std::size_t i = 0;
    int indent = 0;
    while (i < lineText.size() && (lineText[i] == ' ' || lineText[i] == '\t')) {
      indent += (lineText[i] == '\t') ? 4 : 1;
      ++i;
    }

    if (i >= lineText.size() || lineText[i] == '#') {
      continue;
    }

    emitIndentChanges(indent);
    int column = static_cast<int>(i) + 1;

    while (i < lineText.size()) {
      char ch = lineText[i];

      if (ch == ' ' || ch == '\t' || ch == '\r') {
        ++i;
        ++column;
        continue;
      }
      if (ch == '#') break;

      if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
        std::size_t start = i;
        int startCol = column;
        while (i < lineText.size() &&
               (std::isalnum(static_cast<unsigned char>(lineText[i])) || lineText[i] == '_')) {
          ++i;
          ++column;
        }
        const std::string lexeme = lineText.substr(start, i - start);
        static const std::unordered_map<std::string, TokenType> keywords = {
            {"print", TokenType::PRINT}, {"for", TokenType::FOR},  {"in", TokenType::IN},
            {"if", TokenType::IF},       {"else", TokenType::ELSE},
            {"range", TokenType::RANGE}, {"true", TokenType::BOOL}, {"false", TokenType::BOOL},
        };
        auto it = keywords.find(lexeme);
        tokens.push_back(Token{it == keywords.end() ? TokenType::IDENT : it->second, lexeme, line, startCol});
        continue;
      }

      if (std::isdigit(static_cast<unsigned char>(ch))) {
        std::size_t start = i;
        int startCol = column;
        bool hasDot = false;
        while (i < lineText.size() &&
               (std::isdigit(static_cast<unsigned char>(lineText[i])) || (lineText[i] == '.' && !hasDot))) {
          if (lineText[i] == '.') hasDot = true;
          ++i;
          ++column;
        }
        std::string lexeme = lineText.substr(start, i - start);
        tokens.push_back(Token{hasDot ? TokenType::FLOAT : TokenType::INT, lexeme, line, startCol});
        continue;
      }

      if (ch == '\'' || ch == '"') {
        char quote = ch;
        int startCol = column;
        ++i;
        ++column;
        std::string value;
        while (i < lineText.size() && lineText[i] != quote) {
          if (lineText[i] == '\\' && i + 1 < lineText.size()) {
            char esc = lineText[i + 1];
            switch (esc) {
              case 'n':
                value.push_back('\n');
                break;
              case 't':
                value.push_back('\t');
                break;
              default:
                value.push_back(esc);
                break;
            }
            i += 2;
            column += 2;
          } else {
            value.push_back(lineText[i]);
            ++i;
            ++column;
          }
        }
        if (i >= lineText.size()) {
          throw LexicalError("Unterminated string at line " + std::to_string(line) + ", column " +
                             std::to_string(startCol));
        }
        ++i;
        ++column;
        tokens.push_back(Token{TokenType::STRING, value, line, startCol});
        continue;
      }

      int startCol = column;
      auto three = lineText.substr(i, std::min<std::size_t>(3, lineText.size() - i));
      auto two = lineText.substr(i, std::min<std::size_t>(2, lineText.size() - i));
      if (three == "--=") {
        tokens.push_back(Token{TokenType::MINUS_ASSIGN, "--=", line, startCol});
        i += 3;
        column += 3;
        continue;
      }
      if (two == "+=") {
        tokens.push_back(Token{TokenType::PLUS_ASSIGN, "+=", line, startCol});
        i += 2;
        column += 2;
        continue;
      }
      if (two == "-=") {
        tokens.push_back(Token{TokenType::MINUS_ASSIGN, "-=", line, startCol});
        i += 2;
        column += 2;
        continue;
      }
      if (two == "--") {
        tokens.push_back(Token{TokenType::MINUS, "--", line, startCol});
        i += 2;
        column += 2;
        continue;
      }

      TokenType type;
      bool ok = true;
      switch (ch) {
        case '=':
          type = TokenType::ASSIGN;
          break;
        case '+':
          type = TokenType::PLUS;
          break;
        case '-':
          type = TokenType::MINUS;
          break;
        case '*':
          type = TokenType::STAR;
          break;
        case '/':
          type = TokenType::SLASH;
          break;
        case '(':
          type = TokenType::LPAREN;
          break;
        case ')':
          type = TokenType::RPAREN;
          break;
        case '[':
          type = TokenType::LBRACKET;
          break;
        case ']':
          type = TokenType::RBRACKET;
          break;
        case ',':
          type = TokenType::COMMA;
          break;
        case ':':
          type = TokenType::COLON;
          break;
        case ';':
          type = TokenType::SEMICOLON;
          break;
        default:
          ok = false;
          break;
      }
      if (!ok) {
        throw LexicalError("Unexpected character '" + std::string(1, ch) + "' at line " +
                           std::to_string(line) + ", column " + std::to_string(column));
      }
      tokens.push_back(Token{type, std::string(1, ch), line, startCol});
      ++i;
      ++column;
    }

    tokens.push_back(Token{TokenType::NEWLINE, "\\n", line, static_cast<int>(lineText.size()) + 1});
  }

  ++line;
  while (indentStack.size() > 1) {
    indentStack.pop_back();
    tokens.push_back(Token{TokenType::DEDENT, "<DEDENT>", line, 1});
  }
  tokens.push_back(Token{TokenType::END_OF_FILE, "", line, 1});
  return tokens;
}

}  // namespace cd

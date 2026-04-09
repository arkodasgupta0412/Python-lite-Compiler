#include "compiler/preprocessor.hpp"

namespace cd {

std::string Preprocessor::run(const std::string& source) const {
    std::string out = source;

    bool inSingle = false;
    bool inDouble = false;
    bool inTripleSingle = false;
    bool inTripleDouble = false;
    bool inLineComment = false;
    bool inBlockComment = false;
    bool escaping = false;

    for (std::size_t i = 0; i < out.size(); ++i) {
        char cur = out[i];
        char nxt = (i + 1 < out.size()) ? out[i + 1] : '\0';
        char nxt2 = (i + 2 < out.size()) ? out[i + 2] : '\0';

        if (inLineComment) {
            if (cur == '\n') {
                inLineComment = false;
            } else {
                out[i] = ' ';
            }
            continue;
        }

        if (inBlockComment) {
            if (cur == '*' && nxt == '/') {
                out[i] = ' ';
                out[i + 1] = ' ';
                ++i;
                inBlockComment = false;
            } else if (cur != '\n') {
                out[i] = ' ';
            }
            continue;
        }

        if (inTripleSingle) {
            if (cur == '\'' && nxt == '\'' && nxt2 == '\'') {
                out[i] = ' ';
                out[i + 1] = ' ';
                out[i + 2] = ' ';
                i += 2;
                inTripleSingle = false;
            } else if (cur != '\n') {
                out[i] = ' ';
            }
            continue;
        }

        if (inTripleDouble) {
            if (cur == '"' && nxt == '"' && nxt2 == '"') {
                out[i] = ' ';
                out[i + 1] = ' ';
                out[i + 2] = ' ';
                i += 2;
                inTripleDouble = false;
            } else if (cur != '\n') {
                out[i] = ' ';
            }
            continue;
        }

        if (inSingle || inDouble) {
            if (escaping) {
                escaping = false;
                continue;
            }
            if (cur == '\\') {
                escaping = true;
                continue;
            }
            if (inSingle && cur == '\'') {
                inSingle = false;
            } else if (inDouble && cur == '"') {
                inDouble = false;
            }
            continue;
        }

        if (cur == '\'' && nxt == '\'' && nxt2 == '\'') {
            out[i] = ' ';
            out[i + 1] = ' ';
            out[i + 2] = ' ';
            i += 2;
            inTripleSingle = true;
            continue;
        }

        if (cur == '"' && nxt == '"' && nxt2 == '"') {
            out[i] = ' ';
            out[i + 1] = ' ';
            out[i + 2] = ' ';
            i += 2;
            inTripleDouble = true;
            continue;
        }

        if (cur == '\'') {
            inSingle = true;
            continue;
        }
        if (cur == '"') {
            inDouble = true;
            continue;
        }

        if (cur == '/' && nxt == '/') {
            out[i] = ' ';
            out[i + 1] = ' ';
            ++i;
            inLineComment = true;
            continue;
        }

        if (cur == '/' && nxt == '*') {
            out[i] = ' ';
            out[i + 1] = ' ';
            ++i;
            inBlockComment = true;
            continue;
        }

        if (cur == '#') {
            out[i] = ' ';
            inLineComment = true;
            continue;
        }
    }

    return out;
}

}  // namespace cd

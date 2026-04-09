#include "compiler/lex_automata.hpp"

#include <algorithm>
#include <stdexcept>

namespace cd {
namespace {

bool isMeta(char c) {
    return c == '|' || c == '*' || c == '+' || c == '?' || c == '(' || c == ')' || c == '.';
}

std::string esc(char c) {
    std::string s;
    if (isMeta(c) || c == '\\') {
        s.push_back('\\');
    }
    s.push_back(c);
    return s;
}

std::string alt(const std::string& chars) {
    std::string out = "(";
    for (std::size_t i = 0; i < chars.size(); ++i) {
        out += esc(chars[i]);
        if (i + 1 < chars.size()) {
            out += '|';
        }
    }
    out += ')';
    return out;
}

}  // namespace

std::vector<TokenPattern> defaultPatterns() {
    const std::string digits = alt("0123456789");
    const std::string low = alt("abcdefghijklmnopqrstuvwxyz");
    const std::string up = alt("ABCDEFGHIJKLMNOPQRSTUVWXYZ");

    const std::string letter = "(" + low + "|" + up + "|_)";
    const std::string alnum = "(" + letter + "|" + digits + ")";

    const std::string intRx = digits + "+";
    const std::string floatRx = digits + "+\\." + digits + "+";
    const std::string idRx = letter + alnum + "*";

    return {
        {TokenType::OPERATOR, "\\+\\=", 1},
        {TokenType::OPERATOR, "\\-\\=", 2},
        {TokenType::OPERATOR, "\\=", 3},
        {TokenType::OPERATOR, "\\+", 4},
        {TokenType::OPERATOR, "\\-", 5},
        {TokenType::OPERATOR, "\\*", 6},
        {TokenType::OPERATOR, "\\/", 7},
        {TokenType::PUNCTUATOR, "\\(", 8},
        {TokenType::PUNCTUATOR, "\\)", 9},
        {TokenType::PUNCTUATOR, "\\[", 10},
        {TokenType::PUNCTUATOR, "\\]", 11},
        {TokenType::PUNCTUATOR, "\\,", 12},
        {TokenType::PUNCTUATOR, "\\:", 13},
        {TokenType::PUNCTUATOR, "\\;", 14},
        {TokenType::FLOAT, floatRx, 15},
        {TokenType::NUMBER, intRx, 16},
        {TokenType::IDENTIFIER, idRx, 17},
    };
}

std::size_t AutomataEngine::StateSetHash::operator()(const std::vector<int>& set) const {
    std::size_t seed = static_cast<std::size_t>(2166136261u);
    for (int s : set) {
        seed ^= static_cast<std::size_t>(s + 0x9e3779b9);
        seed *= 1099511628211ull;
    }
    return seed;
}

AutomataEngine::AutomataEngine(const std::vector<TokenPattern>& patterns) {
    nfa_.start = nfa_.nextId++;
    for (const auto& p : patterns) {
        std::vector<RTok> tok = regexTokens(withConcat(p.regex));
        std::vector<RTok> post = postfix(tok);
        Fragment f = buildNfa(post, p.priority, p.tokenType);
        nfa_.epsEdges[nfa_.start].push_back(f.start);
    }
}

bool AutomataEngine::longestMatch(
    const std::string& text,
    std::size_t start,
    std::size_t& len,
    TokenType& type
) const {
    len = 0;

    std::vector<int> cur = epsClosure(std::vector<int>{nfa_.start});
    std::unordered_map<std::vector<int>, std::unordered_map<char, std::vector<int>>, StateSetHash> cache;

    bool ok = false;

    for (std::size_t i = start; i < text.size(); ++i) {
        char in = text[i];
        std::vector<int> nxt;

        auto cacheRow = cache.find(cur);
        if (cacheRow != cache.end()) {
            auto hit = cacheRow->second.find(in);
            if (hit != cacheRow->second.end()) {
                nxt = hit->second;
            }
        }

        if (nxt.empty()) {
            nxt = epsClosure(move(cur, in));
            cache[cur][in] = nxt;
        }

        if (nxt.empty()) {
            break;
        }

        cur = nxt;

        TokenType candType = TokenType::ENDMARKER;
        int candPrio = 0;
        if (bestAccept(cur, candType, candPrio)) {
            ok = true;
            type = candType;
            len = (i - start) + 1;
        }
    }

    return ok;
}

std::string AutomataEngine::withConcat(const std::string& regex) {
    std::vector<RTok> ts = regexTokens(regex);
    std::string out;

    auto isOperand = [](const RTok& t) {
        return t.kind == RTokKind::Lit || t.kind == RTokKind::RPar || t.kind == RTokKind::Star ||
               t.kind == RTokKind::Plus || t.kind == RTokKind::Opt;
    };
    auto startsOperand = [](const RTok& t) {
        return t.kind == RTokKind::Lit || t.kind == RTokKind::LPar;
    };

    for (std::size_t i = 0; i < ts.size(); ++i) {
        const RTok& t = ts[i];

        if (t.kind == RTokKind::Lit) {
            out += esc(t.lit);
        } else if (t.kind == RTokKind::Uni) {
            out += "|";
        } else if (t.kind == RTokKind::Cat) {
            out += ".";
        } else if (t.kind == RTokKind::Star) {
            out += "*";
        } else if (t.kind == RTokKind::Plus) {
            out += "+";
        } else if (t.kind == RTokKind::Opt) {
            out += "?";
        } else if (t.kind == RTokKind::LPar) {
            out += "(";
        } else if (t.kind == RTokKind::RPar) {
            out += ")";
        }

        if (i + 1 < ts.size() && isOperand(t) && startsOperand(ts[i + 1])) {
            out += ".";
        }
    }

    return out;
}

std::vector<AutomataEngine::RTok> AutomataEngine::regexTokens(const std::string& regex) {
    std::vector<RTok> out;

    for (std::size_t i = 0; i < regex.size(); ++i) {
        char c = regex[i];
        if (c == '\\') {
            if (i + 1 >= regex.size()) {
                throw std::runtime_error("Invalid regex escape at end");
            }
            out.push_back(RTok{RTokKind::Lit, regex[i + 1]});
            ++i;
            continue;
        }

        if (c == '|') {
            out.push_back(RTok{RTokKind::Uni, '\0'});
        } else if (c == '.') {
            out.push_back(RTok{RTokKind::Cat, '\0'});
        } else if (c == '*') {
            out.push_back(RTok{RTokKind::Star, '\0'});
        } else if (c == '+') {
            out.push_back(RTok{RTokKind::Plus, '\0'});
        } else if (c == '?') {
            out.push_back(RTok{RTokKind::Opt, '\0'});
        } else if (c == '(') {
            out.push_back(RTok{RTokKind::LPar, '\0'});
        } else if (c == ')') {
            out.push_back(RTok{RTokKind::RPar, '\0'});
        } else {
            out.push_back(RTok{RTokKind::Lit, c});
        }
    }

    return out;
}

std::vector<AutomataEngine::RTok> AutomataEngine::postfix(const std::vector<RTok>& tokens) {
    std::vector<RTok> out;
    std::vector<RTok> ops;

    auto prio = [](RTokKind k) {
        if (k == RTokKind::Uni) {
            return 1;
        }
        if (k == RTokKind::Cat) {
            return 2;
        }
        return 0;
    };

    for (const RTok& t : tokens) {
        if (t.kind == RTokKind::Lit) {
            out.push_back(t);
            continue;
        }

        if (t.kind == RTokKind::LPar) {
            ops.push_back(t);
            continue;
        }

        if (t.kind == RTokKind::RPar) {
            bool found = false;
            while (!ops.empty()) {
                RTok top = ops.back();
                ops.pop_back();
                if (top.kind == RTokKind::LPar) {
                    found = true;
                    break;
                }
                out.push_back(top);
            }
            if (!found) {
                throw std::runtime_error("Mismatched parentheses in regex");
            }
            continue;
        }

        if (t.kind == RTokKind::Star || t.kind == RTokKind::Plus || t.kind == RTokKind::Opt) {
            out.push_back(t);
            continue;
        }

        while (!ops.empty()) {
            const RTok& top = ops.back();
            if (top.kind == RTokKind::LPar || prio(top.kind) < prio(t.kind)) {
                break;
            }
            out.push_back(top);
            ops.pop_back();
        }
        ops.push_back(t);
    }

    while (!ops.empty()) {
        RTok top = ops.back();
        ops.pop_back();
        if (top.kind == RTokKind::LPar || top.kind == RTokKind::RPar) {
            throw std::runtime_error("Mismatched parentheses in regex");
        }
        out.push_back(top);
    }

    return out;
}

AutomataEngine::Fragment AutomataEngine::buildNfa(
    const std::vector<RTok>& post,
    int prio,
    TokenType type
) {
    std::vector<Fragment> st;

    auto newState = [this]() {
        return nfa_.nextId++;
    };

    for (const RTok& t : post) {
        if (t.kind == RTokKind::Lit) {
            int s = newState();
            int a = newState();
            nfa_.charEdges[s].push_back(std::make_pair(a, t.lit));
            st.push_back(Fragment{s, a});
            continue;
        }

        if (t.kind == RTokKind::Cat) {
            if (st.size() < 2) {
                throw std::runtime_error("Invalid regex: concat needs two operands");
            }
            Fragment r = st.back();
            st.pop_back();
            Fragment l = st.back();
            st.pop_back();
            nfa_.epsEdges[l.accept].push_back(r.start);
            st.push_back(Fragment{l.start, r.accept});
            continue;
        }

        if (t.kind == RTokKind::Uni) {
            if (st.size() < 2) {
                throw std::runtime_error("Invalid regex: union needs two operands");
            }
            Fragment r = st.back();
            st.pop_back();
            Fragment l = st.back();
            st.pop_back();

            int s = newState();
            int a = newState();
            nfa_.epsEdges[s].push_back(l.start);
            nfa_.epsEdges[s].push_back(r.start);
            nfa_.epsEdges[l.accept].push_back(a);
            nfa_.epsEdges[r.accept].push_back(a);
            st.push_back(Fragment{s, a});
            continue;
        }

        if (t.kind == RTokKind::Star) {
            if (st.empty()) {
                throw std::runtime_error("Invalid regex: star needs one operand");
            }
            Fragment f = st.back();
            st.pop_back();

            int s = newState();
            int a = newState();
            nfa_.epsEdges[s].push_back(f.start);
            nfa_.epsEdges[s].push_back(a);
            nfa_.epsEdges[f.accept].push_back(f.start);
            nfa_.epsEdges[f.accept].push_back(a);
            st.push_back(Fragment{s, a});
            continue;
        }

        if (t.kind == RTokKind::Plus) {
            if (st.empty()) {
                throw std::runtime_error("Invalid regex: plus needs one operand");
            }
            Fragment f = st.back();
            st.pop_back();

            int s = newState();
            int a = newState();
            nfa_.epsEdges[s].push_back(f.start);
            nfa_.epsEdges[f.accept].push_back(f.start);
            nfa_.epsEdges[f.accept].push_back(a);
            st.push_back(Fragment{s, a});
            continue;
        }

        if (t.kind == RTokKind::Opt) {
            if (st.empty()) {
                throw std::runtime_error("Invalid regex: optional needs one operand");
            }
            Fragment f = st.back();
            st.pop_back();

            int s = newState();
            int a = newState();
            nfa_.epsEdges[s].push_back(f.start);
            nfa_.epsEdges[s].push_back(a);
            nfa_.epsEdges[f.accept].push_back(a);
            st.push_back(Fragment{s, a});
            continue;
        }
    }

    if (st.size() != 1) {
        throw std::runtime_error("Invalid regex: unresolved fragments");
    }

    Fragment out = st.back();
    nfa_.acceptInfo[out.accept] = std::make_pair(type, prio);
    return out;
}

std::vector<int> AutomataEngine::normalize(const std::vector<int>& states) {
    std::vector<int> out = states;
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::vector<int> AutomataEngine::epsClosure(const std::vector<int>& states) const {
    std::vector<int> out = normalize(states);
    std::vector<int> st = out;

    while (!st.empty()) {
        int s = st.back();
        st.pop_back();

        auto it = nfa_.epsEdges.find(s);
        if (it == nfa_.epsEdges.end()) {
            continue;
        }

        for (int nxt : it->second) {
            if (!std::binary_search(out.begin(), out.end(), nxt)) {
                out.insert(std::upper_bound(out.begin(), out.end(), nxt), nxt);
                st.push_back(nxt);
            }
        }
    }

    return out;
}

std::vector<int> AutomataEngine::move(const std::vector<int>& states, char in) const {
    std::vector<int> out;

    for (int s : states) {
        auto it = nfa_.charEdges.find(s);
        if (it == nfa_.charEdges.end()) {
            continue;
        }
        for (const auto& edge : it->second) {
            if (edge.second == in) {
                out.push_back(edge.first);
            }
        }
    }

    return normalize(out);
}

bool AutomataEngine::bestAccept(const std::vector<int>& states, TokenType& type, int& prio) const {
    bool found = false;
    int bestPrio = 0;

    for (int s : states) {
        auto it = nfa_.acceptInfo.find(s);
        if (it == nfa_.acceptInfo.end()) {
            continue;
        }

        int p = it->second.second;
        if (!found || p < bestPrio) {
            found = true;
            bestPrio = p;
            type = it->second.first;
        }
    }

    if (found) {
        prio = bestPrio;
    }

    return found;
}

}  // namespace cd

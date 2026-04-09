#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "compiler/tokens.hpp"

namespace cd {

struct TokenPattern {
    TokenType tokenType;
    std::string regex;
    int priority;
};

class AutomataEngine {
public:
    explicit AutomataEngine(const std::vector<TokenPattern>& patterns);

    bool longestMatch(
        const std::string& text,
        std::size_t start,
        std::size_t& len,
        TokenType& type
    ) const;

private:
    struct StateSetHash {
        std::size_t operator()(const std::vector<int>& set) const;
    };

    struct Nfa {
        int start = -1;
        int nextId = 0;
        std::unordered_map<int, std::vector<std::pair<int, char>>> charEdges;
        std::unordered_map<int, std::vector<int>> epsEdges;
        std::unordered_map<int, std::pair<TokenType, int>> acceptInfo;
    };

    struct Fragment {
        int start = -1;
        int accept = -1;
    };

    enum class RTokKind {
        Lit,
        Uni,
        Cat,
        Star,
        Plus,
        Opt,
        LPar,
        RPar
    };

    struct RTok {
        RTokKind kind;
        char lit = '\0';
    };

    Nfa nfa_;

    static std::string withConcat(const std::string& regex);
    static std::vector<RTok> regexTokens(const std::string& regex);
    static std::vector<RTok> postfix(const std::vector<RTok>& tokens);

    Fragment buildNfa(const std::vector<RTok>& post, int prio, TokenType type);

    std::vector<int> epsClosure(const std::vector<int>& states) const;
    std::vector<int> move(const std::vector<int>& states, char in) const;
    static std::vector<int> normalize(const std::vector<int>& states);

    bool bestAccept(const std::vector<int>& states, TokenType& type, int& prio) const;
};

std::vector<TokenPattern> defaultPatterns();

}  // namespace cd

#include "compiler/io_utils.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

namespace cd
{
    namespace
    {

        std::string escapeXML(const std::string &text)
        {
            std::string out;
            out.reserve(text.size());
            for (char c : text)
            {
                switch (c)
                {
                case '&':
                    out += "&amp;";
                    break;
                case '<':
                    out += "&lt;";
                    break;
                case '>':
                    out += "&gt;";
                    break;
                case '"':
                    out += "&quot;";
                    break;
                case '\'':
                    out += "&apos;";
                    break;
                default:
                    out += c;
                    break;
                }
            }
            return out;
        }

        std::string escapeDotLabel(const std::string &text)
        {
            std::string out;
            out.reserve(text.size());
            for (char c : text)
            {
                if (c == '\\' || c == '"')
                    out.push_back('\\');
                if (c == '\n')
                {
                    out += "\\n";
                }
                else
                {
                    out.push_back(c);
                }
            }
            return out;
        }

        std::string displaySymbol(const std::string &symbol)
        {
            static const std::unordered_map<std::string, std::string> pretty = {
                {"ASSIGN", "="},
                {"PLUS_ASSIGN", "+="},
                {"MINUS_ASSIGN", "-="},
                {"PLUS", "+"},
                {"MINUS", "-"},
                {"STAR", "*"},
                {"SLASH", "/"},
                {"LPAREN", "("},
                {"RPAREN", ")"},
                {"LBRACKET", "["},
                {"RBRACKET", "]"},
                {"COMMA", ","},
                {"COLON", ":"},
                {"SEMICOLON", ";"},
                {"NEWLINE", "\\n"},
                {"INDENT", "<INDENT>"},
                {"DEDENT", "<DEDENT>"},
                {"EOF", "$"},
                {"PRINT", "print"},
                {"FOR", "for"},
                {"IN", "in"},
                {"IF", "if"},
                {"ELSE", "else"},
                {"RANGE", "range"},
                {EPSILON, "ε"},
            };
            auto it = pretty.find(symbol);
            if (it != pretty.end())
                return it->second;
            return symbol;
        }

        std::string joinSet(const std::unordered_set<std::string> &values)
        {
            std::set<std::string> sorted(values.begin(), values.end());
            std::ostringstream out;
            out << "{ ";
            std::size_t i = 0;
            for (const auto &item : sorted)
            {
                out << displaySymbol(item);
                if (++i < sorted.size())
                    out << ", ";
            }
            out << " }";
            return out.str();
        }

        std::string joinProduction(const std::vector<std::string> &production)
        {
            std::ostringstream line;
            for (std::size_t i = 0; i < production.size(); ++i)
            {
                line << displaySymbol(production[i]);
                if (i + 1 < production.size())
                    line << " ";
            }
            return line.str();
        }

        std::vector<std::string> splitLines(const std::string &source)
        {
            std::vector<std::string> lines;
            std::istringstream in(source);
            std::string line;
            while (std::getline(in, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                lines.push_back(line);
            }
            if (lines.empty())
                lines.push_back("");
            return lines;
        }

        void writeSVGTable(const std::string &path, const std::string &title,
                           const std::vector<std::string> &headers,
                           const std::vector<std::vector<std::string>> &rows)
        {
            constexpr int padding = 10;
            constexpr int rowHeight = 28;
            constexpr int titleHeight = 44;
            constexpr int fontSize = 13;
            constexpr int charWidth = 8;

            std::vector<int> colWidths(headers.size(), 120);
            for (std::size_t c = 0; c < headers.size(); ++c)
            {
                colWidths[c] = std::max(colWidths[c], static_cast<int>(headers[c].size() * charWidth + 2 * padding));
            }
            for (const auto &row : rows)
            {
                for (std::size_t c = 0; c < row.size() && c < colWidths.size(); ++c)
                {
                    int needed = static_cast<int>(row[c].size() * charWidth + 2 * padding);
                    if (needed > colWidths[c])
                        colWidths[c] = needed;
                }
            }

            int totalWidth = 0;
            for (int w : colWidths)
                totalWidth += w;
            int totalHeight = titleHeight + static_cast<int>((rows.size() + 1) * rowHeight) + 20;

            std::ofstream out(path);
            if (!out)
                throw std::runtime_error("Failed to write SVG file: " + path);

            out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << totalWidth + 20 << "\" height=\"" << totalHeight
                << "\" viewBox=\"0 0 " << totalWidth + 20 << " " << totalHeight << "\">\n";
            out << "  <rect x=\"0\" y=\"0\" width=\"100%\" height=\"100%\" fill=\"#f8fafc\"/>\n";
            out << "  <text x=\"10\" y=\"28\" font-family=\"Consolas, monospace\" font-size=\"18\" font-weight=\"700\" "
                   "fill=\"#0f172a\">"
                << escapeXML(title) << "</text>\n";

            int y = titleHeight;
            int x = 10;
            for (std::size_t c = 0; c < headers.size(); ++c)
            {
                out << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << colWidths[c] << "\" height=\"" << rowHeight
                    << "\" fill=\"#dbeafe\" stroke=\"#94a3b8\"/>\n";
                out << "  <text x=\"" << x + padding << "\" y=\"" << y + 18
                    << "\" font-family=\"Consolas, monospace\" font-size=\"" << fontSize
                    << "\" font-weight=\"700\" fill=\"#0f172a\">" << escapeXML(headers[c]) << "</text>\n";
                x += colWidths[c];
            }

            for (std::size_t r = 0; r < rows.size(); ++r)
            {
                y = titleHeight + static_cast<int>((r + 1) * rowHeight);
                x = 10;
                const std::string fill = (r % 2 == 0) ? "#ffffff" : "#f1f5f9";
                for (std::size_t c = 0; c < headers.size(); ++c)
                {
                    std::string cell = (c < rows[r].size()) ? rows[r][c] : "";
                    out << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << colWidths[c] << "\" height=\"" << rowHeight
                        << "\" fill=\"" << fill << "\" stroke=\"#cbd5e1\"/>\n";
                    out << "  <text x=\"" << x + padding << "\" y=\"" << y + 18
                        << "\" font-family=\"Consolas, monospace\" font-size=\"" << fontSize
                        << "\" fill=\"#1e293b\">" << escapeXML(cell) << "</text>\n";
                    x += colWidths[c];
                }
            }

            out << "</svg>\n";
        }

        class ASTDotBuilder
        {
        public:
            ASTDotBuilder() = default;

            std::string build(const Program &program)
            {
                out_ << "digraph AST {\n";
                out_ << "  rankdir=TB;\n";
                out_ << "  node [shape=box, style=rounded, fontname=\"Consolas\"];\n";

                int programId = emitNode("Program");
                for (const auto &stmt : program.statements)
                {
                    int child = visitStatement(*stmt);
                    emitEdge(programId, child);
                }

                out_ << "}\n";
                return out_.str();
            }

        private:
            std::ostringstream out_;
            int nextId_{0};

            int emitNode(const std::string &label)
            {
                int id = nextId_++;
                out_ << "  n" << id << " [label=\"" << escapeDotLabel(label) << "\"];\n";
                return id;
            }

            void emitEdge(int from, int to) { out_ << "  n" << from << " -> n" << to << ";\n"; }

            int visitStatement(const Statement &stmt)
            {
                if (auto p = dynamic_cast<const AssignmentStatement *>(&stmt))
                    return visitAssignment(*p);
                if (auto p = dynamic_cast<const PrintStatement *>(&stmt))
                    return visitPrint(*p);
                if (auto p = dynamic_cast<const ForStatement *>(&stmt))
                    return visitFor(*p);
                if (auto p = dynamic_cast<const IfStatement *>(&stmt))
                    return visitIf(*p);
                if (auto p = dynamic_cast<const BlockStatement *>(&stmt))
                    return visitBlock(*p);
                return emitNode("UnknownStmt");
            }

            int visitExpression(const Expression &expr)
            {
                if (auto p = dynamic_cast<const IntLiteral *>(&expr))
                    return emitNode("Int: " + std::to_string(p->value));
                if (auto p = dynamic_cast<const FloatLiteral *>(&expr))
                    return emitNode("Float: " + std::to_string(p->value));
                if (auto p = dynamic_cast<const StringLiteral *>(&expr))
                    return emitNode("String: \"" + p->value + "\"");
                if (auto p = dynamic_cast<const BoolLiteral *>(&expr))
                    return emitNode(std::string("Bool: ") + (p->value ? "true" : "false"));
                if (auto p = dynamic_cast<const Identifier *>(&expr))
                    return emitNode("Id: " + p->name);
                if (auto p = dynamic_cast<const ListLiteral *>(&expr))
                    return visitList(*p);
                if (auto p = dynamic_cast<const RangeExpression *>(&expr))
                    return visitRange(*p);
                if (auto p = dynamic_cast<const UnaryExpression *>(&expr))
                    return visitUnary(*p);
                if (auto p = dynamic_cast<const BinaryExpression *>(&expr))
                    return visitBinary(*p);
                return emitNode("UnknownExpr");
            }

            int visitAssignment(const AssignmentStatement &stmt)
            {
                int root = emitNode("Assign (" + stmt.op + ")");
                int idNode = emitNode("Id: " + stmt.name);
                int exprNode = visitExpression(*stmt.expression);
                emitEdge(root, idNode);
                emitEdge(root, exprNode);
                return root;
            }

            int visitPrint(const PrintStatement &stmt)
            {
                int root = emitNode("Print");
                emitEdge(root, visitExpression(*stmt.expression));
                return root;
            }

            int visitFor(const ForStatement &stmt)
            {
                int root = emitNode("For");
                int varNode = emitNode("LoopVar: " + stmt.loopVar);
                int iterNode = visitExpression(*stmt.iterable);
                int bodyNode = visitStatement(*stmt.body);
                emitEdge(root, varNode);
                emitEdge(root, iterNode);
                emitEdge(root, bodyNode);
                return root;
            }

            int visitIf(const IfStatement &stmt)
            {
                int root = emitNode("If");
                int condNode = visitExpression(*stmt.condition);
                int thenNode = visitStatement(*stmt.thenBranch);
                emitEdge(root, condNode);
                emitEdge(root, thenNode);
                if (stmt.elseBranch)
                {
                    int elseNode = emitNode("Else");
                    emitEdge(root, elseNode);
                    emitEdge(elseNode, visitStatement(*stmt.elseBranch));
                }
                return root;
            }

            int visitBlock(const BlockStatement &stmt)
            {
                int root = emitNode("Block");
                for (const auto &bodyStmt : stmt.statements)
                {
                    emitEdge(root, visitStatement(*bodyStmt));
                }
                return root;
            }

            int visitUnary(const UnaryExpression &expr)
            {
                int root = emitNode("Unary (" + expr.op + ")");
                emitEdge(root, visitExpression(*expr.operand));
                return root;
            }

            int visitBinary(const BinaryExpression &expr)
            {
                int root = emitNode("Binary (" + expr.op + ")");
                emitEdge(root, visitExpression(*expr.left));
                emitEdge(root, visitExpression(*expr.right));
                return root;
            }

            int visitList(const ListLiteral &expr)
            {
                int root = emitNode("List");
                for (const auto &element : expr.elements)
                {
                    emitEdge(root, visitExpression(*element));
                }
                return root;
            }

            int visitRange(const RangeExpression &expr)
            {
                int root = emitNode("Range");
                emitEdge(root, visitExpression(*expr.start));
                emitEdge(root, visitExpression(*expr.end));
                if (expr.step)
                    emitEdge(root, visitExpression(*expr.step));
                return root;
            }
        };

    } // namespace

    std::string ProgramReader::read(const std::string &path) const
    {
        std::ifstream in(path);
        if (!in)
            throw std::runtime_error("Failed to open source file: " + path);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    void OutputWriter::write(const std::string &text) const { std::cout << text << "\n"; }

    bool OutputWriter::extractLineColumn(const std::string &message, int &line, int &column)
    {
        std::regex rgx("line\\s+([0-9]+),\\s*column\\s+([0-9]+)");
        std::smatch match;
        if (!std::regex_search(message, match, rgx) || match.size() < 3)
            return false;
        line = std::stoi(match[1]);
        column = std::stoi(match[2]);
        return true;
    }

    void OutputWriter::writeDiagnosticWithSnippet(const std::string &kind,
                                                  const std::string &message,
                                                  const std::string &source,
                                                  int line,
                                                  int column) const
    {
        std::cerr << kind << ": " << message << "\n";
        if (line <= 0 || column <= 0)
            return;

        const auto lines = splitLines(source);
        if (line > static_cast<int>(lines.size()))
            return;

        const std::string &srcLine = lines[static_cast<std::size_t>(line - 1)];
        std::cerr << "  " << line << " | " << srcLine << "\n";
        std::cerr << "    | ";
        for (int i = 1; i < column; ++i)
            std::cerr << ' ';
        std::cerr << "^\n";
    }

    void OutputWriter::writeParseTableReport(const std::string &path, const GrammarArtifacts &artifacts) const
    {
        std::filesystem::path outPath(path);
        if (outPath.has_parent_path())
        {
            std::filesystem::create_directories(outPath.parent_path());
        }

        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("Failed to write report file: " + path);

        out << "Parse Table Report\n";
        out << "==================\n\n";

        out << "Conflicts: " << artifacts.conflicts.size() << "\n";
        for (const auto &conflict : artifacts.conflicts)
        {
            out << "- " << conflict << "\n";
        }
        out << "\n";

        out << "FIRST Sets\n";
        out << "----------\n";
        std::map<std::string, std::unordered_set<std::string>> sortedFirst(artifacts.first.begin(), artifacts.first.end());
        for (const auto &[nonTerminal, setValues] : sortedFirst)
        {
            out << nonTerminal << " = { ";
            std::set<std::string> ordered(setValues.begin(), setValues.end());
            std::size_t k = 0;
            for (const auto &item : ordered)
            {
                out << item;
                if (++k < ordered.size())
                    out << ", ";
            }
            out << " }\n";
        }
        out << "\n";

        out << "FOLLOW Sets\n";
        out << "-----------\n";
        std::map<std::string, std::unordered_set<std::string>> sortedFollow(artifacts.follow.begin(), artifacts.follow.end());
        for (const auto &[nonTerminal, setValues] : sortedFollow)
        {
            out << nonTerminal << " = { ";
            std::set<std::string> ordered(setValues.begin(), setValues.end());
            std::size_t k = 0;
            for (const auto &item : ordered)
            {
                out << item;
                if (++k < ordered.size())
                    out << ", ";
            }
            out << " }\n";
        }
        out << "\n";

        out << "LL(1) Validation\n";
        out << "---------------\n";
        out << "Valid LL(1): " << (artifacts.ll1Validation.valid ? "yes" : "no") << "\n";
        out << "FIRST/FIRST conflicts: " << artifacts.ll1Validation.firstFirstConflicts.size() << "\n";
        for (const auto &conflict : artifacts.ll1Validation.firstFirstConflicts)
        {
            out << "  - " << conflict << "\n";
        }
        out << "FIRST/FOLLOW conflicts: " << artifacts.ll1Validation.firstFollowConflicts.size() << "\n";
        for (const auto &conflict : artifacts.ll1Validation.firstFollowConflicts)
        {
            out << "  - " << conflict << "\n";
        }
        out << "\n";

        out << "LL(1) Parse Table\n";
        out << "-----------------\n";
        std::map<std::string, std::unordered_map<std::string, std::vector<std::string>>> sortedTable(
            artifacts.parseTable.begin(), artifacts.parseTable.end());
        for (const auto &[nonTerminal, row] : sortedTable)
        {
            out << nonTerminal << ":\n";
            std::map<std::string, std::vector<std::string>> orderedRow(row.begin(), row.end());
            for (const auto &[terminal, production] : orderedRow)
            {
                out << "  M[" << nonTerminal << ", " << terminal << "] = " << joinProduction(production) << "\n";
            }
        }

        out << "\n";
        out << "LL(1) Panic-Mode Recovery (Pop/Scan)\n";
        out << "------------------------------------\n";
        out << "Accepted without recovery errors: " << (artifacts.ll1PanicParse.accepted ? "yes" : "no") << "\n";
        out << "Recovered errors: " << artifacts.ll1PanicParse.errorsRecovered << "\n";
        std::size_t popTerminal = 0;
        std::size_t popNonTerminal = 0;
        std::size_t scanDiscard = 0;
        for (const auto &action : artifacts.ll1PanicParse.actionLog)
        {
            if (action.find("Panic pop: terminal mismatch") == 0)
                ++popTerminal;
            else if (action.find("Panic pop: pop non-terminal") == 0)
                ++popNonTerminal;
            else if (action.find("Panic scan:") == 0)
                ++scanDiscard;
        }
        out << "Category summary:\n";
        out << "  - Panic pop (terminal mismatch): " << popTerminal << "\n";
        out << "  - Panic pop (sync non-terminal): " << popNonTerminal << "\n";
        out << "  - Panic scan (discard token): " << scanDiscard << "\n";
        out << "Actions:\n";
        if (artifacts.ll1PanicParse.actionLog.empty())
        {
            out << "  (no actions logged)\n";
        }
        else
        {
            for (const auto &action : artifacts.ll1PanicParse.actionLog)
            {
                out << "  - " << action << "\n";
            }
        }
    }

    void OutputWriter::writeGrammarImages(const std::string &outputDir, const GrammarArtifacts &artifacts) const
    {
        std::filesystem::create_directories(outputDir);

        std::map<std::string, std::unordered_set<std::string>> sortedFirst(artifacts.first.begin(), artifacts.first.end());
        std::map<std::string, std::unordered_set<std::string>> sortedFollow(artifacts.follow.begin(),
                                                                            artifacts.follow.end());

        std::vector<std::vector<std::string>> firstRows;
        for (const auto &[nt, setValues] : sortedFirst)
        {
            firstRows.push_back({nt, joinSet(setValues)});
        }
        writeSVGTable(outputDir + "/first_set.svg", "FIRST Set Table", {"Non-Terminal", "FIRST"}, firstRows);

        std::vector<std::vector<std::string>> followRows;
        for (const auto &[nt, setValues] : sortedFollow)
        {
            followRows.push_back({nt, joinSet(setValues)});
        }
        writeSVGTable(outputDir + "/follow_set.svg", "FOLLOW Set Table", {"Non-Terminal", "FOLLOW"}, followRows);

        std::set<std::string> terminals;
        for (const auto &[_, row] : artifacts.parseTable)
        {
            for (const auto &[terminal, __] : row)
                terminals.insert(terminal);
        }

        std::vector<std::string> headers;
        headers.push_back("Non-Terminal");
        for (const auto &terminal : terminals)
        {
            headers.push_back(displaySymbol(terminal));
        }

        std::map<std::string, std::unordered_map<std::string, std::vector<std::string>>> sortedTable(
            artifacts.parseTable.begin(), artifacts.parseTable.end());
        std::vector<std::vector<std::string>> parseRows;
        for (const auto &[nt, row] : sortedTable)
        {
            std::vector<std::string> cells;
            cells.push_back(nt);
            for (const auto &terminal : terminals)
            {
                auto it = row.find(terminal);
                cells.push_back(it == row.end() ? "" : joinProduction(it->second));
            }
            parseRows.push_back(std::move(cells));
        }
        writeSVGTable(outputDir + "/parse_table.svg", "LL(1) Parse Table", headers, parseRows);
        writePanicRecoveryImage(outputDir + "/panic_recovery.svg", artifacts.ll1PanicParse);
    }

    void OutputWriter::writePanicRecoveryImage(const std::string &path, const LL1PanicParseResult &panicResult) const
    {
        std::vector<std::vector<std::string>> rows;
        rows.push_back({"Accepted without recovery errors", panicResult.accepted ? "yes" : "no"});
        rows.push_back({"Recovered errors", std::to_string(panicResult.errorsRecovered)});

        std::size_t popTerminal = 0;
        std::size_t popNonTerminal = 0;
        std::size_t scanDiscard = 0;
        for (const auto &action : panicResult.actionLog)
        {
            if (action.find("Panic pop: terminal mismatch") == 0)
                ++popTerminal;
            else if (action.find("Panic pop: pop non-terminal") == 0)
                ++popNonTerminal;
            else if (action.find("Panic scan:") == 0)
                ++scanDiscard;
        }
        rows.push_back({"Panic pop (terminal mismatch)", std::to_string(popTerminal)});
        rows.push_back({"Panic pop (sync non-terminal)", std::to_string(popNonTerminal)});
        rows.push_back({"Panic scan (discard token)", std::to_string(scanDiscard)});

        if (panicResult.actionLog.empty())
        {
            rows.push_back({"Action", "(no actions logged)"});
        }
        else
        {
            for (std::size_t i = 0; i < panicResult.actionLog.size(); ++i)
            {
                rows.push_back({std::to_string(i + 1), panicResult.actionLog[i]});
            }
        }

        writeSVGTable(path, "LL(1) Panic-Mode Recovery (Pop/Scan)", {"Step", "Action"}, rows);
    }

    void OutputWriter::writeGrammarRulesImage(const std::string &path, const CFGDefinition &cfg) const
    {
        std::filesystem::path outPath(path);
        if (outPath.has_parent_path())
        {
            std::filesystem::create_directories(outPath.parent_path());
        }

        std::map<std::string, std::vector<std::vector<std::string>>> sorted(cfg.productions.begin(), cfg.productions.end());
        std::vector<std::vector<std::string>> rows;
        rows.reserve(sorted.size());
        for (const auto &[nt, productions] : sorted)
        {
            std::ostringstream rhs;
            for (std::size_t i = 0; i < productions.size(); ++i)
            {
                rhs << joinProduction(productions[i]);
                if (i + 1 < productions.size())
                    rhs << " | ";
            }
            rows.push_back({nt, rhs.str()});
        }

        writeSVGTable(path, "Grammar Rules (Raw CFG, Start=" + cfg.startSymbol + ")", {"Non-Terminal", "Production"}, rows);
    }

    void OutputWriter::writeTokenStreamReport(const std::string &path, const std::vector<Token> &tokens) const
    {
        std::filesystem::path outPath(path);
        if (outPath.has_parent_path())
        {
            std::filesystem::create_directories(outPath.parent_path());
        }

        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("Failed to write token report: " + path);
        out << "Token Stream\n";
        out << "============\n\n";
        out << std::left << std::setw(16) << "Type" << std::setw(16) << "Lexeme" << std::setw(8) << "Line"
            << std::setw(8) << "Col" << "\n";
        out << std::string(48, '-') << "\n";

        for (const auto &token : tokens)
        {
            out << std::left << std::setw(16) << tokenTypeName(token.type) << std::setw(16) << token.lexeme << std::setw(8)
                << token.line << std::setw(8) << token.column << "\n";
        }
    }

    void OutputWriter::writeSymbolTableReport(
        const std::string &path, const std::unordered_map<std::string, SymbolRecord> &symbolSnapshot) const
    {
        std::filesystem::path outPath(path);
        if (outPath.has_parent_path())
        {
            std::filesystem::create_directories(outPath.parent_path());
        }

        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("Failed to write symbol table report: " + path);
        out << "Symbol Table\n";
        out << "============\n\n";
        out << std::left << std::setw(20) << "Name" << std::setw(12) << "Type" << "Value\n";
        out << std::string(72, '-') << "\n";

        std::map<std::string, SymbolRecord> sorted(symbolSnapshot.begin(), symbolSnapshot.end());
        for (const auto &[name, record] : sorted)
        {
            out << std::left << std::setw(20) << name << std::setw(12) << record.varType << record.valueRepr << "\n";
        }
    }

    void OutputWriter::writeTokenStreamImage(const std::string &path, const std::vector<Token> &tokens) const
    {
        std::vector<std::vector<std::string>> rows;
        rows.reserve(tokens.size());
        for (const auto &token : tokens)
        {
            rows.push_back({tokenTypeName(token.type), token.lexeme, std::to_string(token.line), std::to_string(token.column)});
        }
        writeSVGTable(path, "Token Stream", {"Type", "Lexeme", "Line", "Column"}, rows);
    }

    void OutputWriter::writeSymbolTableImage(
        const std::string &path, const std::unordered_map<std::string, SymbolRecord> &symbolSnapshot) const
    {
        std::map<std::string, SymbolRecord> sorted(symbolSnapshot.begin(), symbolSnapshot.end());
        std::vector<std::vector<std::string>> rows;
        rows.reserve(sorted.size());
        for (const auto &[name, record] : sorted)
        {
            rows.push_back({name, record.varType, record.valueRepr});
        }
        writeSVGTable(path, "Symbol Table", {"Name", "Type", "Value"}, rows);
    }

    void OutputWriter::writeASTDot(const std::string &path, const Program &program) const
    {
        std::filesystem::path outPath(path);
        if (outPath.has_parent_path())
        {
            std::filesystem::create_directories(outPath.parent_path());
        }
        ASTDotBuilder builder;
        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("Failed to write AST dot file: " + path);
        out << builder.build(program);
    }

    bool OutputWriter::renderDotToSvg(const std::string &dotPath, const std::string &svgPath) const
    {
        std::filesystem::path outPath(svgPath);
        if (outPath.has_parent_path())
        {
            std::filesystem::create_directories(outPath.parent_path());
        }
        std::string command = "dot -Tsvg \"" + dotPath + "\" -o \"" + svgPath + "\"";
        int code = std::system(command.c_str());
        return code == 0;
    }

} // namespace cd
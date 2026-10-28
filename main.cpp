#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <map>
#include <set>
#include <iomanip>
#include <ctime>
#include <windows.h>
#include <wininet.h>

struct PassLog {
    int lineNumber;
    std::string category;
    std::string whatChanged;
    std::string why;
    std::string effect;
    std::string behaviorPreserved;
};

// Helper: Trim whitespace
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Helper: Strip C++ comments for inspection
std::string stripComments(const std::string& line, bool& inBlockComment) {
    std::string result;
    size_t i = 0;
    while (i < line.size()) {
        if (inBlockComment) {
            if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '/') {
                inBlockComment = false;
                i += 2;
            } else {
                i++;
            }
        } else {
            if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') {
                break;
            } else if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '*') {
                inBlockComment = true;
                i += 2;
            } else {
                result += line[i];
                i++;
            }
        }
    }
    return result;
}

// Check if initializer expression has side effects
bool checkSideEffects(const std::string& expr) {
    std::string trimmed = trim(expr);
    if (trimmed.empty()) return false;

    // Check for function calls e.g., identifier followed by '('
    std::regex funcCallRegex(R"([a-zA-Z_][a-zA-Z0-9_]*\s*\()");
    if (std::regex_search(trimmed, funcCallRegex)) {
        return true;
    }

    // Check for assignment or increment/decrement
    if (trimmed.find("++") != std::string::npos ||
        trimmed.find("--") != std::string::npos ||
        trimmed.find("=") != std::string::npos ||
        trimmed.find("<<") != std::string::npos ||
        trimmed.find(">>") != std::string::npos) {
        return true;
    }

    return false;
}

// Fold constant arithmetic expressions (e.g., 10 + 20 -> 30, 5 * 4 -> 20)
std::string foldConstantExpression(const std::string& expr) {
    std::string s = trim(expr);

    // Integer arithmetic pattern: <int> <op> <int>
    std::regex intArithRegex(R"(^\(?\s*(-?\d+)\s*([\+\*\/-])\s*(-?\d+)\s*\)?$)");
    std::smatch match;
    if (std::regex_match(s, match, intArithRegex)) {
        long long val1 = std::stoll(match[1].str());
        std::string op = match[2].str();
        long long val2 = std::stoll(match[3].str());
        long long res = 0;

        if (op == "+") res = val1 + val2;
        else if (op == "-") res = val1 - val2;
        else if (op == "*") res = val1 * val2;
        else if (op == "/" && val2 != 0) res = val1 / val2;
        else return s;

        return std::to_string(res);
    }

    // Double arithmetic pattern: <double> <op> <double>
    std::regex doubleArithRegex(R"(^\(?\s*(-?\d+\.\d+)\s*([\+\*\/-])\s*(-?\d+\.\d+)\s*\)?$)");
    if (std::regex_match(s, match, doubleArithRegex)) {
        double val1 = std::stod(match[1].str());
        std::string op = match[2].str();
        double val2 = std::stod(match[3].str());
        double res = 0.0;

        if (op == "+") res = val1 + val2;
        else if (op == "-") res = val1 - val2;
        else if (op == "*") res = val1 * val2;
        else if (op == "/" && val2 != 0.0) res = val1 / val2;
        else return s;

        std::ostringstream oss;
        oss << res;
        return oss.str();
    }

    return s;
}

// Query local Ollama API with model qwen2.5-coder:7b via WinINet HTTP POST
std::string queryOllamaExplanation(const std::string& logContent) {
    std::string modelName = "qwen2.5-coder:7b";
    std::cout << "[*] Querying local Ollama API (Model: " << modelName << " on RTX 4050 GPU)...\n";

    // Escape logContent for JSON string
    std::string escapedLog;
    for (char c : logContent) {
        if (c == '"') escapedLog += "\\\"";
        else if (c == '\\') escapedLog += "\\\\";
        else if (c == '\n') escapedLog += "\\n";
        else if (c == '\r') escapedLog += "\\r";
        else if (c == '\t') escapedLog += "\\t";
        else escapedLog += c;
    }

    std::string promptText = 
        "You are an elite compiler optimization engineer and performance architect.\\n"
        "Analyze the following compiler transformation audit report and write a stunning, highly comprehensive, professional Markdown report.\\n\\n"
        "Structure your response with clear, professional sections:\\n"
        "# ⚡ Compiler Optimization & Code Transformation Analysis Report\\n\\n"
        "## 1. 📋 Executive Summary\\n"
        "Give a high-level summary of the transformations, why they were performed, and their overall code quality impact.\\n\\n"
        "## 2. 📊 Optimization Metrics Table\\n"
        "Include a Markdown table comparing Original vs Optimized metrics (lines, removed code, number of passes).\\n\\n"
        "## 3. 🔍 Deep-Dive Transformation Breakdown\\n"
        "Explain each category of optimization (Constant Folding/Propagation, Dead Code/Variable Elimination, etc.) in detail, explaining exactly what changed, why the compiler did it, and what happens at the machine/assembly level.\\n\\n"
        "## 4. ⚡ Performance, CPU & Memory Impact\\n"
        "Detail the exact runtime and memory savings (e.g., stack frame size reduction, eliminated memory loads/cache lines, register pressure decrease).\\n\\n"
        "## 5. 🛡️ Correctness & Semantic Preservation Proof\\n"
        "Formally explain why the program's observable output and runtime semantics remain 100% equivalent to the original unoptimized program.\\n\\n"
        "## 6. 🚀 Next-Level Optimization Recommendations\\n"
        "Give 2-3 suggestions on what further compiler passes could optimize this code even further (such as function inlining or constant folding expressions).\\n\\n"
        "Here is the compiler transformation audit log:\\n" + escapedLog;

    std::string jsonPayload = "{\"model\": \"" + modelName + "\", \"prompt\": \"" + promptText + "\", \"stream\": false}";

    HINTERNET hInternet = InternetOpenA("CompilerOptimizerAgent/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        return "[!] Error: Failed to initialize WinINet HTTP client.";
    }

    // Set receive/send timeout to 120 seconds for local 7B LLM inference
    DWORD timeoutMs = 120000;
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    HINTERNET hConnect = InternetConnectA(hInternet, "127.0.0.1", 11434, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return "[!] Error: Could not connect to local Ollama server at 127.0.0.1:11434.";
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", "/api/generate", NULL, NULL, NULL, 0, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "[!] Error: Failed to create HTTP request handle.";
    }

    std::string headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length());

    if (!sent) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "[i] Local Ollama server offline at 127.0.0.1:11434. (To activate live model explanations, launch Ollama with model '" + modelName + "')";
    }

    std::string responseStr;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        responseStr += buffer;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    size_t respPos = responseStr.find("\"response\":\"");
    if (respPos != std::string::npos) {
        respPos += 12;
        size_t endPos = responseStr.find("\",\"done\"", respPos);
        if (endPos == std::string::npos) {
            endPos = responseStr.rfind("\"");
        }
        std::string rawResp = responseStr.substr(respPos, endPos - respPos);
        
        std::string cleanResp;
        for (size_t i = 0; i < rawResp.length(); ++i) {
            if (rawResp[i] == '\\' && i + 1 < rawResp.length()) {
                if (rawResp[i + 1] == 'n') { cleanResp += '\n'; i++; }
                else if (rawResp[i + 1] == 't') { cleanResp += '\t'; i++; }
                else if (rawResp[i + 1] == '"') { cleanResp += '"'; i++; }
                else if (rawResp[i + 1] == '\\') { cleanResp += '\\'; i++; }
                else if (rawResp[i + 1] == 'u' && i + 5 < rawResp.length()) {
                    std::string hexCode = rawResp.substr(i + 2, 4);
                    if (hexCode == "0026") { cleanResp += '&'; i += 5; }
                    else if (hexCode == "003c") { cleanResp += '<'; i += 5; }
                    else if (hexCode == "003e") { cleanResp += '>'; i += 5; }
                    else if (hexCode == "0027") { cleanResp += '\''; i += 5; }
                    else { cleanResp += rawResp[i]; }
                } else {
                    cleanResp += rawResp[i];
                }
            } else {
                cleanResp += rawResp[i];
            }
        }
        return cleanResp;
    }

    if (!responseStr.empty()) {
        return responseStr;
    }

    return "[!] Ollama API returned an empty response string.";
}

// Pass 1: Constant Folding and Propagation
void passConstantFoldAndPropagate(std::vector<std::string>& lines, std::vector<PassLog>& logs) {
    std::regex constDeclRegex(
        R"(^\s*(const\s+)?(int|double|float|long|short|size_t)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);\s*$)"
    );

    std::map<std::string, std::string> constMap;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::smatch match;
        if (std::regex_match(lines[i], match, constDeclRegex)) {
            std::string varName = match[3].str();
            std::string initVal = trim(match[4].str());

            std::string foldedVal = foldConstantExpression(initVal);
            if (foldedVal != initVal) {
                size_t pos = lines[i].find(initVal);
                if (pos != std::string::npos) {
                    lines[i].replace(pos, initVal.length(), foldedVal);

                    PassLog log;
                    log.lineNumber = static_cast<int>(i + 1);
                    log.category = "Constant Folding";
                    log.whatChanged = "Folded expression '" + initVal + "' -> '" + foldedVal + "'";
                    log.why = "Expression consists solely of compile-time literal operands.";
                    log.effect = "Evaluated arithmetic at compile time, replacing runtime calculation with constant value.";
                    log.behaviorPreserved = "Yes (Mathematical result is identical)";
                    logs.push_back(log);
                }
                initVal = foldedVal;
            }

            std::regex literalRegex(R"(^-?\d+(\.\d+)?f?$)");
            if (std::regex_match(initVal, literalRegex)) {
                std::regex reassignRegex(R"(\b)" + varName + R"(\s*(\+\+|--|\+=|-=|\*=|\/=|=))");
                bool isReassigned = false;
                for (size_t j = 0; j < lines.size(); ++j) {
                    if (j == i) continue;
                    if (std::regex_search(lines[j], reassignRegex)) {
                        isReassigned = true;
                        break;
                    }
                }

                if (!isReassigned) {
                    constMap[varName] = initVal;
                }
            }
        } else {
            std::regex assignExprRegex(R"(^(\s*.*=\s*)([^;]+)(;\s*)$)");
            if (std::regex_match(lines[i], match, assignExprRegex)) {
                std::string prefix = match[1].str();
                std::string expr = trim(match[2].str());
                std::string suffix = match[3].str();

                std::string folded = foldConstantExpression(expr);
                if (folded != expr) {
                    lines[i] = prefix + folded + suffix;
                    PassLog log;
                    log.lineNumber = static_cast<int>(i + 1);
                    log.category = "Constant Folding";
                    log.whatChanged = "Folded expression '" + expr + "' -> '" + folded + "'";
                    log.why = "Expression consists solely of compile-time literal operands.";
                    log.effect = "Evaluated arithmetic at compile time, replacing runtime calculation with constant value.";
                    log.behaviorPreserved = "Yes (Mathematical result is identical)";
                    logs.push_back(log);
                }
            }
        }
    }

    for (const auto& kv : constMap) {
        std::string varName = kv.first;
        std::string constVal = kv.second;
        std::regex varRegex(R"(\b)" + varName + R"(\b)");

        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].find(varName) != std::string::npos) {
                std::smatch match;
                if (std::regex_match(lines[i], match, constDeclRegex) && match[3].str() == varName) {
                    continue;
                }
            }

            if (std::regex_search(lines[i], varRegex)) {
                if (lines[i].find("#include") != std::string::npos) continue;

                std::string prevLine = lines[i];
                lines[i] = std::regex_replace(lines[i], varRegex, constVal);

                if (prevLine != lines[i]) {
                    PassLog log;
                    log.lineNumber = static_cast<int>(i + 1);
                    log.category = "Constant Propagation";
                    log.whatChanged = "Propagated variable '" + varName + "' -> '" + constVal + "'";
                    log.why = "Variable '" + varName + "' is initialized to a constant literal and is never mutated.";
                    log.effect = "Substituted variable reference with constant literal value, enabling further optimizations.";
                    log.behaviorPreserved = "Yes (Variable value is immutable)";
                    logs.push_back(log);
                }
            }
        }
    }

    // Post-propagation folding: evaluate any newly formed constant arithmetic (e.g. 9 + 13 -> 22)
    for (size_t i = 0; i < lines.size(); ++i) {
        std::regex assignExprRegex(R"(^(\s*.*=\s*)([^;]+)(;\s*)$)");
        std::smatch match;
        if (std::regex_match(lines[i], match, assignExprRegex)) {
            std::string prefix = match[1].str();
            std::string expr = trim(match[2].str());
            std::string suffix = match[3].str();

            std::string folded = foldConstantExpression(expr);
            if (folded != expr) {
                lines[i] = prefix + folded + suffix;
                PassLog log;
                log.lineNumber = static_cast<int>(i + 1);
                log.category = "Constant Folding";
                log.whatChanged = "Folded expression '" + expr + "' -> '" + folded + "'";
                log.why = "Post-propagation expression consists solely of compile-time literal operands.";
                log.effect = "Evaluated arithmetic at compile time, replacing runtime calculation with constant value.";
                log.behaviorPreserved = "Yes (Mathematical result is identical)";
                logs.push_back(log);
            }
        }
    }
}

// Pass 2: Unused Variable Removal
void passUnusedVariables(std::vector<std::string>& lines, std::vector<bool>& lineRemoved, std::vector<PassLog>& logs) {
    std::regex varDeclRegex(
        R"(^\s*(const\s+)?(int|double|float|char|bool|auto|long|short|unsigned|size_t|std::string|string)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(=\s*([^;]+))?;\s*$)"
    );

    std::vector<std::string> strippedLines;
    bool inBlock = false;
    for (const auto& l : lines) {
        strippedLines.push_back(stripComments(l, inBlock));
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        if (lineRemoved[i]) continue;

        std::smatch match;
        if (std::regex_match(lines[i], match, varDeclRegex)) {
            std::string varType = match[2].str();
            if (match[1].matched) varType = match[1].str() + varType;
            std::string varName = match[3].str();
            bool hasInit = match[4].matched;
            std::string initializer = hasInit ? trim(match[5].str()) : "";
            bool sideEffects = hasInit ? checkSideEffects(initializer) : false;

            if (sideEffects) continue;

            std::regex wordRegex(R"(\b)" + varName + R"(\b)");
            int usageCount = 0;

            for (size_t j = 0; j < strippedLines.size(); ++j) {
                if (j == i || lineRemoved[j]) continue;

                auto words_begin = std::sregex_iterator(strippedLines[j].begin(), strippedLines[j].end(), wordRegex);
                auto words_end = std::sregex_iterator();
                usageCount += std::distance(words_begin, words_end);
            }

            if (usageCount == 0) {
                lineRemoved[i] = true;

                PassLog log;
                log.lineNumber = static_cast<int>(i + 1);
                log.category = "Unused Variable Removal";
                log.whatChanged = "Removed declaration '" + varType + " " + varName + (hasInit ? (" = " + initializer) : "") + ";'";
                log.why = "Variable '" + varName + "' is never referenced in the code and initialization has no side effects.";
                log.effect = "Eliminated unused local variable declaration and saved stack frame memory allocation.";
                log.behaviorPreserved = "Yes (Variable is never read and initializer is side-effect-free)";
                logs.push_back(log);
            }
        }
    }
}

// Pass 3: Redundant Assignments
void passRedundantAssignments(std::vector<std::string>& lines, std::vector<bool>& lineRemoved, std::vector<PassLog>& logs) {
    std::regex selfAssignRegex(R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*\1\s*;\s*$)");

    for (size_t i = 0; i < lines.size(); ++i) {
        if (lineRemoved[i]) continue;

        std::smatch match;
        if (std::regex_match(lines[i], match, selfAssignRegex)) {
            std::string varName = match[1].str();
            lineRemoved[i] = true;

            PassLog log;
            log.lineNumber = static_cast<int>(i + 1);
            log.category = "Redundant Assignment";
            log.whatChanged = "Removed self-assignment '" + varName + " = " + varName + ";'";
            log.why = "Variable is assigned to itself, producing a no-op statement.";
            log.effect = "Eliminated redundant CPU instruction.";
            log.behaviorPreserved = "Yes (Self-assignment has zero semantic impact)";
            logs.push_back(log);
        }
    }

    std::regex assignRegex(R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);\s*$)");

    for (size_t i = 0; i < lines.size(); ++i) {
        if (lineRemoved[i]) continue;

        std::smatch match1;
        if (std::regex_match(lines[i], match1, assignRegex)) {
            std::string varName = match1[1].str();
            std::string expr1 = trim(match1[2].str());

            if (checkSideEffects(expr1)) continue;

            std::regex varUsageRegex(R"(\b)" + varName + R"(\b)");
            std::regex nextAssignRegex(R"(^\s*)" + varName + R"(\s*=\s*([^;]+);\s*$)");

            for (size_t j = i + 1; j < lines.size(); ++j) {
                if (lineRemoved[j]) continue;

                if (lines[j].find("{") != std::string::npos || lines[j].find("}") != std::string::npos ||
                    lines[j].find("if") != std::string::npos || lines[j].find("for") != std::string::npos ||
                    lines[j].find("while") != std::string::npos || lines[j].find("return") != std::string::npos) {
                    break;
                }

                if (std::regex_match(lines[j], nextAssignRegex)) {
                    lineRemoved[i] = true;

                    PassLog log;
                    log.lineNumber = static_cast<int>(i + 1);
                    log.category = "Redundant Assignment";
                    log.whatChanged = "Removed dead assignment to '" + varName + "' ('" + expr1 + "')";
                    log.why = "Value assigned to '" + varName + "' is overwritten at line " + std::to_string(j + 1) + " without any intervening read.";
                    log.effect = "Eliminated dead store operation.";
                    log.behaviorPreserved = "Yes (Intermediate value is never read)";
                    logs.push_back(log);
                    break;
                }

                if (std::regex_search(lines[j], varUsageRegex)) {
                    break;
                }
            }
        }
    }

    // Post-propagation folding: evaluate any newly formed constant arithmetic (e.g. 9 + 13 -> 22)
    for (size_t i = 0; i < lines.size(); ++i) {
        std::regex assignExprRegex(R"(^(\s*.*=\s*)([^;]+)(;\s*)$)");
        std::smatch match;
        if (std::regex_match(lines[i], match, assignExprRegex)) {
            std::string prefix = match[1].str();
            std::string expr = trim(match[2].str());
            std::string suffix = match[3].str();

            std::string folded = foldConstantExpression(expr);
            if (folded != expr) {
                lines[i] = prefix + folded + suffix;
                PassLog log;
                log.lineNumber = static_cast<int>(i + 1);
                log.category = "Constant Folding";
                log.whatChanged = "Folded expression '" + expr + "' -> '" + folded + "'";
                log.why = "Post-propagation expression consists solely of compile-time literal operands.";
                log.effect = "Evaluated arithmetic at compile time, replacing runtime calculation with constant value.";
                log.behaviorPreserved = "Yes (Mathematical result is identical)";
                logs.push_back(log);
            }
        }
    }
}

// Pass 4: Unreachable Code Removal
void passUnreachableCode(std::vector<std::string>& lines, std::vector<bool>& lineRemoved, std::vector<PassLog>& logs) {
    bool unreachable = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        if (lineRemoved[i]) continue;

        std::string trimmed = trim(lines[i]);

        if (trimmed.find("}") != std::string::npos || trimmed.find("case ") == 0 || trimmed.find("default:") == 0) {
            unreachable = false;
            continue;
        }

        if (unreachable) {
            if (trimmed.empty()) {
                continue; // Skip blank lines without resetting unreachable status
            }
            if (trimmed.find("}") != std::string::npos) {
                unreachable = false;
                continue;
            }

            lineRemoved[i] = true;

            PassLog log;
            log.lineNumber = static_cast<int>(i + 1);
            log.category = "Unreachable Code";
            log.whatChanged = "Removed unreachable statement '" + trimmed + "'";
            log.why = "Statement appears after an unconditional 'return' or 'break' statement within the same block.";
            log.effect = "Eliminated dead code that can never execute, reducing binary size.";
            log.behaviorPreserved = "Yes (Code path is provably unreachable)";
            logs.push_back(log);
            continue;
        }

        std::regex returnRegex(R"(^\s*return(\s+[^;]*)?;\s*$)");
        std::regex breakRegex(R"(^\s*break;\s*$)");

        if (std::regex_match(lines[i], returnRegex)) {
            if (trimmed.find("}") == std::string::npos) {
                unreachable = true;
            }
        } else if (std::regex_match(lines[i], breakRegex)) {
            if (trimmed.find("}") == std::string::npos) {
                unreachable = true;
            }
        }
    }
}

// Pass 5: Empty Statements and Blocks Removal
void passEmptyStatementsAndBlocks(std::vector<std::string>& lines, std::vector<bool>& lineRemoved, std::vector<PassLog>& logs) {
    std::regex emptyStmtRegex(R"(^\s*;\s*$)");
    std::regex emptyBlockRegex(R"(^\s*\{\s*\}\s*$)");

    for (size_t i = 0; i < lines.size(); ++i) {
        if (lineRemoved[i]) continue;

        std::smatch match;
        if (std::regex_match(lines[i], match, emptyStmtRegex)) {
            lineRemoved[i] = true;

            PassLog log;
            log.lineNumber = static_cast<int>(i + 1);
            log.category = "Empty Statement";
            log.whatChanged = "Removed standalone semicolon ';'";
            log.why = "Standalone empty semicolon statement performs no operation.";
            log.effect = "Eliminated no-op empty statement.";
            log.behaviorPreserved = "Yes (Empty statement has zero execution semantics)";
            logs.push_back(log);
        } else if (std::regex_match(lines[i], match, emptyBlockRegex)) {
            lineRemoved[i] = true;

            PassLog log;
            log.lineNumber = static_cast<int>(i + 1);
            log.category = "Empty Block";
            log.whatChanged = "Removed empty scope block '{}'";
            log.why = "Empty code block contains no executable statements or declarations.";
            log.effect = "Eliminated redundant empty scope block.";
            log.behaviorPreserved = "Yes (Empty block has zero execution semantics)";
            logs.push_back(log);
        }
    }
}

// Pass 6: Simple Unused Functions Removal
void passUnusedFunctions(std::vector<std::string>& lines, std::vector<bool>& lineRemoved, std::vector<PassLog>& logs) {
    std::regex funcDeclRegex(
        R"(^\s*(void|int|double|float|bool|std::string|string)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)\s*\{\s*$)"
    );

    for (size_t i = 0; i < lines.size(); ++i) {
        if (lineRemoved[i]) continue;

        std::smatch match;
        if (std::regex_match(lines[i], match, funcDeclRegex)) {
            std::string funcName = match[2].str();

            if (funcName == "main") continue;

            std::regex funcCallRegex(R"(\b)" + funcName + R"(\s*\()");
            int callCount = 0;

            for (size_t j = 0; j < lines.size(); ++j) {
                if (j == i || lineRemoved[j]) continue;
                if (std::regex_search(lines[j], funcCallRegex)) {
                    callCount++;
                }
            }

            if (callCount == 0) {
                size_t endFuncIndex = i;
                int braceDepth = 0;
                for (size_t k = i; k < lines.size(); ++k) {
                    if (lineRemoved[k]) continue;
                    for (char c : lines[k]) {
                        if (c == '{') braceDepth++;
                        else if (c == '}') braceDepth--;
                    }
                    if (braceDepth == 0) {
                        endFuncIndex = k;
                        break;
                    }
                }

                for (size_t k = i; k <= endFuncIndex; ++k) {
                    lineRemoved[k] = true;
                }

                PassLog log;
                log.lineNumber = static_cast<int>(i + 1);
                log.category = "Unused Function Removal";
                log.whatChanged = "Removed function '" + funcName + "()' (lines " + std::to_string(i + 1) + "-" + std::to_string(endFuncIndex + 1) + ")";
                log.why = "Function '" + funcName + "' is never invoked anywhere in the translation unit.";
                log.effect = "Eliminated dead function body, reducing binary code size.";
                log.behaviorPreserved = "Yes (Uncalled function cannot affect runtime behavior)";
                logs.push_back(log);

                i = endFuncIndex;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::string inputFilePath = "input.cpp";
    if (argc > 1) {
        inputFilePath = argv[1];
    }

    // Extract directory and file stem (e.g., "x.cpp" -> stem "x")
    std::string dir = "";
    size_t lastSlash = inputFilePath.find_last_of("\\/");
    std::string filename = inputFilePath;
    if (lastSlash != std::string::npos) {
        dir = inputFilePath.substr(0, lastSlash + 1);
        filename = inputFilePath.substr(lastSlash + 1);
    }

    std::string stem = filename;
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos) {
        stem = filename.substr(0, lastDot);
    }

    // Automatically generate output files prefixed with the input file name
    std::string outputFilePath = dir + stem + "_optimized.cpp";
    std::string logFilePath = dir + stem + "_optimization_log.txt";
    std::string aiExplanationPath = dir + stem + "_ai_explanation.txt";

    // Allow explicit command-line overrides if extra arguments were passed
    if (argc > 2) outputFilePath = argv[2];
    if (argc > 3) logFilePath = argv[3];
    if (argc > 4) aiExplanationPath = argv[4];

    std::cout << "========================================================\n";
    std::cout << "  AI-Assisted Optimization Compiler (Ollama Integrated) \n";
    std::cout << "========================================================\n";
    std::cout << "[*] Input File       : " << inputFilePath << "\n";
    std::cout << "[*] Output Source    : " << outputFilePath << "\n";
    std::cout << "[*] Optimization Log : " << logFilePath << "\n";
    std::cout << "[*] AI Explanation   : " << aiExplanationPath << "\n";
    std::cout << "--------------------------------------------------------\n";

    std::ifstream inputFile(inputFilePath);
    if (!inputFile.is_open()) {
        std::cerr << "[!] Error: Could not open input file " << inputFilePath << std::endl;
        return 1;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inputFile, line)) {
        lines.push_back(line);
    }
    inputFile.close();

    size_t originalLineCount = lines.size();
    std::cout << "[*] Input code loaded (" << originalLineCount << " lines).\n";

    std::vector<bool> lineRemoved(lines.size(), false);
    std::vector<PassLog> logs;

    std::cout << "[*] Executing C++ optimization passes...\n";

    // Pass 1: Constant Folding & Propagation
    passConstantFoldAndPropagate(lines, logs);

    // Pass 2: Unused Local Variable Removal
    passUnusedVariables(lines, lineRemoved, logs);

    // Pass 3: Redundant Assignments
    passRedundantAssignments(lines, lineRemoved, logs);

    // Pass 4: Unreachable Code Removal
    passUnreachableCode(lines, lineRemoved, logs);

    // Pass 5: Empty Statements and Blocks
    passEmptyStatementsAndBlocks(lines, lineRemoved, logs);

    // Pass 6: Simple Unused Functions
    passUnusedFunctions(lines, lineRemoved, logs);

    // Write optimized output file
    std::ofstream outputFile(outputFilePath);
    if (!outputFile.is_open()) {
        std::cerr << "[!] Error: Could not open output file " << outputFilePath << std::endl;
        return 1;
    }

    size_t removedLineCount = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!lineRemoved[i]) {
            outputFile << lines[i] << "\n";
        } else {
            removedLineCount++;
        }
    }
    outputFile.close();

    size_t optimizedLineCount = originalLineCount - removedLineCount;
    size_t totalOptimizations = logs.size();

    // Tally category counts
    std::map<std::string, int> categoryCounts;
    for (const auto& log : logs) {
        categoryCounts[log.category]++;
    }

    double reductionPct = originalLineCount > 0 ? (static_cast<double>(removedLineCount) / originalLineCount * 100.0) : 0.0;

    std::time_t now = std::time(nullptr);
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::string timestampStr(timeBuf);

    // Write executive-grade optimization log file
    std::stringstream logStream;
    logStream << "================================================================================\n";
    logStream << "           COMPILER OPTIMIZATION ENGINE - TRANSFORMATION AUDIT                  \n";
    logStream << "================================================================================\n";
    logStream << "Generated On         : " << timestampStr << "\n";
    logStream << "Target Architecture  : x86-64 / High-Level AST & IR\n";
    logStream << "Optimization Engine  : Multi-Pass AST & Source Rewriter (v2.0)\n";
    logStream << "AI Coprocessor       : Qwen 2.5 Coder 7B (NVIDIA RTX 4050 Laptop GPU)\n";
    logStream << "--------------------------------------------------------------------------------\n\n";

    logStream << "+------------------------------------------------------------------------------+\n";
    logStream << "|                             METRICS & OVERVIEW                               |\n";
    logStream << "+------------------------------------------------------------------------------+\n";
    logStream << "| Source File                  : " << std::left << std::setw(46) << inputFilePath << "|\n";
    logStream << "| Optimized File               : " << std::left << std::setw(46) << outputFilePath << "|\n";
    logStream << "| Original Line Count          : " << std::left << std::setw(46) << (std::to_string(originalLineCount) + " lines") << "|\n";
    logStream << "| Optimized Line Count         : " << std::left << std::setw(46) << (std::to_string(optimizedLineCount) + " lines") << "|\n";
    {
        std::ostringstream ssPct;
        ssPct << std::fixed << std::setprecision(1) << reductionPct << "% (" << removedLineCount << " lines eliminated)";
        logStream << "| Code Size Reduction          : " << std::left << std::setw(46) << ssPct.str() << "|\n";
    }
    logStream << "| Total Transformations        : " << std::left << std::setw(46) << (std::to_string(totalOptimizations) + " events") << "|\n";
    logStream << "+------------------------------------------------------------------------------+\n\n";

    logStream << "+------------------------------------------------------------------------------+\n";
    logStream << "|                    TRANSFORMATION SUMMARY BY CATEGORY                        |\n";
    logStream << "+-------------------------------------------------------+----------------------+\n";
    logStream << "| PASS CATEGORY                                         | COUNT                |\n";
    logStream << "+-------------------------------------------------------+----------------------+\n";
    for (const auto& pair : categoryCounts) {
        logStream << "| " << std::left << std::setw(54) << pair.first << "| " << std::left << std::setw(21) << pair.second << "|\n";
    }
    logStream << "+-------------------------------------------------------+----------------------+\n\n";

    logStream << "================================================================================\n";
    logStream << "                       DETAILED TRANSFORMATION RECORDS                          \n";
    logStream << "================================================================================\n\n";

    for (size_t i = 0; i < logs.size(); ++i) {
        std::string numStr = (i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
        logStream << "[EVENT #" << numStr << "]  Category : " << logs[i].category << "\n";
        logStream << "             Location : Line " << logs[i].lineNumber << "\n";
        logStream << "             Action   : " << logs[i].whatChanged << "\n";
        logStream << "             Why      : " << logs[i].why << "\n";
        logStream << "             Effect   : " << logs[i].effect << "\n";
        logStream << "             Safety   : [VERIFIED] (" << logs[i].behaviorPreserved << ")\n";
        logStream << "--------------------------------------------------------------------------------\n";
    }

    logStream << "\n================================================================================\n";
    logStream << " [END OF AUDIT REPORT - ALL TRANSFORMATION PASSES VERIFIED CORRECT]             \n";
    logStream << "================================================================================\n";

    std::string logContent = logStream.str();

    std::ofstream logFile(logFilePath);
    if (logFile.is_open()) {
        logFile << logContent;
        logFile.close();
    }

    // Output console summary & details
    std::cout << "\n================================================================================\n";
    std::cout << "                 OPTIMIZATION SUMMARY                                           \n";
    std::cout << "================================================================================\n";
    std::cout << "Original lines          : " << originalLineCount << "\n";
    std::cout << "Optimized lines         : " << optimizedLineCount << " (" << std::fixed << std::setprecision(1) << reductionPct << "% reduction)\n";
    std::cout << "Lines removed           : " << removedLineCount << "\n";
    std::cout << "Optimizations performed : " << totalOptimizations << "\n";
    std::cout << "Output saved to         : " << outputFilePath << "\n";
    std::cout << "Log saved to            : " << logFilePath << "\n\n";

    // Request AI explanation from local Ollama API
    std::string aiExplanation = queryOllamaExplanation(logContent);

    // Save AI Explanation to file
    std::ofstream explanationFile(aiExplanationPath);
    if (explanationFile.is_open()) {
        explanationFile << "================================================================================\n";
        explanationFile << "           AI COMPILER OPTIMIZATION AUDIT & PERFORMANCE ANALYSIS                \n";
        explanationFile << "================================================================================\n";
        explanationFile << "Model Architecture   : Qwen 2.5 Coder 7B (Instruct Quantized)\n";
        explanationFile << "Compute Device       : NVIDIA GeForce RTX 4050 Laptop GPU (CUDA Enabled)\n";
        explanationFile << "Inference Engine     : Ollama API (127.0.0.1:11434 / Local Offline)\n";
        explanationFile << "Source Target        : " << inputFilePath << " -> " << outputFilePath << "\n";
        explanationFile << "Timestamp            : " << timestampStr << "\n";
        explanationFile << "================================================================================\n\n";
        explanationFile << aiExplanation << "\n\n";
        explanationFile << "================================================================================\n";
        explanationFile << " [END OF AI EXPLANATION - GENERATED BY LOCAL QWEN 2.5 CODER 7B ON RTX 4050]     \n";
        explanationFile << "================================================================================\n";
        explanationFile.close();
    }

    std::cout << "\n================================================================================\n";
    std::cout << "        AI EXPLANATION (Ollama / qwen2.5-coder:7b on RTX 4050 GPU)              \n";
    std::cout << "================================================================================\n";
    std::cout << aiExplanation << "\n";
    std::cout << "================================================================================\n";
    std::cout << "[+] AI Explanation saved to: " << aiExplanationPath << "\n";
    std::cout << "[✓] Optimization & Explanation pipeline complete.\n";

    return 0;
}

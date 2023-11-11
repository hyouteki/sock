#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream> 
#include <fstream>
#include <cwctype> 
#include <vector>
#include <stack>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "sock_enr.cpp"

typedef enum {
    WALRUS, EQUAL, TEXT, OPEN_CB, CLOSE_CB, PIPE, AT,
    MOD, EXPR_MOD, KEYWORD, QUOTE, SEMI_COLON, ANY,
} Token_Type;

typedef struct Token {
    Token_Type type;
    std::string str; 
public:
    void print() const;   
} Token;

typedef struct Statement {
    std::vector<Token> tokens;
public:
    void print() const;
    std::string tostr() const;
    bool match(const std::vector<Token_Type>) const;
} Statement;

#define RULE_SYNTAX {TEXT, WALRUS, TEXT, EQUAL, TEXT}
#define SHAPE_SYNTAX {TEXT, WALRUS, TEXT, OPEN_CB, ANY, CLOSE_CB, MOD}
#define IMPORT_SYNTAX {KEYWORD, QUOTE, TEXT, QUOTE}
#define MACRO_SYNTAX {AT, TEXT, WALRUS, ANY}

void print(const std::vector<std::string>);
void print_rulebook();
std::vector<std::string> read_file(const std::string);
std::string tokenize_join(const std::string, const char);
Statement parse_statement(const std::string);
Statement expand_macros(const Statement statement);
Statement merge_text(const Statement statement);
Expr* parse_expr(std::string);
void execute_import(const Statement); 
void execute_rule(const Statement);
void execute_shape(const Statement);
void execute_macro(const Statement);
void execute_statement(const Statement);

void Token::print() const {
    std::cout << this->type << "\t" << this->str;
}

void Statement::print() const {
    for (Token token: this->tokens) {
        token.print(); 
        std::cout << std::endl;
    }
}

std::string Statement::tostr() const {
    std::string str = "";
    for (Token token: this->tokens)
        str += token.str + " ";
    return str;
}

bool Statement::match(const std::vector<Token_Type> pattern) const {
    size_t i = 0;
    for (i; i < this->tokens.size() && pattern[i] != ANY; ++i)
        if (this->tokens[i].type != pattern[i]) return false;
    for (size_t j = this->tokens.size()-1, k = pattern.size()-1; k > i; --j, --k) {
        if (this->tokens[j].type != pattern[k]) return false;
    }
    return true;
}

std::vector<std::string> lines;
std::unordered_map<std::string, Rule*> rulebook;
std::unordered_map<std::string, Expr*> exprbook;
std::unordered_map<std::string, std::string> macros;
std::unordered_set<char> special_chars = {'=', ':', '|', '{', '}', '"', ';', '@', '$'};
std::unordered_map<std::string, Token_Type> symbol_type_map = {
    {"all", EXPR_MOD}, {"void", MOD}, {"dump", MOD}, 
    {"$dump", MOD}, {"over", EXPR_MOD}, {"import", KEYWORD},
};

std::string replaceString(
    const std::string initial, 
    const std::string target, 
    const std::string replacement
) {
    std::string result = initial;
    size_t pos = 0;
    while ((pos = result.find(target, pos)) != std::string::npos) {
        result.replace(pos, target.length(), replacement);
        pos += replacement.length();
    }
    return result;
}

void print(const std::vector<std::string> lines) {
    for (std::string line: lines) std::cout << line << std::endl;
}

void print_rulebook() {
    for (auto x: rulebook) {
        std::cout << x.first << " := ";
        x.second->print(); 
        std::cout << std::endl;
    }
}

std::vector<std::string> read_file(const std::string filename) {
    std::ifstream fd(filename);
    if (fd.fail()) {
        std::cerr << "BAD FILE:: Filename `";
        std::cerr << filename << "` specified does not exist" << std::endl;
        exit(1);
    }
    std::string line;
    std::vector<std::string> lines;
    while (getline(fd, line)) {
        if (line.find("#") != std::string::npos) {
            std::string tmp = "";
            for (char ch: line) {
                if (ch != '#') tmp.push_back(ch);
                else break;
            }  
            line = tmp;
        }
        if (line.size() == 0) continue;
        if (line.find("{") != std::string::npos && line.find("}") == std::string::npos) {
            std::string tmp(line);
            while (getline(fd, line)) {
                if (line.find("#") != std::string::npos) {
                    std::string tmp2 = "";
                    for (char ch: line) {
                        if (ch != '#') tmp2.push_back(ch);
                        else break;
                    }  
                    line = tmp2;
                }
                tmp += line;
                if (line.find("}") != std::string::npos) break;
            }
            line = tmp;
        }
        lines.push_back(line);
    }
    fd.close();
    return lines;
}

std::string tokenize_join(const std::string str, const char del = ' ') {
    std::string pre = "";
    for (char ch: str) if (ch != del) pre.push_back(ch);
    return pre;
}

Statement parse_statement(const std::string line) {
    std::string tmp = tokenize_join(line);
    size_t i = 0;
    std::vector<Token> tokens; 
    while (i < tmp.size()) {
        char ch = tmp[i];
        switch (ch) {
            case ' ': {break;}
            case '=': {
                ++i;
                tokens.push_back((Token){.type = EQUAL, .str = "="});
                break;
            }
            case ':': {
                if (i+1 < tmp.size() && tmp[i+1] == '=') {
                    i += 2;
                    tokens.push_back((Token){.type = WALRUS, .str = ":="});
                    break;
                } else {
                    std::cerr << line << std::endl;
                    std::cerr << "^^^ SYNTAX ERROR:: Not a valid token" << std::endl;
                    exit(1);
                }
            } 
            case '{': {
                ++i;
                tokens.push_back((Token){.type = OPEN_CB, .str = "{"});
                break;
            }
            case '}': {
                ++i;
                tokens.push_back((Token){.type = CLOSE_CB, .str = "}"});
                break;
            }
            case '|': {
                ++i;
                tokens.push_back((Token){.type = PIPE, .str = "|"});
                break;
            }
            case '"': {
                ++i;
                tokens.push_back((Token){.type = QUOTE, .str = "\""});
                break;
            }
            case '@': {
                tokens.push_back((Token){.type = AT, .str = "@"});
                ++i;
                std::string symbol = "";
                for (i; i < tmp.size() && isalnum(tmp[i]); ++i)
                    symbol.push_back(tmp[i]);
                tokens.push_back((Token){.type = TEXT, .str = symbol});
                break;
            }
            case ';': {
                ++i;
                tokens.push_back((Token){.type = SEMI_COLON, .str = ";"});
                break;
            }
            case '$': {
                ++i;
                std::string symbol = "$";
                for (i; i < tmp.size() && special_chars.find(tmp[i]) == special_chars.end(); ++i)
                    symbol.push_back(tmp[i]);
                if (symbol_type_map.find(symbol) != symbol_type_map.end())
                    tokens.push_back((Token){.type = symbol_type_map[symbol], .str = symbol});
                else tokens.push_back((Token){.type = TEXT, .str = symbol});
                break;
            }
            default: {
                std::string symbol = "";
                for (i; i < tmp.size() && special_chars.find(tmp[i]) == special_chars.end(); ++i)
                    symbol.push_back(tmp[i]);
                if (symbol_type_map.find(symbol) != symbol_type_map.end())
                    tokens.push_back((Token){.type = symbol_type_map[symbol], .str = symbol});
                else tokens.push_back((Token){.type = TEXT, .str = symbol});
            }
        }
    }
    return (Statement){.tokens = tokens};
}

Statement expand_macros(const Statement statement) {
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < statement.tokens.size()) {
        Token token = statement.tokens[i];
        if (token.type != AT) {
            tokens.push_back(token);
            ++i;
            continue;
        }
        std::string macro_name = statement.tokens[i+1].str;
        Statement expanded = parse_statement(macros[macro_name]);
        expanded = expand_macros(expanded);
        for (Token x: expanded.tokens) tokens.push_back(x);
        i += 2;
    }
    return (Statement){.tokens = tokens};
}

Statement merge_text(const Statement statement) {
    std::vector<Token> tokens;
    for (size_t i = 0; i < statement.tokens.size(); ++i) {
        if (statement.tokens[i].type == TEXT && i > 0 && tokens[tokens.size()-1].type == TEXT)
            tokens[tokens.size()-1].str += statement.tokens[i].str;
        else tokens.push_back(statement.tokens[i]);
    }
    return (Statement){.tokens = tokens};
}

Expr* parse_expr(std::string str) {
    std::stack<std::pair<Expr, bool>> stk;
    std::string pre = "";
    for (wchar_t ch: str) {
        if (iswalnum(ch)) pre.push_back(ch);
        else {
            switch (ch) {
                case '(': {
                    if (pre != "") stk.push(std::make_pair(
                        (Expr){.type = Fun, .name = pre, .args = {}}, false
                    ));
                    break;
                }
                case ')': {
                    if (pre != "") stk.push(std::make_pair(
                        (Expr){.type = Sym, .name = pre, .args = {}}, true
                    ));
                    std::vector<Expr> args;
                    while (!stk.empty() && !(stk.top().first.type == Fun && !stk.top().second)) {
                        args.push_back(stk.top().first.clone());
                        stk.pop();
                    }
                    if (stk.empty()) {
                        if (args.size() == 1) {
                            stk.push(std::make_pair(args[0], true)); 
                        }
                    } else {
                        std::reverse(args.begin(), args.end());
                        Expr expr = stk.top().first;
                        stk.pop();
                        expr.args = args;
                        stk.push(std::make_pair(expr, true));
                    }
                    break;
                }
                case ',': {
                    if (pre != "") stk.push(std::make_pair(
                        (Expr){.type = Sym, .name = pre, .args = {}}, true
                    ));
                    break;
                }
                default:
                    std::cerr << str << std::endl;
                    std::cerr << "^^^ INVALID EXPR" << std::endl;
                    exit(1);
            }
            pre = "";
        }
    }
    if (stk.empty()) {
        Expr* tmp = new Expr();
        tmp->type = Sym;
        tmp->name = pre;
        tmp->args = {};
        return tmp;
    } else {
        Expr* tmp = new Expr(stk.top().first);
        return tmp;
    };
}

void execute_import(const Statement statement) {
    std::string filename = statement.tokens[2].str;
    for (std::string line: read_file(filename)) {
        Statement statement = parse_statement(line);
        if (!statement.match(MACRO_SYNTAX)) statement = expand_macros(statement);
        statement = merge_text(statement);
        execute_statement(statement);
    }
}

void execute_rule(const Statement statement) {
    std::string name = statement.tokens[0].str;
    Expr* left_expr = parse_expr(statement.tokens[2].str);
    Expr* right_expr = parse_expr(statement.tokens[4].str);
    Rule* rule = new Rule();
    rule->left = left_expr;
    rule->right = right_expr;
    rulebook[name] = rule;
}

void execute_shape(const Statement statement) {
    std::string name = statement.tokens[0].str;
    std::string expr_str = statement.tokens[2].str;
    Expr* expr = (exprbook.find(expr_str) == exprbook.end())?
         parse_expr(statement.tokens[2].str): exprbook[expr_str];
    for (size_t i = 4; i+3 < statement.tokens.size(); i += 4) {
        std::string rulename = statement.tokens[i].str;
        if (rulebook.find(rulename) == rulebook.end() && rulename != "?") {
            std::cerr << statement.tostr() << std::endl;
            std::cerr << "^^^ EXISTENTIAL CRISIS: Rule `" << rulename << "` does not exist" << std::endl;
            exit(1);
        }
        std::string expr_mod = statement.tokens[i+2].str;
        if (expr_mod == "all") {
            if (rulename != "?") rulebook[rulename]->apply_all(expr);
            else for (auto x: rulebook) x.second->apply_all(expr);
        }
        else if (expr_mod == "over") rulebook[rulename]->apply(expr);
        else {
            std::cerr << statement.tostr() << std::endl;
            std::cerr << "^^^ INVALID EXPR MOD: Expression mod `";
            std::cerr << expr_mod << "` is not valid" << std::endl;
            exit(1);
        }
    }
    std::string mod = statement.tokens[statement.tokens.size()-1].str;
    if (mod == "void");
    else if (mod == "dump") {
        expr->print();
        std::cout << std::endl;
    }
    else if (mod == "$dump") {
        std::cout << expr->value() << std::endl;
    }
    else {
        std::cerr << statement.tostr() << std::endl;
        std::cerr << "^^^ INVALID MOD: Mod `" << mod << "` is not valid" << std::endl;
        exit(1);
    }
    if (name == "_") return;
    else exprbook[name] = expr;
}

void execute_macro(const Statement statement) {
    std::string name = statement.tokens[1].str;
    std::string val = "";
    for (size_t i = 3; i < statement.tokens.size(); ++i) 
        val += statement.tokens[i].str;
    macros[name] = val;
}

void execute_statement(const Statement statement) {
    if (statement.match(IMPORT_SYNTAX)) execute_import(statement);
    else if (statement.match(RULE_SYNTAX)) execute_rule(statement);
    else if (statement.match(SHAPE_SYNTAX)) execute_shape(statement);
    else if (statement.match(MACRO_SYNTAX)) execute_macro(statement);
    else {
        std::cerr << statement.tostr() << std::endl;
        std::cerr << "^^^ SYNTAX ERROR: Does not match any pattern" << std::endl;
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    std::string filename = (argc > 1)? std::string(argv[1]): "sock.soq";
    for (std::string line: read_file(filename)) {
        Statement statement = parse_statement(line);
        if (!statement.match(MACRO_SYNTAX)) statement = expand_macros(statement);
        statement = merge_text(statement);
        execute_statement(statement); 
    }
    return 0;
}

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
    WALRUS, EQUAL, TEXT, OPEN_CB, CLOSE_CB, PIPE, 
    MOD, EXPR_MOD, KEYWORD, QUOTE, BACK_SLASH, ANY,
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

void print(const std::vector<std::string>);
void print_rulebook();
std::vector<std::string> read_file(const std::string);
std::string tokenize_join(const std::string, const char);
Statement parse_statement(const std::string);
Expr* parse_expr(std::string);
void execute_import(const Statement); 
void execute_rule(const Statement);
void execute_shape(const Statement);
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
    if (this->tokens.size() < pattern.size()) return false;
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
std::unordered_set<char> special_chars = {'=', ':', '|', '{', '}', '"'};
std::unordered_map<std::string, Token_Type> symbol_type_map = {
    {"all", EXPR_MOD}, {"void", MOD}, {"dump", MOD}, {"over", EXPR_MOD}, {"import", KEYWORD},
};

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
        if (line.size() == 0) continue; 
        if (line.find("{") != std::string::npos) {
            std::string tmp(line);
            while (getline(fd, line)) {
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
    Expr* expr = parse_expr(statement.tokens[2].str);
    std::string rulename = statement.tokens[4].str;
    if (rulebook.find(rulename) == rulebook.end() && rulename != "?") {
        std::cerr << statement.tostr() << std::endl;
        std::cerr << "^^^ EXISTENTIAL CRISIS: Rule `" << rulename << "` does not exist" << std::endl;
        exit(1);
    }
    std::string expr_mod = statement.tokens[6].str;
    if (expr_mod == "all") {
        if (rulename != "?") rulebook[rulename]->apply_all(expr);
        else for (auto x: rulebook) x.second->apply_all(expr);
    }
    else if (expr_mod == "over") rulebook[rulename]->apply(expr);
    else {
        std::cerr << statement.tostr() << std::endl;
        std::cerr << "^^^ INVALID EXPR MOD: Expression mod `";
        std::cerr << statement.tokens[6].str << "` is not valid" << std::endl;
        exit(1);
    }
    std::string mod = statement.tokens[8].str;
    if (mod == "void") return;
    else if (mod == "dump") {
        expr->print();
        std::cout << std::endl;
    }
    else {
        std::cerr << statement.tostr() << std::endl;
        std::cerr << "^^^ INVALID MOD: Mod `";
        std::cerr << statement.tokens[8].str << "` is not valid" << std::endl;
        exit(1);
    }
    if (name == "_") return;
    else exprbook[name] = expr;
}

void execute_statement(const Statement statement) {
    if (statement.match(IMPORT_SYNTAX)) execute_import(statement);
    else if (statement.match(RULE_SYNTAX)) execute_rule(statement);
    else if (statement.match(SHAPE_SYNTAX)) execute_shape(statement);
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
        execute_statement(statement);   
    }
    return 0;
}

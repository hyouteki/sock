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
    WALRUS,
    EQUAL,
    RULE,
    TEXT
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
    bool match(const std::vector<Token_Type>) const;
} Statement;

#define RULE_SYNTAX {TEXT, WALRUS, TEXT, EQUAL, TEXT}

void Token::print() const {
    std::cout << this->type << "\t" << this->str;
}

void Statement::print() const {
    for (Token token: this->tokens) {
        token.print();
        std::cout << std::endl;
    }
}

bool Statement::match(const std::vector<Token_Type> pattern) const {
    if (this->tokens.size() != pattern.size()) return false;
    for (size_t i = 0; i < this->tokens.size(); ++i)
        if (this->tokens[i].type != pattern[i]) return false;
    return true;
}

std::string filename = "sock.soq";
std::unordered_map<std::string, Rule> rulebook;
std::unordered_map<std::string, Token_Type> symbol_type_map = {{"rule", RULE}};
std::unordered_set<char> special_chars = {'=', ':'};

void print(std::vector<std::string> lines) {
    for (std::string line: lines) std::cout << line << std::endl;
}

std::vector<std::string> read_file() {
    std::ifstream fd(filename);
    std::string line;
    std::vector<std::string> lines;
    while (getline(fd, line)) lines.push_back(line);
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
                    std::cerr << "SYNTAX ERROR" << std::endl;
                    exit(1);
                }
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

Expr parse_expr(std::string str) {
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
                    std::cerr << "Invalid syntax >> " << str << std::endl;
                    return {};
            }
            pre = "";
        }
    }
    return (stk.empty())? (Expr){.type = Sym, .name = pre, .args = {}}: stk.top().first;
}

void execute_rule(const Statement statement) {
    Expr left_expr = parse_expr(statement.tokens[2].str);
    Expr right_expr = parse_expr(statement.tokens[4].str);
    Rule rule = (Rule){.left = &left_expr, .right = &right_expr};
    rule.print(); std::cout << std::endl;
}

void execute_statement(const Statement statement) {
    if (statement.match(RULE_SYNTAX)) execute_rule(statement);
}

int main(int argc, char* argv[]) {
    if (argc > 1) filename = std::string(argv[1]);
    std::vector<std::string> lines = read_file();
    std::string statement_str = "swap := swap(pair(a, b)) = pair(b, a)";
    Statement statement = parse_statement(statement_str);
    execute_statement(statement);
    return 0;
}

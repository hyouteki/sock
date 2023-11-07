#ifndef SOCK_ENR_CPP_
#define SOCK_ENR_CPP_

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <optional>

typedef enum {
    Sym,
    Fun
} Expr_Type;

typedef struct Expr {
    Expr_Type type;
    std::string name; 
    std::vector<Expr> args;
public:   
    void print() const;
    bool equal(const Expr*) const;
    std::string tostr() const; 
    std::optional<std::unordered_map<std::string, Expr>> match(const Expr*) const;
    void apply(std::unordered_map<std::string, Expr> bindings);
    Expr clone() const;
private:
    bool match_helper(
        const Expr*, 
        std::unordered_map<std::string, Expr>* bindings
    ) const;
} Expr;

typedef struct Rule {
    Expr* left;
    Expr* right;
public:    
    void print() const;
    void apply(Expr*) const;
    void apply_all(Expr*) const;
} Rule;

void Expr::print() const {
    switch (this->type) {
        case Sym:
            std::cout << this->name;
            break;
        case Fun:
            std::cout << this->name << "(";
            for (size_t i = 0; i < this->args.size(); ++i) {
                this->args[i].print();
                if (i != this->args.size() - 1) std::cout << ",";
            }            
            std::cout << ")";
            break;
        default:
            std::cerr <<  "Invalid Expr Type" << std::endl;
    }
}

void print(const std::vector<Expr> exprs) {
    for (Expr expr: exprs) {
        expr.print();
        std::cout << std::endl;
    }
}

bool Expr::equal(const Expr* expr) const {
    if (expr == NULL || this->type != expr->type) return false;
    switch (this->type) {
        case Sym: return this->name == expr->name;
        case Fun:
            if (this->args.size() != expr->args.size()) return false;
            for (size_t i = 0; i < this->args.size(); ++i)
                if (!this->args[i].equal(&expr->args[i])) return false;
            return this->name == expr->name;
        default:
            std::cerr << "Invalid Expr" << std::endl; 
            return false;         
    }
}

std::string Expr::tostr() const {
    std::string out = ""; 
    switch (this->type) {
        case Sym:
            out += this->name;
            break;
        case Fun:
            out += this->name + "(";
            for (size_t i = 0; i < this->args.size(); ++i) {
                out += this->args[i].tostr();
                if (i != this->args.size() - 1) out += ",";
            }            
            out += ")";
            break;
        default:
            std::cerr <<  "Invalid Expr" << std::endl;
    }
    return out;
}

bool Expr::match_helper(
    const Expr* expr, 
    std::unordered_map<std::string, Expr>* bindings
) const {
    if (!expr) return false;
    switch (this->type) {
        case Sym:
            if (bindings->find(this->name) == bindings->end()) {
                (*bindings)[this->name] = *expr;
                return true;
            }
            else return (*bindings)[this->name].equal(expr);
        case Fun:
            switch (expr->type) {
                case Sym: return false;
                case Fun:
                    if (this->name != expr->name || this->args.size() != expr->args.size()) 
                        return false;
                    for (size_t i = 0; i < this->args.size(); ++i)
                        if (!this->args[i].match_helper(&expr->args[i], bindings)) return false;
                    return true;
                default:
                    std::cerr << "Invalid Expr" << std::endl;
                    return false;
            }
        default:
            std::cerr << "Invalid Expr" << std::endl;
            return false;
    }
}

void Expr::apply(std::unordered_map<std::string, Expr> bindings) {
    switch(this->type) {
        case Sym: 
            if (bindings.find(this->name) != bindings.end())
                *this = bindings[this->name];                
            break;
        case Fun:
            for (size_t i = 0; i < this->args.size(); ++i)
                this->args[i].apply(bindings);
            break;
        default:
            std::cerr << "Invalid Expr" << std::endl;
    }
}

Expr Expr::clone() const {
    Expr new_expr = {.type = this->type, .name = this->name, .args = std::vector<Expr>()};
    for (Expr arg: this->args)
        new_expr.args.push_back(arg.clone());
    return new_expr;
}

std::optional<std::unordered_map<std::string, Expr>> Expr::match(const Expr* expr) const {
    std::unordered_map<std::string, Expr> bindings;
    bool match = this->match_helper(expr, &bindings);
    if (match) return bindings;
    return {};
}

void Rule::print() const {
    this->left->print();
    std::cout << " = ";
    this->right->print();
}

void print(const std::vector<Rule> rules) {
    for (Rule rule: rules) {
        rule.print();
        std::cout << std::endl;
    }
}

void Rule::apply(Expr* expr) const {
    auto out = this->left->match(expr);
    if (!out) {
        std::cerr << "Rule does not match with the given Expr" << std::endl;
        return;
    }
    *expr = this->right->clone();
    expr->apply(*out);
}

void Rule::apply_all(Expr* expr) const {
    auto out = this->left->match(expr);
    Expr old_expr = expr->clone();
    if (out) this->apply(expr);
    for (size_t i = 0; i < expr->args.size(); ++i)
        this->apply_all(&expr->args[i]);
    if (!expr->equal(&old_expr)) this->apply_all(expr);
}

void print_bindings(std::unordered_map<std::string, Expr> bindings) {
    for (auto binding: bindings) {
        std::cout << binding.first << " => ";
        binding.second.print();
        std::cout << std::endl;
    }
}

#endif // SOCK_ENR_CPP_
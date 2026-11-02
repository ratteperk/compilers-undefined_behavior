#pragma once

#include "ast.hpp"
#include "symbol_table.hpp"
#include <string>
#include <vector>

struct SemanticError {
    int line;
    int col;
    std::string message;
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(ASTContext &ctx) : ctx(ctx) {}

    bool analyze(const Node *root);
    const std::vector<SemanticError> &getErrors() const { return errors; }
    void printErrors() const;

private:
    ASTContext &ctx;
    SymbolTable symbols;
    std::vector<SemanticError> errors;
    int contextDepth = 0; // глубина func/lambda/prog — гейт для 'return'
    int loopDepth = 0;    // глубина while — гейт для 'break'

    void addError(int line, int col, const std::string &msg);
    void registerTopLevelFunctions(const Node *root);
    void checkNode(const Node *node);
    void checkSpecialForm(const Node *node, const std::string &name);
    void checkCall(const Node *node);
    void checkBuiltinCall(const Node *node, const std::string &name);

    static bool isSpecialForm(const std::string &name);
    static bool isBuiltin(const std::string &name);
    static int getBuiltinArity(const std::string &name);
};

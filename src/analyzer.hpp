#pragma once

#include "ast.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SemanticError {
    int line;
    int col;
    std::string message;
};

class SymbolTable {
  public:
    SymbolTable();
    void enterScope();
    void exitScope();
    bool declare(const std::string &name); // false if duplicate in current scope
    void define(const std::string &name);  // unconditional insertion
    bool lookup(const std::string &name) const;
    bool isCurrentScope(const std::string &name) const;

  private:
    std::vector<std::unordered_set<std::string>> scopes;
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
    int contextDepth = 0; // > 0 if inside func, lambda, or prog

    void addError(int line, int col, const std::string &msg);

    void checkNode(const Node *node);
    void checkSpecialForm(const Node *node, const std::string &name);
    void checkCall(const Node *node);
    void checkBuiltinCall(const Node *node, const std::string &name);

    // Helpers
    static bool isSpecialForm(const std::string &name);
    static bool isBuiltin(const std::string &name);
    static int getBuiltinArity(const std::string &name);
};
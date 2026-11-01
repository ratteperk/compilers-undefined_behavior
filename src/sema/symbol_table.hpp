#pragma once

#include <string>
#include <unordered_set>
#include <vector>

class SymbolTable {
public:
    SymbolTable();
    void enterScope();
    void exitScope();
    bool declare(const std::string &name);
    void define(const std::string &name);
    bool lookup(const std::string &name) const;
    bool isCurrentScope(const std::string &name) const;

private:
    std::vector<std::unordered_set<std::string>> scopes;
};

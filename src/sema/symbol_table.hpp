#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class SymbolKind { Builtin, User };

class SymbolTable {
public:
    SymbolTable();
    void enterScope();
    void exitScope();
    bool declare(const std::string &name);
    void define(const std::string &name);
    bool lookup(const std::string &name) const;
    std::optional<SymbolKind> lookupKind(const std::string &name) const;
    bool isCurrentScope(const std::string &name) const;

private:
    std::vector<std::unordered_map<std::string, SymbolKind>> scopes;
};
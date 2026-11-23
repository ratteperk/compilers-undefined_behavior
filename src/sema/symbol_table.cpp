// src/sema/symbol_table.cpp
#include "symbol_table.hpp"

SymbolTable::SymbolTable() {
    scopes.emplace_back();
    const std::vector<std::string> builtins = {
        "plus",    "minus",     "times",  "divide", "equal",  "nonequal", "less",  "lesseq",
        "greater", "greatereq", "and",    "or",     "xor",    "not",      "head",  "tail",
        "cons",    "isint",     "isreal", "isbool", "isnull", "isatom",   "islist", "eval"};
    for (const auto& b : builtins) {
        scopes.back()[b] = SymbolKind::Builtin;
    }
}

void SymbolTable::enterScope() { scopes.emplace_back(); }

void SymbolTable::exitScope() {
    if (scopes.size() > 1) {
        scopes.pop_back();
    }
}

bool SymbolTable::declare(const std::string& name) {
    if (scopes.empty()) return false;
    auto& current = scopes.back();
    if (current.find(name) != current.end()) {
        return false;
    }
    current[name] = SymbolKind::User;
    return true;
}

void SymbolTable::define(const std::string& name) {
    if (!scopes.empty()) {
        scopes.back()[name] = SymbolKind::User;
    }
}

bool SymbolTable::lookup(const std::string& name) const {
    return lookupKind(name).has_value();
}

std::optional<SymbolKind> SymbolTable::lookupKind(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

bool SymbolTable::isCurrentScope(const std::string& name) const {
    if (scopes.empty()) return false;
    return scopes.back().find(name) != scopes.back().end();
}

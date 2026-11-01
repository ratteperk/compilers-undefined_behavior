#include "symbol_table.hpp"

SymbolTable::SymbolTable() {
    scopes.emplace_back();
    const std::vector<std::string> builtins = {
        "plus",    "minus",     "times",  "divide", "equal",  "nonequal", "less",  "lesseq",
        "greater", "greatereq", "and",    "or",     "xor",    "not",      "head",  "tail",
        "cons",    "isint",     "isreal", "isbool", "isnull", "isatom",   "islist"};
    for (const auto& b : builtins) {
        scopes.back().insert(b);
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
    current.insert(name);
    return true;
}

void SymbolTable::define(const std::string& name) {
    if (!scopes.empty()) {
        scopes.back().insert(name);
    }
}

bool SymbolTable::lookup(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->find(name) != it->end())
            return true;
    }
    return false;
}

bool SymbolTable::isCurrentScope(const std::string& name) const {
    if (scopes.empty()) return false;
    return scopes.back().find(name) != scopes.back().end();
}

#include "analyzer.hpp"

#include <iostream>

SymbolTable::SymbolTable() {
    // Global scope
    scopes.emplace_back();

    // Register built-in operations in the root scope
    const std::vector<std::string> builtins = {
        "plus",    "minus",     "times",  "divide", "equal",  "nonequal", "less",  "lesseq",
        "greater", "greatereq", "and",    "or",     "xor",    "not",      "head",  "tail",
        "cons",    "isint",     "isreal", "isbool", "isnull", "isatom",   "islist"};
    for (const auto& b : builtins) {
        scopes.back().insert(b);
    }
}

void SymbolTable::enterScope() {
    scopes.emplace_back();
}

void SymbolTable::exitScope() {
    if (scopes.size() > 1) {
        scopes.pop_back();
    }
}

bool SymbolTable::declare(const std::string& name) {
    if (scopes.empty())
        return false;
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
    if (scopes.empty())
        return false;
    return scopes.back().find(name) != scopes.back().end();
}

void SemanticAnalyzer::addError(int line, int col, const std::string& msg) {
    errors.push_back({line, col, msg});
}

void SemanticAnalyzer::printErrors() const {
    for (const auto& err : errors) {
        std::cerr << "[line " << err.line << ":" << err.col << "] Semantic Error: " << err.message
                  << "\n";
    }
}

bool SemanticAnalyzer::isSpecialForm(const std::string& name) {
    static const std::unordered_set<std::string> forms = {"quote", "setq", "func",  "lambda",
                                                          "prog",  "cond", "while", "return"};
    return forms.find(name) != forms.end();
}

bool SemanticAnalyzer::isBuiltin(const std::string& name) {
    static const std::unordered_set<std::string> b = {
        "plus",    "minus",     "times",  "divide", "equal",  "nonequal", "less",  "lesseq",
        "greater", "greatereq", "and",    "or",     "xor",    "not",      "head",  "tail",
        "cons",    "isint",     "isreal", "isbool", "isnull", "isatom",   "islist"};
    return b.find(name) != b.end();
}

int SemanticAnalyzer::getBuiltinArity(const std::string& name) {
    if (name == "not" || name == "head" || name == "tail" || name == "isint" || name == "isreal" ||
        name == "isbool" || name == "isnull" || name == "isatom" || name == "islist") {
        return 1;
    }
    return 2;  // All other builtins (math, logic, comparison, cons) require 2 args
}

bool SemanticAnalyzer::analyze(const Node* root) {
    errors.clear();
    if (!root)
        return true;

    if (root->kind == Kind::List) {
        for (const Node* expr : root->items) {
            checkNode(expr);
        }
    } else {
        checkNode(root);
    }

    return errors.empty();
}

void SemanticAnalyzer::checkNode(const Node* node) {
    if (!node)
        return;

    switch (node->kind) {
    case Kind::Int:
    case Kind::Real:
    case Kind::Bool:
    case Kind::Null:
        return;

    case Kind::Atom: {
        // Identifier used as a standalone value
        if (!symbols.lookup(node->name)) {
            addError(node->line, node->col, "Use of undeclared identifier '" + node->name + "'.");
        }
        return;
    }

    case Kind::List: {
        if (node->items.empty())
            return;

        const Node* first = node->items[0];
        if (first->kind == Kind::Atom && isSpecialForm(first->name)) {
            checkSpecialForm(node, first->name);
        } else {
            checkCall(node);
        }
        return;
    }
    }
}

void SemanticAnalyzer::checkSpecialForm(const Node* node, const std::string& name) {
    size_t nArgs = node->items.size() - 1;

    // --- CHECK 1: Special Forms Shape & Arity ---
    if (name == "quote") {
        if (nArgs != 1) {
            addError(node->line, node->col,
                     "Special form 'quote' expects exactly 1 argument, got " +
                         std::to_string(nArgs) + ".");
        }
        // Quote does not analyze its inner payload as executable code!
        return;
    }

    if (name == "setq") {
        if (nArgs != 2) {
            addError(node->line, node->col,
                     "Special form 'setq' expects exactly 2 arguments, got " +
                         std::to_string(nArgs) + ".");
            return;
        }
        const Node* target = node->items[1];
        if (target->kind != Kind::Atom) {
            addError(target->line, target->col,
                     "First argument of 'setq' must be an identifier (Atom), got " +
                         kindToString(target->kind) + ".");
            return;
        }
        // Evaluate the assignment value in current scope
        checkNode(node->items[2]);
        // Register variable in the current scope
        symbols.define(target->name);
        return;
    }

    if (name == "func") {
        if (nArgs < 3) {
            addError(node->line, node->col,
                     "Special form 'func' expects at least 3 arguments (name, parameters, "
                     "body...), got " +
                         std::to_string(nArgs) + ".");
            return;
        }
        const Node* fnName = node->items[1];
        if (fnName->kind != Kind::Atom) {
            addError(fnName->line, fnName->col,
                     "Function name must be an identifier, got " + kindToString(fnName->kind) +
                         ".");
            return;
        }
        const Node* params = node->items[2];
        if (params->kind != Kind::List) {
            addError(params->line, params->col,
                     "Function parameters must be a list, got " + kindToString(params->kind) + ".");
            return;
        }

        // Register function name in enclosing scope
        symbols.define(fnName->name);

        // Enter function body scope
        symbols.enterScope();
        // Allow self-recursion: function name is visible inside its own body
        symbols.define(fnName->name);

        // --- CHECK 3: Duplicate Parameters Check ---
        for (const Node* p : params->items) {
            if (p->kind != Kind::Atom) {
                addError(p->line, p->col,
                         "Function parameter must be an identifier, got " + kindToString(p->kind) +
                             ".");
            } else {
                if (!symbols.declare(p->name)) {
                    addError(p->line, p->col,
                             "Duplicate parameter '" + p->name + "' in function '" + fnName->name +
                                 "'.");
                }
            }
        }

        contextDepth++;
        for (size_t i = 3; i < node->items.size(); ++i) {
            checkNode(node->items[i]);
        }
        contextDepth--;
        symbols.exitScope();
        return;
    }

    if (name == "lambda") {
        if (nArgs < 2) {
            addError(
                node->line, node->col,
                "Special form 'lambda' expects at least 2 arguments (parameters, body...), got " +
                    std::to_string(nArgs) + ".");
            return;
        }
        const Node* params = node->items[1];
        if (params->kind != Kind::List) {
            addError(params->line, params->col,
                     "Lambda parameters must be a list, got " + kindToString(params->kind) + ".");
            return;
        }

        symbols.enterScope();
        for (const Node* p : params->items) {
            if (p->kind != Kind::Atom) {
                addError(p->line, p->col,
                         "Lambda parameter must be an identifier, got " + kindToString(p->kind) +
                             ".");
            } else {
                if (!symbols.declare(p->name)) {
                    addError(p->line, p->col, "Duplicate parameter '" + p->name + "' in lambda.");
                }
            }
        }

        contextDepth++;
        for (size_t i = 2; i < node->items.size(); ++i) {
            checkNode(node->items[i]);
        }
        contextDepth--;
        symbols.exitScope();
        return;
    }

    if (name == "prog") {
        if (nArgs < 1) {
            addError(node->line, node->col,
                     "Special form 'prog' expects at least 1 argument (variables list), got " +
                         std::to_string(nArgs) + ".");
            return;
        }
        const Node* vars = node->items[1];
        if (vars->kind != Kind::List) {
            addError(vars->line, vars->col,
                     "First argument of 'prog' must be a list of variables, got " +
                         kindToString(vars->kind) + ".");
            return;
        }

        symbols.enterScope();
        for (const Node* v : vars->items) {
            if (v->kind != Kind::Atom) {
                addError(v->line, v->col,
                         "Variable in 'prog' must be an identifier, got " + kindToString(v->kind) +
                             ".");
            } else {
                if (!symbols.declare(v->name)) {
                    addError(v->line, v->col,
                             "Duplicate variable '" + v->name + "' declared in 'prog'.");
                }
            }
        }

        contextDepth++;
        for (size_t i = 2; i < node->items.size(); ++i) {
            checkNode(node->items[i]);
        }
        contextDepth--;
        symbols.exitScope();
        return;
    }

    if (name == "cond") {
        if (nArgs < 2 || nArgs > 3) {
            addError(node->line, node->col,
                     "Special form 'cond' expects 2 or 3 arguments, got " + std::to_string(nArgs) +
                         ".");
            return;
        }
        for (size_t i = 1; i < node->items.size(); ++i) {
            checkNode(node->items[i]);
        }
        return;
    }

    if (name == "while") {
        if (nArgs != 2) {
            addError(node->line, node->col,
                     "Special form 'while' expects exactly 2 arguments, got " +
                         std::to_string(nArgs) + ".");
            return;
        }
        checkNode(node->items[1]);
        checkNode(node->items[2]);
        return;
    }

    if (name == "return") {
        // --- CHECK 2: Contextual Keyword Validation ---
        if (contextDepth == 0) {
            addError(node->line, node->col,
                     "'return' statement is not allowed outside of a function or 'prog' context.");
        }
        if (nArgs != 1) {
            addError(node->line, node->col,
                     "Special form 'return' expects exactly 1 argument, got " +
                         std::to_string(nArgs) + ".");
            return;
        }
        checkNode(node->items[1]);
        return;
    }
}

void SemanticAnalyzer::checkCall(const Node* node) {
    const Node* callee = node->items[0];

    if (callee->kind == Kind::Atom) {
        if (!symbols.lookup(callee->name)) {
            addError(callee->line, callee->col,
                     "Use of undeclared function or variable '" + callee->name + "'.");
        } else if (isBuiltin(callee->name)) {
            checkBuiltinCall(node, callee->name);
            return;
        }
    } else {
        // Higher-order function invocation: ((lambda ...) arg) or ((wrap f g) 2)
        checkNode(callee);
    }

    for (size_t i = 1; i < node->items.size(); ++i) {
        checkNode(node->items[i]);
    }
}

void SemanticAnalyzer::checkBuiltinCall(const Node* node, const std::string& name) {
    size_t nArgs = node->items.size() - 1;
    int expected = getBuiltinArity(name);

    if (static_cast<int>(nArgs) != expected) {
        addError(node->line, node->col,
                 "Built-in '" + name + "' expects " + std::to_string(expected) +
                     " arguments, got " + std::to_string(nArgs) + ".");
    }

    for (size_t i = 1; i < node->items.size(); ++i) {
        checkNode(node->items[i]);
    }

    // --- CHECK 4: Static Literal Type & Domain Checking ---
    if (static_cast<int>(nArgs) == expected) {
        const Node* a1 = node->items[1];
        const Node* a2 = (expected == 2) ? node->items[2] : nullptr;

        // Domain: Division by zero check
        if (name == "divide" && a2 && a2->isLiteral()) {
            if ((a2->kind == Kind::Int && a2->i == 0) || (a2->kind == Kind::Real && a2->r == 0.0)) {
                addError(a2->line, a2->col, "Division by zero detected statically.");
            }
        }

        // Static type mismatch: Math
        if (name == "plus" || name == "minus" || name == "times" || name == "divide") {
            if (a1->isLiteral() && a1->kind != Kind::Int && a1->kind != Kind::Real) {
                addError(a1->line, a1->col,
                         "Built-in '" + name + "' requires integer or real arguments, got " +
                             kindToString(a1->kind) + ".");
            }
            if (a2 && a2->isLiteral() && a2->kind != Kind::Int && a2->kind != Kind::Real) {
                addError(a2->line, a2->col,
                         "Built-in '" + name + "' requires integer or real arguments, got " +
                             kindToString(a2->kind) + ".");
            }
        }

        // Static type mismatch: Logic
        if (name == "and" || name == "or" || name == "xor") {
            if (a1->isLiteral() && a1->kind != Kind::Bool) {
                addError(a1->line, a1->col,
                         "Logical operator '" + name + "' requires boolean arguments, got " +
                             kindToString(a1->kind) + ".");
            }
            if (a2 && a2->isLiteral() && a2->kind != Kind::Bool) {
                addError(a2->line, a2->col,
                         "Logical operator '" + name + "' requires boolean arguments, got " +
                             kindToString(a2->kind) + ".");
            }
        }
        if (name == "not") {
            if (a1->isLiteral() && a1->kind != Kind::Bool) {
                addError(a1->line, a1->col,
                         "Logical operator 'not' requires a boolean argument, got " +
                             kindToString(a1->kind) + ".");
            }
        }

        // Static type mismatch: Comparisons
        if (name == "less" || name == "lesseq" || name == "greater" || name == "greatereq") {
            if (a1->isLiteral() && a1->kind != Kind::Int && a1->kind != Kind::Real) {
                addError(a1->line, a1->col,
                         "Comparison operator '" + name + "' requires numeric arguments, got " +
                             kindToString(a1->kind) + ".");
            }
            if (a2 && a2->isLiteral() && a2->kind != Kind::Int && a2->kind != Kind::Real) {
                addError(a2->line, a2->col,
                         "Comparison operator '" + name + "' requires numeric arguments, got " +
                             kindToString(a2->kind) + ".");
            }
        }

        // Static type mismatch: Lists
        if (name == "head" || name == "tail") {
            if (a1->isLiteral() && a1->kind != Kind::List && a1->kind != Kind::Null) {
                addError(a1->line, a1->col,
                         "Built-in '" + name + "' requires a list argument, got " +
                             kindToString(a1->kind) + ".");
            }
        }
    }
}
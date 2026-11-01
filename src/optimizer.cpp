#include "optimizer.hpp"

#include <cmath>

Node* ASTOptimizer::optimize(Node* root) {
    if (!root)
        return nullptr;

    const int MAX_ITERATIONS = 10;
    int iter = 0;
    do {
        changed = false;
        root = pass(root);
        iter++;
    } while (changed && iter < MAX_ITERATIONS);

    return root;
}

bool ASTOptimizer::isPure(const Node* node) {
    if (!node)
        return true;
    if (node->isLiteral())
        return true;
    if (node->kind == Kind::Atom)
        return true;
    if (node->kind == Kind::List) {
        if (node->items.empty())
            return true;
        const Node* head = node->items[0];
        if (head->kind == Kind::Atom) {
            // setq, func, while are impure
            if (head->name == "setq" || head->name == "while" || head->name == "return")
                return false;
            if (head->name == "quote")
                return true;
        }
        for (const Node* item : node->items) {
            if (!isPure(item))
                return false;
        }
        return true;
    }
    return false;
}

Node* ASTOptimizer::makeFoldedNumber(double val, bool isIntResult, int line, int col) {
    if (isIntResult) {
        return ctx.makeInt(static_cast<long long>(val), line, col);
    }
    return ctx.makeReal(val, line, col);
}

Node* ASTOptimizer::pass(Node* node) {
    if (!node)
        return nullptr;

    if (node->kind != Kind::List)
        return node;
    if (node->items.empty())
        return node;

    const Node* first = node->items[0];

    // Do not optimize contents of quote!
    if (first->kind == Kind::Atom && first->name == "quote") {
        return node;
    }

    // --- OPTIMIZATION 3: Unreachable Code Elimination after 'return' ---
    // Applies to sequential bodies: func, lambda, prog, or root elements
    bool isSequentialBlock = false;
    size_t bodyStartIdx = 0;
    if (first->kind == Kind::Atom) {
        if (first->name == "prog" && node->items.size() > 2) {
            isSequentialBlock = true;
            bodyStartIdx = 2;
        } else if (first->name == "func" && node->items.size() > 3) {
            isSequentialBlock = true;
            bodyStartIdx = 3;
        } else if (first->name == "lambda" && node->items.size() > 2) {
            isSequentialBlock = true;
            bodyStartIdx = 2;
        }
    }

    if (isSequentialBlock) {
        std::vector<Node*> trimmed;
        for (size_t i = 0; i < bodyStartIdx; ++i) {
            trimmed.push_back(pass(node->items[i]));
        }
        bool hitReturn = false;
        for (size_t i = bodyStartIdx; i < node->items.size(); ++i) {
            if (hitReturn) {
                // Drop dead code
                changed = true;
                continue;
            }
            Node* optItem = pass(node->items[i]);
            trimmed.push_back(optItem);
            if (optItem->kind == Kind::List && !optItem->items.empty() &&
                optItem->items[0]->kind == Kind::Atom && optItem->items[0]->name == "return") {
                hitReturn = true;
            }
        }
        node->items = std::move(trimmed);
        return node;
    }

    // Bottom-up recursion: optimize all children first
    for (size_t i = 0; i < node->items.size(); ++i) {
        node->items[i] = pass(node->items[i]);
    }

    // --- OPTIMIZATION 2: Dead Branch Elimination & Loop Pruning ---
    if (first->kind == Kind::Atom) {
        // cond simplification
        if (first->name == "cond" && node->items.size() >= 3) {
            const Node* pred = node->items[1];
            if (pred->kind == Kind::Bool) {
                changed = true;
                if (pred->b) {
                    // (cond true then else) -> then
                    return node->items[2];
                } else {
                    // (cond false then else) -> else (or null if no else)
                    if (node->items.size() == 4) {
                        return node->items[3];
                    } else {
                        return ctx.makeNull(node->line, node->col);
                    }
                }
            }
        }

        // while false simplification: (while false body) -> null
        if (first->name == "while" && node->items.size() == 3) {
            const Node* pred = node->items[1];
            if (pred->kind == Kind::Bool && !pred->b) {
                changed = true;
                return ctx.makeNull(node->line, node->col);
            }
        }
    }

    // --- OPTIMIZATION 1: Constant Folding ---
    return foldNode(node);
}

Node* ASTOptimizer::foldNode(Node* node) {
    if (node->kind != Kind::List || node->items.empty())
        return node;
    const Node* first = node->items[0];
    if (first->kind != Kind::Atom)
        return node;

    return foldBuiltin(node, first->name);
}

Node* ASTOptimizer::foldBuiltin(Node* node, const std::string& op) {
    size_t nArgs = node->items.size() - 1;

    // 1-argument operations
    if (nArgs == 1) {
        const Node* a = node->items[1];

        // Type predicates
        if (op == "isint") {
            changed = true;
            return ctx.makeBool(a->kind == Kind::Int, node->line, node->col);
        }
        if (op == "isreal") {
            changed = true;
            return ctx.makeBool(a->kind == Kind::Real, node->line, node->col);
        }
        if (op == "isbool") {
            changed = true;
            return ctx.makeBool(a->kind == Kind::Bool, node->line, node->col);
        }
        if (op == "isnull") {
            changed = true;
            return ctx.makeBool(a->kind == Kind::Null, node->line, node->col);
        }
        if (op == "isatom") {
            changed = true;
            return ctx.makeBool(a->kind == Kind::Atom, node->line, node->col);
        }
        if (op == "islist") {
            changed = true;
            return ctx.makeBool(a->kind == Kind::List, node->line, node->col);
        }

        // Logic NOT
        if (op == "not" && a->kind == Kind::Bool) {
            changed = true;
            return ctx.makeBool(!a->b, node->line, node->col);
        }

        // List operations on static quoted lists
        if (op == "head" && a->kind == Kind::List && a->items.size() == 2 &&
            a->items[0]->kind == Kind::Atom && a->items[0]->name == "quote") {
            const Node* quoted = a->items[1];
            if (quoted->kind == Kind::List && !quoted->items.empty()) {
                changed = true;
                // head '(x y z) -> 'x if x is complex, or literal x
                const Node* h = quoted->items[0];
                if (h->isLiteral())
                    return ctx.clone(h);
                // wrap in quote
                return ctx.makeList({ctx.makeAtom("quote", node->line, node->col), ctx.clone(h)},
                                    node->line, node->col);
            }
        }
        if (op == "tail" && a->kind == Kind::List && a->items.size() == 2 &&
            a->items[0]->kind == Kind::Atom && a->items[0]->name == "quote") {
            const Node* quoted = a->items[1];
            if (quoted->kind == Kind::List && !quoted->items.empty()) {
                changed = true;
                std::vector<Node*> rest(quoted->items.begin() + 1, quoted->items.end());
                Node* tailList = ctx.makeList(std::move(rest), node->line, node->col);
                return ctx.makeList({ctx.makeAtom("quote", node->line, node->col), tailList},
                                    node->line, node->col);
            }
        }
    }

    // 2-argument operations
    if (nArgs == 2) {
        const Node* a = node->items[1];
        const Node* b = node->items[2];

        // Math: plus, minus, times, divide
        bool isMath = (op == "plus" || op == "minus" || op == "times" || op == "divide");
        if (isMath && (a->kind == Kind::Int || a->kind == Kind::Real) &&
            (b->kind == Kind::Int || b->kind == Kind::Real)) {
            bool isIntRes = (a->kind == Kind::Int && b->kind == Kind::Int);
            double v1 = (a->kind == Kind::Int) ? static_cast<double>(a->i) : a->r;
            double v2 = (b->kind == Kind::Int) ? static_cast<double>(b->i) : b->r;

            if (op == "plus") {
                changed = true;
                return makeFoldedNumber(v1 + v2, isIntRes, node->line, node->col);
            }
            if (op == "minus") {
                changed = true;
                return makeFoldedNumber(v1 - v2, isIntRes, node->line, node->col);
            }
            if (op == "times") {
                changed = true;
                return makeFoldedNumber(v1 * v2, isIntRes, node->line, node->col);
            }
            if (op == "divide") {
                if (v2 != 0.0) {
                    changed = true;
                    if (isIntRes) {
                        return ctx.makeInt(a->i / b->i, node->line, node->col);
                    }
                    return ctx.makeReal(v1 / v2, node->line, node->col);
                }
            }
        }

        // Logic: and, or, xor
        if (a->kind == Kind::Bool && b->kind == Kind::Bool) {
            if (op == "and") {
                changed = true;
                return ctx.makeBool(a->b && b->b, node->line, node->col);
            }
            if (op == "or") {
                changed = true;
                return ctx.makeBool(a->b || b->b, node->line, node->col);
            }
            if (op == "xor") {
                changed = true;
                return ctx.makeBool(a->b ^ b->b, node->line, node->col);
            }
        }

        // Short-circuit folding with pure expressions
        if (op == "and") {
            if (a->kind == Kind::Bool && !a->b) {
                changed = true;
                return ctx.makeBool(false, node->line, node->col);
            }
            if (a->kind == Kind::Bool && a->b && isPure(b)) {
                changed = true;
                return ctx.clone(b);
            }
        }
        if (op == "or") {
            if (a->kind == Kind::Bool && a->b) {
                changed = true;
                return ctx.makeBool(true, node->line, node->col);
            }
            if (a->kind == Kind::Bool && !a->b && isPure(b)) {
                changed = true;
                return ctx.clone(b);
            }
        }

        // Comparisons: equal, nonequal, less, lesseq, greater, greatereq
        if ((a->kind == Kind::Int || a->kind == Kind::Real) &&
            (b->kind == Kind::Int || b->kind == Kind::Real)) {
            double v1 = (a->kind == Kind::Int) ? static_cast<double>(a->i) : a->r;
            double v2 = (b->kind == Kind::Int) ? static_cast<double>(b->i) : b->r;

            if (op == "equal") {
                changed = true;
                return ctx.makeBool(v1 == v2, node->line, node->col);
            }
            if (op == "nonequal") {
                changed = true;
                return ctx.makeBool(v1 != v2, node->line, node->col);
            }
            if (op == "less") {
                changed = true;
                return ctx.makeBool(v1 < v2, node->line, node->col);
            }
            if (op == "lesseq") {
                changed = true;
                return ctx.makeBool(v1 <= v2, node->line, node->col);
            }
            if (op == "greater") {
                changed = true;
                return ctx.makeBool(v1 > v2, node->line, node->col);
            }
            if (op == "greatereq") {
                changed = true;
                return ctx.makeBool(v1 >= v2, node->line, node->col);
            }
        }

        if (a->kind == Kind::Bool && b->kind == Kind::Bool) {
            if (op == "equal") {
                changed = true;
                return ctx.makeBool(a->b == b->b, node->line, node->col);
            }
            if (op == "nonequal") {
                changed = true;
                return ctx.makeBool(a->b != b->b, node->line, node->col);
            }
        }
    }

    return node;
}
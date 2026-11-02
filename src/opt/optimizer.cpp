#include "optimizer.hpp"
#include <cmath>
#include <unordered_set>

Node* ASTOptimizer::optimize(Node* root) {
    if (!root) return nullptr;

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
    if (!node) return true;
    if (node->isLiteral()) return true;
    if (node->kind == Kind::Atom) return true;
    if (node->kind == Kind::List) {
        if (node->items.empty()) return true;
        const Node* head = node->items[0];
        if (head->kind == Kind::Atom) {
            if (head->name == "quote") return true;

            // divide чистый ТОЛЬКО если делитель — статически ненулевой литерал
            if (head->name == "divide" && node->items.size() == 3) {
                const Node* divisor = node->items[2];
                bool nonZero = (divisor->kind == Kind::Int && divisor->i != 0) ||
                               (divisor->kind == Kind::Real && divisor->r != 0.0);
                if (!nonZero) return false;
                return isPure(node->items[1]);
            }

            static const std::unordered_set<std::string> pureBuiltins = {
                "plus", "minus", "times", "equal", "nonequal",
                "less", "lesseq", "greater", "greatereq", "and", "or",
                "xor", "not", "head", "tail", "cons", "isint", "isreal",
                "isbool", "isnull", "isatom", "islist"
            };
            if (pureBuiltins.find(head->name) == pureBuiltins.end()) {
                return false;
            }
        } else {
            return false;
        }
        for (const Node* item : node->items) {
            if (!isPure(item)) return false;
        }
        return true;
    }
    return false;
}

// Вид ЗНАЧЕНИЯ выражения, если он известен на этапе компиляции.
// Внимание: это НЕ node->kind. Для атома `x` вид узла — Atom, а вид значения
// зависит от того, что лежит в переменной, и статически неизвестен.
// Известен он только в двух случаях:
//   * голый литерал  5 / 1.5 / true / null  — значение это он сам;
//   * (quote X)      — quote возвращает X невычисленным, значит вид значения = вид X.
bool ASTOptimizer::staticKind(const Node* node, Kind& out) {
    if (!node) return false;
    if (node->isLiteral()) {
        out = node->kind;
        return true;
    }
    if (node->kind == Kind::List && node->items.size() == 2 &&
        node->items[0]->kind == Kind::Atom && node->items[0]->name == "quote") {
        out = node->items[1]->kind;
        return true;
    }
    return false;
}

Node* ASTOptimizer::pass(Node* node) {
    if (!node) return nullptr;
    if (node->kind != Kind::List || node->items.empty()) return node;

    const Node* first = node->items[0];
    if (first->kind == Kind::Atom && first->name == "quote") {
        return node;
    }

    bool isSequentialBlock = false;
    size_t bodyStartIdx = 0;
    if (first->kind == Kind::Atom) {
        if (first->name == "prog" && node->items.size() > 2) {
            isSequentialBlock = true;
            bodyStartIdx = 2; // [0]=prog, [1]=vars
        } else if (first->name == "func" && node->items.size() > 3) {
            isSequentialBlock = true;
            bodyStartIdx = 3; // [0]=func, [1]=name, [2]=params
        } else if (first->name == "lambda" && node->items.size() > 2) {
            isSequentialBlock = true;
            bodyStartIdx = 2; // [0]=lambda, [1]=params
        }
    }

    if (isSequentialBlock) {
        std::vector<Node*> trimmed;
        for (size_t i = 0; i < bodyStartIdx; ++i) {
            trimmed.push_back(node->items[i]);
        }
        bool hitReturn = false;
        for (size_t i = bodyStartIdx; i < node->items.size(); ++i) {
            if (hitReturn) {
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

    for (size_t i = 0; i < node->items.size(); ++i) {
        node->items[i] = pass(node->items[i]);
    }

    // Dead branch pruning
    if (first->kind == Kind::Atom) {
        if (first->name == "cond" && node->items.size() >= 3) {
            const Node* pred = node->items[1];
            if (pred->kind == Kind::Bool) {
                changed = true;
                if (pred->b) {
                    return node->items[2];
                } else {
                    if (node->items.size() == 4) {
                        return node->items[3];
                    } else {
                        return ctx.makeNull(node->line, node->col);
                    }
                }
            }
        }

        if (first->name == "while" && node->items.size() == 3) {
            const Node* pred = node->items[1];
            if (pred->kind == Kind::Bool && !pred->b) {
                changed = true;
                return ctx.makeNull(node->line, node->col);
            }
        }
    }

    return foldNode(node);
}

Node* ASTOptimizer::foldNode(Node* node) {
    if (node->kind != Kind::List || node->items.empty()) return node;
    const Node* first = node->items[0];
    if (first->kind != Kind::Atom) return node;

    return foldBuiltin(node, first->name);
}

Node* ASTOptimizer::foldBuiltin(Node* node, const std::string& op) {
    size_t nArgs = node->items.size() - 1;

    if (nArgs == 1) {
        const Node* a = node->items[1];

        // Предикаты типа: сворачиваем ТОЛЬКО если вид значения известен статически.
        // Для (isint x) ответ зависит от того, что в x во время выполнения, —
        // такой вызов обязан дожить до рантайма.
        static const std::unordered_set<std::string> typePredicates = {
            "isint", "isreal", "isbool", "isnull", "isatom", "islist"};
        if (typePredicates.find(op) != typePredicates.end()) {
            Kind vk;
            if (!staticKind(a, vk)) return node;
            bool result = (op == "isint" && vk == Kind::Int) ||
                          (op == "isreal" && vk == Kind::Real) ||
                          (op == "isbool" && vk == Kind::Bool) ||
                          (op == "isnull" && vk == Kind::Null) ||
                          (op == "isatom" && vk == Kind::Atom) ||
                          (op == "islist" && vk == Kind::List);
            changed = true;
            return ctx.makeBool(result, node->line, node->col);
        }

        if (op == "not" && a->kind == Kind::Bool) {
            changed = true;
            return ctx.makeBool(!a->b, node->line, node->col);
        }

        if (op == "head" && a->kind == Kind::List && a->items.size() == 2 &&
            a->items[0]->kind == Kind::Atom && a->items[0]->name == "quote") {
            const Node* quoted = a->items[1];
            if (quoted->kind == Kind::List && !quoted->items.empty()) {
                changed = true;
                const Node* h = quoted->items[0];
                if (h->isLiteral()) return ctx.clone(h);
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

    if (nArgs == 2) {
        const Node* a = node->items[1];
        const Node* b = node->items[2];

        // Constant folding для cons
        if (op == "cons") {
            if (b->kind == Kind::List && b->items.size() == 2 &&
                b->items[0]->kind == Kind::Atom && b->items[0]->name == "quote") {
                const Node* quoted = b->items[1];
                if (quoted->kind == Kind::List) {
                    Node* elemToInsert = nullptr;
                    if (a->isLiteral()) {
                        elemToInsert = ctx.clone(a);
                    } else if (a->kind == Kind::List && a->items.size() == 2 &&
                               a->items[0]->kind == Kind::Atom && a->items[0]->name == "quote") {
                        elemToInsert = ctx.clone(a->items[1]);
                    }

                    if (elemToInsert) {
                        changed = true;
                        std::vector<Node*> newItems;
                        newItems.reserve(quoted->items.size() + 1);
                        newItems.push_back(elemToInsert);
                        for (const Node* item : quoted->items) {
                            newItems.push_back(ctx.clone(item));
                        }
                        Node* newList = ctx.makeList(std::move(newItems), node->line, node->col);
                        return ctx.makeList({ctx.makeAtom("quote", node->line, node->col), newList},
                                            node->line, node->col);
                    }
                }
            }
        }

        // Арифметика целых чисел
        if (a->kind == Kind::Int && b->kind == Kind::Int) {
            if (op == "plus")  { changed = true; return ctx.makeInt(a->i + b->i, node->line, node->col); }
            if (op == "minus") { changed = true; return ctx.makeInt(a->i - b->i, node->line, node->col); }
            if (op == "times") { changed = true; return ctx.makeInt(a->i * b->i, node->line, node->col); }
            if (op == "divide" && b->i != 0) {
                changed = true;
                return ctx.makeInt(a->i / b->i, node->line, node->col);
            }
        } else if ((a->kind == Kind::Int || a->kind == Kind::Real) &&
                   (b->kind == Kind::Int || b->kind == Kind::Real)) {
            double v1 = (a->kind == Kind::Int) ? static_cast<double>(a->i) : a->r;
            double v2 = (b->kind == Kind::Int) ? static_cast<double>(b->i) : b->r;

            if (op == "plus")  { changed = true; return ctx.makeReal(v1 + v2, node->line, node->col); }
            if (op == "minus") { changed = true; return ctx.makeReal(v1 - v2, node->line, node->col); }
            if (op == "times") { changed = true; return ctx.makeReal(v1 * v2, node->line, node->col); }
            if (op == "divide" && v2 != 0.0) {
                changed = true;
                return ctx.makeReal(v1 / v2, node->line, node->col);
            }
        }

        if (a->kind == Kind::Bool && b->kind == Kind::Bool) {
            if (op == "and") { changed = true; return ctx.makeBool(a->b && b->b, node->line, node->col); }
            if (op == "or")  { changed = true; return ctx.makeBool(a->b || b->b, node->line, node->col); }
            if (op == "xor") { changed = true; return ctx.makeBool(a->b ^ b->b, node->line, node->col); }
        }

        // Short-circuit folding с проверкой чистоты
        if (op == "and") {
            if (a->kind == Kind::Bool && !a->b && isPure(b)) {
                changed = true;
                return ctx.makeBool(false, node->line, node->col);
            }
            if (a->kind == Kind::Bool && a->b && isPure(b)) {
                changed = true;
                return ctx.clone(b);
            }
        }
        if (op == "or") {
            if (a->kind == Kind::Bool && a->b && isPure(b)) {
                changed = true;
                return ctx.makeBool(true, node->line, node->col);
            }
            if (a->kind == Kind::Bool && !a->b && isPure(b)) {
                changed = true;
                return ctx.clone(b);
            }
        }

        if ((a->kind == Kind::Int || a->kind == Kind::Real) &&
            (b->kind == Kind::Int || b->kind == Kind::Real)) {
            double v1 = (a->kind == Kind::Int) ? static_cast<double>(a->i) : a->r;
            double v2 = (b->kind == Kind::Int) ? static_cast<double>(b->i) : b->r;

            if (op == "equal")     { changed = true; return ctx.makeBool(v1 == v2, node->line, node->col); }
            if (op == "nonequal")  { changed = true; return ctx.makeBool(v1 != v2, node->line, node->col); }
            if (op == "less")      { changed = true; return ctx.makeBool(v1 < v2, node->line, node->col); }
            if (op == "lesseq")    { changed = true; return ctx.makeBool(v1 <= v2, node->line, node->col); }
            if (op == "greater")   { changed = true; return ctx.makeBool(v1 > v2, node->line, node->col); }
            if (op == "greatereq") { changed = true; return ctx.makeBool(v1 >= v2, node->line, node->col); }
        }

        if (a->kind == Kind::Bool && b->kind == Kind::Bool) {
            if (op == "equal")    { changed = true; return ctx.makeBool(a->b == b->b, node->line, node->col); }
            if (op == "nonequal") { changed = true; return ctx.makeBool(a->b != b->b, node->line, node->col); }
        }
    }

    return node;
}

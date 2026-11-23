#include "optimizer.hpp"
#include <limits>
#include <unordered_set>

void ASTOptimizer::enterScope(bool isFunctionBoundary) {
    scopes.push_back({{}, isFunctionBoundary});
}

void ASTOptimizer::exitScope() {
    if (!scopes.empty()) {
        scopes.pop_back();
    }
}

void ASTOptimizer::declareLocal(const std::string& name) {
    if (!scopes.empty()) {
        scopes.back().names.insert(name);
    }
}

void ASTOptimizer::defineInFunctionScope(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->names.find(name) != it->names.end()) {
            return;
        }
        if (it->isFunctionBoundary) {
            it->names.insert(name);
            return;
        }
    }
    if (!scopes.empty()) {
        scopes.front().names.insert(name);
    }
}

bool ASTOptimizer::isShadowed(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->names.find(name) != it->names.end()) {
            return true;
        }
    }
    return false;
}

void ASTOptimizer::registerFunctionsInBlock(const Node* listNode, std::size_t startIdx) {
    if (!listNode || listNode->kind != Kind::List) return;
    for (std::size_t i = startIdx; i < listNode->items.size(); ++i) {
        const Node* expr = listNode->items[i];
        if (expr->kind == Kind::List && expr->items.size() >= 4 &&
            expr->items[0]->kind == Kind::Atom && expr->items[0]->name == "func" &&
            expr->items[1]->kind == Kind::Atom) {
            declareLocal(expr->items[1]->name);
        }
    }
}

void ASTOptimizer::collectSetqTargets(const Node* node) {
    if (!node || node->kind != Kind::List || node->items.empty()) return;
    const Node* first = node->items[0];
    if (first->kind == Kind::Atom) {
        if (first->name == "quote" || first->name == "func" || first->name == "lambda") {
            return;
        }
        if (first->name == "setq" && node->items.size() == 3) {
            if (node->items[1]->kind == Kind::Atom) {
                defineInFunctionScope(node->items[1]->name);
            }
            collectSetqTargets(node->items[2]);
            return;
        }
    }
    for (const Node* child : node->items) {
        collectSetqTargets(child);
    }
}

Node* ASTOptimizer::optimize(Node* root) {
    if (!root) return nullptr;

    const int MAX_ITERATIONS = 10;
    int iter = 0;
    do {
        changed = false;
        scopes.clear();
        enterScope(true);

        if (root->kind == Kind::List) {
            root = passBlock(root, 0, false);
        } else {
            root = pass(root);
        }

        exitScope();
        iter++;
    } while (changed && iter < MAX_ITERATIONS);

    return root;
}

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

bool ASTOptimizer::isGuaranteedBool(const Node* node) const {
    if (!node) return false;
    if (node->kind == Kind::Bool) return true;
    if (node->kind == Kind::List && !node->items.empty()) {
        const Node* head = node->items[0];
        if (head->kind != Kind::Atom || isShadowed(head->name)) {
            return false;
        }
        const std::string& op = head->name;
        if (node->items.size() == 2) {
            if (op == "isint" || op == "isreal" || op == "isbool" || op == "isnull" ||
                op == "isatom" || op == "islist") {
                return true;
            }
            if (op == "not") {
                return isGuaranteedBool(node->items[1]);
            }
        }
        if (node->items.size() == 3 && (op == "and" || op == "or" || op == "xor")) {
            return isGuaranteedBool(node->items[1]) && isGuaranteedBool(node->items[2]);
        }
    }
    return false;
}

bool ASTOptimizer::isPure(const Node* node) const {
    if (!node) return true;
    if (node->isLiteral() || node->kind == Kind::Atom) return true;
    if (node->kind != Kind::List || node->items.empty()) return false;

    const Node* head = node->items[0];
    if (head->kind != Kind::Atom) return false;
    if (head->name == "quote") return true;
    if (isShadowed(head->name)) return false;

    const std::string& op = head->name;
    std::size_t nArgs = node->items.size() - 1;

    // Предикаты типа принимают любое значение и не кидают RuntimeError
    if (nArgs == 1 && (op == "isint" || op == "isreal" || op == "isbool" || op == "isnull" ||
                       op == "isatom" || op == "islist")) {
        return isPure(node->items[1]);
    }

    if (nArgs == 1 && op == "not") {
        return isGuaranteedBool(node->items[1]) && isPure(node->items[1]);
    }

    if (nArgs == 2 && (op == "and" || op == "or" || op == "xor")) {
        return isGuaranteedBool(node->items[1]) && isGuaranteedBool(node->items[2]) &&
               isPure(node->items[1]) && isPure(node->items[2]);
    }

    if ((op == "head" || op == "tail") && nArgs == 1) {
        const Node* arg = node->items[1];
        return arg->kind == Kind::List && arg->items.size() == 2 &&
               arg->items[0]->kind == Kind::Atom && arg->items[0]->name == "quote" &&
               arg->items[1]->kind == Kind::List && !arg->items[1]->items.empty();
    }

    if (op == "cons" && nArgs == 2) {
        const Node* tailArg = node->items[2];
        bool tailIsValid =
            (tailArg->kind == Kind::Null) ||
            (tailArg->kind == Kind::List && tailArg->items.size() == 2 &&
             tailArg->items[0]->kind == Kind::Atom && tailArg->items[0]->name == "quote" &&
             tailArg->items[1]->kind == Kind::List);
        return tailIsValid && isPure(node->items[1]);
    }

    // Арифметика и сравнения над литералами уже сворачиваются в foldBuiltin;
    // на переменных они могут упасть по типам или делению на ноль, поэтому не чисты.
    return false;
}

Node* ASTOptimizer::passBlock(Node* listNode, std::size_t startIdx, bool allowReturnTrim) {
    registerFunctionsInBlock(listNode, startIdx);

    std::vector<Node*> kept;
    kept.reserve(listNode->items.size());
    for (std::size_t i = 0; i < startIdx; ++i) {
        kept.push_back(listNode->items[i]);
    }

    bool hitReturn = false;
    for (std::size_t i = startIdx; i < listNode->items.size(); ++i) {
        Node* item = listNode->items[i];
        bool isFuncDecl = (item->kind == Kind::List && item->items.size() >= 4 &&
                           item->items[0]->kind == Kind::Atom && item->items[0]->name == "func");

        if (hitReturn && !isFuncDecl) {
            changed = true;
            continue;
        }

        Node* optItem = pass(item);
        kept.push_back(optItem);

        if (allowReturnTrim && !hitReturn && optItem->kind == Kind::List &&
            !optItem->items.empty() && optItem->items[0]->kind == Kind::Atom &&
            optItem->items[0]->name == "return") {
            hitReturn = true;
        }
    }

    listNode->items = std::move(kept);
    return listNode;
}

Node* ASTOptimizer::pass(Node* node) {
    if (!node) return nullptr;
    if (node->kind != Kind::List || node->items.empty()) return node;

    const Node* first = node->items[0];
    if (first->kind == Kind::Atom) {
        const std::string& form = first->name;

        if (form == "quote") {
            return node;
        }

        if (form == "func" && node->items.size() >= 4) {
            if (node->items[1]->kind == Kind::Atom) {
                declareLocal(node->items[1]->name);
            }
            enterScope(true);
            if (node->items[2]->kind == Kind::List) {
                for (const Node* p : node->items[2]->items) {
                    if (p->kind == Kind::Atom) {
                        declareLocal(p->name);
                    }
                }
            }
            passBlock(node, 3, true);
            exitScope();
            return node;
        }

        if (form == "lambda" && node->items.size() >= 3) {
            enterScope(true);
            if (node->items[1]->kind == Kind::List) {
                for (const Node* p : node->items[1]->items) {
                    if (p->kind == Kind::Atom) {
                        declareLocal(p->name);
                    }
                }
            }
            passBlock(node, 2, true);
            exitScope();
            return node;
        }

        if (form == "prog" && node->items.size() >= 2) {
            enterScope(false);
            if (node->items[1]->kind == Kind::List) {
                for (const Node* v : node->items[1]->items) {
                    if (v->kind == Kind::Atom) {
                        declareLocal(v->name);
                    }
                }
            }
            passBlock(node, 2, true);
            exitScope();
            return node;
        }

        if (form == "setq" && node->items.size() == 3) {
            node->items[2] = pass(node->items[2]);
            if (node->items[1]->kind == Kind::Atom) {
                defineInFunctionScope(node->items[1]->name);
            }
            return node;
        }

        if (form == "cond" && (node->items.size() == 3 || node->items.size() == 4)) {
            node->items[1] = pass(node->items[1]);
            const Node* pred = node->items[1];
            if (pred->kind == Kind::Bool) {
                changed = true;
                if (pred->b) {
                    return pass(node->items[2]);
                }
                if (node->items.size() == 4) {
                    return pass(node->items[3]);
                }
                return ctx.makeNull(node->line, node->col);
            }
            node->items[2] = pass(node->items[2]);
            if (node->items.size() == 4) {
                node->items[3] = pass(node->items[3]);
            }
            return node;
        }

        if (form == "while" && node->items.size() == 3) {
            collectSetqTargets(node->items[1]);
            collectSetqTargets(node->items[2]);
            node->items[1] = pass(node->items[1]);
            const Node* pred = node->items[1];
            if (pred->kind == Kind::Bool && !pred->b) {
                changed = true;
                return ctx.makeNull(node->line, node->col);
            }
            node->items[2] = pass(node->items[2]);
            return node;
        }
    }

    for (std::size_t i = 0; i < node->items.size(); ++i) {
        node->items[i] = pass(node->items[i]);
    }

    return foldNode(node);
}

Node* ASTOptimizer::foldNode(Node* node) {
    if (node->kind != Kind::List || node->items.empty()) return node;
    const Node* first = node->items[0];
    if (first->kind != Kind::Atom) return node;
    if (isShadowed(first->name)) return node;

    return foldBuiltin(node, first->name);
}

Node* ASTOptimizer::foldBuiltin(Node* node, const std::string& op) {
    std::size_t nArgs = node->items.size() - 1;

    if (nArgs == 1) {
        const Node* a = node->items[1];

        static const std::unordered_set<std::string> typePredicates = {
            "isint", "isreal", "isbool", "isnull", "isatom", "islist"};
        if (typePredicates.find(op) != typePredicates.end()) {
            Kind vk;
            if (!staticKind(a, vk)) return node;
            bool isEmptyQuotedList =
                (a->kind == Kind::List && a->items.size() == 2 &&
                 a->items[0]->kind == Kind::Atom && a->items[0]->name == "quote" &&
                 a->items[1]->kind == Kind::List && a->items[1]->items.empty());
            bool result = (op == "isint" && vk == Kind::Int) ||
                          (op == "isreal" && vk == Kind::Real) ||
                          (op == "isbool" && vk == Kind::Bool) ||
                          (op == "isnull" && (vk == Kind::Null || isEmptyQuotedList)) ||
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

        if (op == "cons") {
            const Node* tailQuotedList = nullptr;
            bool tailIsNull = (b->kind == Kind::Null);
            if (b->kind == Kind::List && b->items.size() == 2 &&
                b->items[0]->kind == Kind::Atom && b->items[0]->name == "quote" &&
                b->items[1]->kind == Kind::List) {
                tailQuotedList = b->items[1];
            }

            if (tailQuotedList || tailIsNull) {
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
                    if (tailQuotedList) {
                        newItems.reserve(tailQuotedList->items.size() + 1);
                    }
                    newItems.push_back(elemToInsert);
                    if (tailQuotedList) {
                        for (const Node* item : tailQuotedList->items) {
                            newItems.push_back(ctx.clone(item));
                        }
                    }
                    Node* newList = ctx.makeList(std::move(newItems), node->line, node->col);
                    return ctx.makeList({ctx.makeAtom("quote", node->line, node->col), newList},
                                        node->line, node->col);
                }
            }
        }

        if (a->kind == Kind::Int && b->kind == Kind::Int) {
            using LL = long long;
            constexpr LL LL_MAX = std::numeric_limits<LL>::max();
            constexpr LL LL_MIN = std::numeric_limits<LL>::min();

            if (op == "plus") {
                if (!((b->i > 0 && a->i > LL_MAX - b->i) || (b->i < 0 && a->i < LL_MIN - b->i))) {
                    changed = true;
                    return ctx.makeInt(a->i + b->i, node->line, node->col);
                }
            } else if (op == "minus") {
                if (!((b->i < 0 && a->i > LL_MAX + b->i) || (b->i > 0 && a->i < LL_MIN + b->i))) {
                    changed = true;
                    return ctx.makeInt(a->i - b->i, node->line, node->col);
                }
            } else if (op == "times") {
                bool overflows = false;
                if (a->i != 0 && b->i != 0) {
                    if ((a->i == -1 && b->i == LL_MIN) || (b->i == -1 && a->i == LL_MIN)) {
                        overflows = true;
                    } else if (a->i > 0) {
                        overflows = (b->i > 0) ? (a->i > LL_MAX / b->i) : (b->i < LL_MIN / a->i);
                    } else {
                        overflows = (b->i > 0) ? (a->i < LL_MIN / b->i) : (a->i < LL_MAX / b->i);
                    }
                }
                if (!overflows) {
                    changed = true;
                    return ctx.makeInt(a->i * b->i, node->line, node->col);
                }
            } else if (op == "divide" && b->i != 0 && !(a->i == LL_MIN && b->i == -1)) {
                changed = true;
                return ctx.makeInt(a->i / b->i, node->line, node->col);
            }
        } else if ((a->kind == Kind::Int || a->kind == Kind::Real) &&
                   (b->kind == Kind::Int || b->kind == Kind::Real)) {
            double v1 = (a->kind == Kind::Int) ? static_cast<double>(a->i) : a->r;
            double v2 = (b->kind == Kind::Int) ? static_cast<double>(b->i) : b->r;

            if (op == "plus") { changed = true; return ctx.makeReal(v1 + v2, node->line, node->col); }
            if (op == "minus") { changed = true; return ctx.makeReal(v1 - v2, node->line, node->col); }
            if (op == "times") { changed = true; return ctx.makeReal(v1 * v2, node->line, node->col); }
            if (op == "divide" && v2 != 0.0) {
                changed = true;
                return ctx.makeReal(v1 / v2, node->line, node->col);
            }
        }

        if (a->kind == Kind::Bool && b->kind == Kind::Bool) {
            if (op == "and") { changed = true; return ctx.makeBool(a->b && b->b, node->line, node->col); }
            if (op == "or") { changed = true; return ctx.makeBool(a->b || b->b, node->line, node->col); }
            if (op == "xor") { changed = true; return ctx.makeBool(a->b ^ b->b, node->line, node->col); }
        }

        if (op == "and" && a->kind == Kind::Bool) {
            if (!a->b && isGuaranteedBool(b) && isPure(b)) {
                changed = true;
                return ctx.makeBool(false, node->line, node->col);
            }
            if (a->b && isGuaranteedBool(b)) {
                changed = true;
                return ctx.clone(b);
            }
        }

        if (op == "or" && a->kind == Kind::Bool) {
            if (a->b && isGuaranteedBool(b) && isPure(b)) {
                changed = true;
                return ctx.makeBool(true, node->line, node->col);
            }
            if (!a->b && isGuaranteedBool(b)) {
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
            if (op == "equal") { changed = true; return ctx.makeBool(a->b == b->b, node->line, node->col); }
            if (op == "nonequal") { changed = true; return ctx.makeBool(a->b != b->b, node->line, node->col); }
        }
    }

    return node;
}
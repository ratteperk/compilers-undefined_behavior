#include "interpreter.hpp"
#include "builtins.hpp"
#include <unordered_set>

namespace {

struct EnvClearGuard {
    std::shared_ptr<Env> env;
    ~EnvClearGuard() {
        if (env) env->clear();
    }
};

struct CallDepthGuard {
    int& depth;
    explicit CallDepthGuard(int& d) : depth(d) { ++depth; }
    ~CallDepthGuard() { --depth; }
};

} // namespace

bool Interpreter::isSpecialForm(const std::string& name) {
    static const std::unordered_set<std::string> forms = {"quote", "setq",   "func",
                                                          "lambda", "prog",  "cond",
                                                          "while",  "return", "break"};
    return forms.find(name) != forms.end();
}

bool Interpreter::isTopLevelDefinition(const Node* node) {
    if (!node || node->kind != Kind::List || node->items.empty()) return false;
    const Node* head = node->items[0];
    return head->kind == Kind::Atom && (head->name == "func" || head->name == "setq");
}

ClosurePtr Interpreter::makeClosure(const Node* formNode, const std::string& name,
                                    const Node* paramsNode, size_t bodyStart,
                                    const std::shared_ptr<Env>& env) {
    if (!paramsNode || paramsNode->kind != Kind::List) {
        throw RuntimeError(formNode->line, formNode->col, "Parameters must be a list.");
    }
    auto closure = std::make_shared<Closure>();
    closure->name = name;
    closure->formNode = formNode;
    closure->bodyStart = bodyStart;
    closure->capturedEnv = env;
    closure->params.reserve(paramsNode->items.size());
    for (const Node* p : paramsNode->items) {
        if (p->kind != Kind::Atom) {
            throw RuntimeError(p->line, p->col, "Parameter name must be an identifier.");
        }
        closure->params.push_back(p->name);
    }
    return closure;
}

void Interpreter::registerFunctionsInBlock(const Node* listNode, size_t startIdx,
                                           const std::shared_ptr<Env>& env) {
    if (!listNode || listNode->kind != Kind::List) return;
    for (size_t i = startIdx; i < listNode->items.size(); ++i) {
        const Node* expr = listNode->items[i];
        if (expr->kind == Kind::List && expr->items.size() >= 4 &&
            expr->items[0]->kind == Kind::Atom && expr->items[0]->name == "func") {
            const Node* fnName = expr->items[1];
            if (fnName->kind != Kind::Atom) {
                throw RuntimeError(fnName->line, fnName->col,
                                   "Function name must be an identifier.");
            }
            ClosurePtr fn = makeClosure(expr, fnName->name, expr->items[2], 3, env);
            env->setLocal(fnName->name, Value::makeClosure(std::move(fn)));
        }
    }
}

void Interpreter::run(const Node* root, std::ostream& out) {
    if (!root) return;

    callDepth = 0;
    auto globalEnv = std::make_shared<Env>(nullptr, true);
    EnvClearGuard guard{globalEnv};
    registerBuiltins(*globalEnv);

    if (root->kind != Kind::List) {
        ExecResult res = eval(root, globalEnv);
        if (res.signal != ExecResult::Signal::Normal) {
            throw RuntimeError(root->line, root->col, "Unexpected control flow at top level.");
        }
        out << valueToString(res.value) << "\n";
        return;
    }

    registerFunctionsInBlock(root, 0, globalEnv);

    for (const Node* expr : root->items) {
        ExecResult res = eval(expr, globalEnv);
        if (res.signal == ExecResult::Signal::Return) {
            throw RuntimeError(expr->line, expr->col, "'return' outside of function or 'prog'.");
        }
        if (res.signal == ExecResult::Signal::Break) {
            throw RuntimeError(expr->line, expr->col, "'break' outside of 'while' loop.");
        }
        if (!isTopLevelDefinition(expr)) {
            out << valueToString(res.value) << "\n";
        }
    }
}

Value Interpreter::evalQuote(const Node* node) {
    switch (node->kind) {
    case Kind::Int:  return Value::makeInt(node->i);
    case Kind::Real: return Value::makeReal(node->r);
    case Kind::Bool: return Value::makeBool(node->b);
    case Kind::Null: return Value::makeNull();
    case Kind::Atom: return Value::makeSymbol(node->name);
    case Kind::List: {
        ListPtr tail = nullptr;
        for (auto it = node->items.rbegin(); it != node->items.rend(); ++it) {
            tail = std::make_shared<ConsCell>(ConsCell{evalQuote(*it), std::move(tail)});
        }
        return Value::makeList(std::move(tail));
    }
    }
    return Value::makeNull();
}

ExecResult Interpreter::evalBlock(const Node* listNode, size_t startIdx,
                                  const std::shared_ptr<Env>& env) {
    registerFunctionsInBlock(listNode, startIdx, env);
    Value last = Value::makeNull();
    for (size_t i = startIdx; i < listNode->items.size(); ++i) {
        ExecResult res = eval(listNode->items[i], env);
        if (res.signal == ExecResult::Signal::Return) {
            return ExecResult::normal(std::move(res.value));
        }
        if (res.signal == ExecResult::Signal::Break) {
            return res;
        }
        last = std::move(res.value);
    }
    return ExecResult::normal(std::move(last));
}

ExecResult Interpreter::eval(const Node* node, const std::shared_ptr<Env>& env) {
    if (!node) return ExecResult::normal();

    switch (node->kind) {
    case Kind::Int:  return ExecResult::normal(Value::makeInt(node->i));
    case Kind::Real: return ExecResult::normal(Value::makeReal(node->r));
    case Kind::Bool: return ExecResult::normal(Value::makeBool(node->b));
    case Kind::Null: return ExecResult::normal(Value::makeNull());
    case Kind::Atom: {
        const Value* found = env->lookup(node->name);
        if (!found) {
            throw RuntimeError(node->line, node->col,
                               "Undefined identifier '" + node->name + "'.");
        }
        return ExecResult::normal(*found);
    }
    case Kind::List: {
        if (node->items.empty()) {
            throw RuntimeError(node->line, node->col, "Empty list '()' cannot be evaluated.");
        }
        const Node* first = node->items[0];
        if (first->kind == Kind::Atom && isSpecialForm(first->name)) {
            return evalSpecialForm(node, first->name, env);
        }
        return evalCall(node, env);
    }
    }
    return ExecResult::normal();
}

ExecResult Interpreter::evalSpecialForm(const Node* node, const std::string& name,
                                        const std::shared_ptr<Env>& env) {
    size_t nArgs = node->items.size() - 1;

    if (name == "quote") {
        if (nArgs != 1) {
            throw RuntimeError(node->line, node->col, "'quote' expects 1 argument.");
        }
        return ExecResult::normal(evalQuote(node->items[1]));
    }

    if (name == "setq") {
        if (nArgs != 2 || node->items[1]->kind != Kind::Atom) {
            throw RuntimeError(node->line, node->col,
                               "'setq' expects identifier and value expression.");
        }
        ExecResult rhs = eval(node->items[2], env);
        if (rhs.signal != ExecResult::Signal::Normal) return rhs;
        env->set(node->items[1]->name, rhs.value);
        return ExecResult::normal(std::move(rhs.value));
    }

    if (name == "func") {
        if (nArgs < 3 || node->items[1]->kind != Kind::Atom) {
            throw RuntimeError(node->line, node->col,
                               "'func' expects name, parameters list, and body.");
        }
        const std::string& fnName = node->items[1]->name;
        Value fnVal = Value::makeClosure(makeClosure(node, fnName, node->items[2], 3, env));
        env->setLocal(fnName, fnVal);
        return ExecResult::normal(std::move(fnVal));
    }

    if (name == "lambda") {
        if (nArgs < 2) {
            throw RuntimeError(node->line, node->col, "'lambda' expects parameters list and body.");
        }
        Value fnVal = Value::makeClosure(makeClosure(node, "", node->items[1], 2, env));
        return ExecResult::normal(std::move(fnVal));
    }

    if (name == "prog") {
        if (nArgs < 1 || node->items[1]->kind != Kind::List) {
            throw RuntimeError(node->line, node->col, "'prog' expects a variable list and body.");
        }
        auto progEnv = std::make_shared<Env>(env, false);
        for (const Node* v : node->items[1]->items) {
            if (v->kind != Kind::Atom) {
                throw RuntimeError(v->line, v->col, "Variable in 'prog' must be an identifier.");
            }
            progEnv->setLocal(v->name, Value::makeNull());
        }
        return evalBlock(node, 2, progEnv);
    }

    if (name == "cond") {
        if (nArgs < 2 || nArgs > 3) {
            throw RuntimeError(node->line, node->col, "'cond' expects 2 or 3 arguments.");
        }
        ExecResult pred = eval(node->items[1], env);
        if (pred.signal != ExecResult::Signal::Normal) return pred;
        if (!pred.value.isBool()) {
            throw RuntimeError(node->items[1]->line, node->items[1]->col,
                               "Condition in 'cond' must be boolean, got " +
                                   valueTypeName(pred.value) + ".");
        }
        if (pred.value.asBool()) {
            return eval(node->items[2], env);
        }
        if (nArgs == 3) {
            return eval(node->items[3], env);
        }
        return ExecResult::normal(Value::makeNull());
    }

    if (name == "while") {
        if (nArgs != 2) {
            throw RuntimeError(node->line, node->col, "'while' expects 2 arguments.");
        }
        while (true) {
            ExecResult pred = eval(node->items[1], env);
            if (pred.signal != ExecResult::Signal::Normal) return pred;
            if (!pred.value.isBool()) {
                throw RuntimeError(node->items[1]->line, node->items[1]->col,
                                   "Condition in 'while' must be boolean, got " +
                                       valueTypeName(pred.value) + ".");
            }
            if (!pred.value.asBool()) break;

            ExecResult bodyRes = eval(node->items[2], env);
            if (bodyRes.signal == ExecResult::Signal::Break) break;
            if (bodyRes.signal == ExecResult::Signal::Return) return bodyRes;
        }
        return ExecResult::normal(Value::makeNull());
    }

    if (name == "return") {
        if (nArgs != 1) {
            throw RuntimeError(node->line, node->col, "'return' expects 1 argument.");
        }
        ExecResult res = eval(node->items[1], env);
        if (res.signal != ExecResult::Signal::Normal) return res;
        return ExecResult::ret(std::move(res.value));
    }

    if (name == "break") {
        if (nArgs != 0) {
            throw RuntimeError(node->line, node->col, "'break' expects no arguments.");
        }
        return ExecResult::brk();
    }

    return ExecResult::normal();
}

ExecResult Interpreter::evalCall(const Node* node, const std::shared_ptr<Env>& env) {
    ExecResult calleeRes = eval(node->items[0], env);
    if (calleeRes.signal != ExecResult::Signal::Normal) return calleeRes;

    std::vector<Value> args;
    args.reserve(node->items.size() - 1);
    for (size_t i = 1; i < node->items.size(); ++i) {
        ExecResult argRes = eval(node->items[i], env);
        if (argRes.signal != ExecResult::Signal::Normal) return argRes;
        args.push_back(std::move(argRes.value));
    }

    return apply(calleeRes.value, args, node->line, node->col);
}

ExecResult Interpreter::apply(const Value& callee, const std::vector<Value>& args, int line,
                              int col) {
    if (callee.isBuiltin()) {
        const Builtin& b = callee.asBuiltin();
        if (static_cast<int>(args.size()) != b.arity) {
            throw RuntimeError(line, col,
                               "Built-in '" + b.name + "' expects " + std::to_string(b.arity) +
                                   " arguments, got " + std::to_string(args.size()) + ".");
        }
        return ExecResult::normal(b.fn(args, line, col));
    }

    if (callee.isClosure()) {
        const ClosurePtr& fn = callee.asClosure();
        if (args.size() != fn->params.size()) {
            std::string label = fn->name.empty() ? "lambda" : "function '" + fn->name + "'";
            throw RuntimeError(line, col,
                               "Call to " + label + " expects " +
                                   std::to_string(fn->params.size()) + " arguments, got " +
                                   std::to_string(args.size()) + ".");
        }
        if (callDepth >= MAX_CALL_DEPTH) {
            throw RuntimeError(line, col, "Maximum call depth exceeded.");
        }
        CallDepthGuard depthGuard{callDepth};

        auto callEnv = std::make_shared<Env>(fn->capturedEnv, true);
        for (size_t i = 0; i < fn->params.size(); ++i) {
            callEnv->setLocal(fn->params[i], args[i]);
        }

        ExecResult res = evalBlock(fn->formNode, fn->bodyStart, callEnv);
        if (res.signal == ExecResult::Signal::Break) {
            throw RuntimeError(line, col, "'break' cannot cross function boundary.");
        }
        return res;
    }

    throw RuntimeError(line, col,
                       "Attempt to call non-function value of type " + valueTypeName(callee) + ".");
}
#pragma once

#include "ast.hpp"
#include "env.hpp"
#include "value.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct ExecResult {
    enum class Signal { Normal, Return, Break };

    Signal signal = Signal::Normal;
    Value value;

    static ExecResult normal(Value v = Value::makeNull()) {
        return {Signal::Normal, std::move(v)};
    }
    static ExecResult ret(Value v) { return {Signal::Return, std::move(v)}; }
    static ExecResult brk() { return {Signal::Break, Value::makeNull()}; }
};

class Interpreter {
public:
    void run(const Node* root, std::ostream& out);

private:
    static constexpr int MAX_CALL_DEPTH = 2048;
    int callDepth = 0;

    void registerFunctionsInBlock(const Node* listNode, size_t startIdx,
                                  const std::shared_ptr<Env>& env);
    ClosurePtr makeClosure(const Node* formNode, const std::string& name, const Node* paramsNode,
                           size_t bodyStart, const std::shared_ptr<Env>& env);

    ExecResult eval(const Node* node, const std::shared_ptr<Env>& env);
    ExecResult evalBlock(const Node* listNode, size_t startIdx, const std::shared_ptr<Env>& env);
    ExecResult evalSpecialForm(const Node* node, const std::string& name,
                               const std::shared_ptr<Env>& env);
    ExecResult evalCall(const Node* node, const std::shared_ptr<Env>& env);
    ExecResult apply(const Value& callee, const std::vector<Value>& args, int line, int col);
    Value evalQuote(const Node* node);

    static bool isSpecialForm(const std::string& name);
    static bool isTopLevelDefinition(const Node* node);
};
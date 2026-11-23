#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

struct Node;
class Env;
struct ConsCell;
struct Closure;

struct RuntimeError : public std::runtime_error {
    int line;
    int col;

    RuntimeError(int line, int col, const std::string& msg)
        : std::runtime_error(msg), line(line), col(col) {}
};

struct NullVal {};

struct Symbol {
    std::string name;
};

using ListPtr = std::shared_ptr<const ConsCell>;
using ClosurePtr = std::shared_ptr<const Closure>;

struct Value;
using BuiltinFn = Value (*)(const std::vector<Value>& args, int line, int col);

struct Builtin {
    std::string name;
    int arity;
    BuiltinFn fn;
};

struct Value {
    using Variant =
        std::variant<NullVal, long long, double, bool, Symbol, ListPtr, ClosurePtr, Builtin>;
    Variant data;

    Value();
    explicit Value(Variant v);
    Value(const Value&);
    Value(Value&&) noexcept;
    Value& operator=(const Value&);
    Value& operator=(Value&&) noexcept;
    ~Value();

    static Value makeNull();
    static Value makeInt(long long v);
    static Value makeReal(double v);
    static Value makeBool(bool v);
    static Value makeSymbol(std::string name);
    static Value makeList(ListPtr head);
    static Value makeClosure(ClosurePtr fn);
    static Value makeBuiltin(std::string name, int arity, BuiltinFn fn);

    bool isNull() const { return std::holds_alternative<NullVal>(data); }
    bool isInt() const { return std::holds_alternative<long long>(data); }
    bool isReal() const { return std::holds_alternative<double>(data); }
    bool isNumber() const { return isInt() || isReal(); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isSymbol() const { return std::holds_alternative<Symbol>(data); }
    bool isList() const { return std::holds_alternative<ListPtr>(data); }
    bool isEmptyList() const { return isList() && std::get<ListPtr>(data) == nullptr; }
    bool isClosure() const { return std::holds_alternative<ClosurePtr>(data); }
    bool isBuiltin() const { return std::holds_alternative<Builtin>(data); }

    long long asInt() const { return std::get<long long>(data); }
    double asReal() const { return std::get<double>(data); }
    double asNumber() const { return isInt() ? static_cast<double>(asInt()) : asReal(); }
    bool asBool() const { return std::get<bool>(data); }
    const std::string& asSymbol() const { return std::get<Symbol>(data).name; }
    const ListPtr& asList() const { return std::get<ListPtr>(data); }
    const ClosurePtr& asClosure() const { return std::get<ClosurePtr>(data); }
    const Builtin& asBuiltin() const { return std::get<Builtin>(data); }
};

struct ConsCell {
    Value head;
    ListPtr tail;
};

struct Closure {
    std::string name;
    std::vector<std::string> params;
    const Node* formNode = nullptr;
    std::size_t bodyStart = 0;
    std::shared_ptr<Env> capturedEnv;
};

std::string valueTypeName(const Value& v);
std::string valueToString(const Value& v);
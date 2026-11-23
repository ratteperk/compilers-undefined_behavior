#include "value.hpp"
#include <iomanip>
#include <sstream>

Value::Value() : data(NullVal{}) {}
Value::Value(Variant v) : data(std::move(v)) {}
Value::Value(const Value&) = default;
Value::Value(Value&&) noexcept = default;
Value& Value::operator=(const Value&) = default;
Value& Value::operator=(Value&&) noexcept = default;
Value::~Value() = default;

Value Value::makeNull() { return Value(NullVal{}); }
Value Value::makeInt(long long v) { return Value(v); }
Value Value::makeReal(double v) { return Value(v); }
Value Value::makeBool(bool v) { return Value(v); }
Value Value::makeSymbol(std::string name) { return Value(Symbol{std::move(name)}); }
Value Value::makeList(ListPtr head) { return Value(std::move(head)); }
Value Value::makeClosure(ClosurePtr fn) { return Value(std::move(fn)); }
Value Value::makeBuiltin(std::string name, int arity, BuiltinFn fn) {
    return Value(Builtin{std::move(name), arity, fn});
}

std::string valueTypeName(const Value& v) {
    if (v.isNull()) return "Null";
    if (v.isInt()) return "Int";
    if (v.isReal()) return "Real";
    if (v.isBool()) return "Bool";
    if (v.isSymbol()) return "Atom";
    if (v.isList()) return "List";
    if (v.isClosure() || v.isBuiltin()) return "Function";
    return "Unknown";
}

std::string valueToString(const Value& v) {
    if (v.isNull()) return "null";
    if (v.isInt()) return std::to_string(v.asInt());
    if (v.isReal()) {
        std::ostringstream ss;
        ss << std::setprecision(15) << v.asReal();
        std::string s = ss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
            s.find('E') == std::string::npos) {
            s += ".0";
        }
        return s;
    }
    if (v.isBool()) return v.asBool() ? "true" : "false";
    if (v.isSymbol()) return v.asSymbol();
    if (v.isList()) {
        std::string out = "(";
        ListPtr cur = v.asList();
        bool first = true;
        while (cur) {
            if (!first) out += " ";
            first = false;
            out += valueToString(cur->head);
            cur = cur->tail;
        }
        out += ")";
        return out;
    }
    if (v.isClosure()) {
        const auto& name = v.asClosure()->name;
        return name.empty() ? "<lambda>" : "<func " + name + ">";
    }
    if (v.isBuiltin()) {
        return "<builtin " + v.asBuiltin().name + ">";
    }
    return "<unknown>";
}
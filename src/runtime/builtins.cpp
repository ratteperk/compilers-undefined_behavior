#include "builtins.hpp"
#include <limits>

namespace {

using LL = long long;
constexpr LL LL_MAX = std::numeric_limits<LL>::max();
constexpr LL LL_MIN = std::numeric_limits<LL>::min();

void requireNumber(const Value& v, const char* op, int line, int col) {
    if (!v.isNumber()) {
        throw RuntimeError(line, col,
                           std::string("'") + op + "' requires integer or real arguments, got " +
                               valueTypeName(v) + ".");
    }
}

void requireBool(const Value& v, const char* op, int line, int col) {
    if (!v.isBool()) {
        throw RuntimeError(line, col,
                           std::string("'") + op + "' requires boolean arguments, got " +
                               valueTypeName(v) + ".");
    }
}

int compareNumbers(const Value& a, const Value& b) {
    if (a.isInt() && b.isInt()) {
        LL x = a.asInt();
        LL y = b.asInt();
        return (x < y) ? -1 : (x > y ? 1 : 0);
    }
    double x = a.asNumber();
    double y = b.asNumber();
    return (x < y) ? -1 : (x > y ? 1 : 0);
}

Value fnPlus(const std::vector<Value>& args, int line, int col) {
    requireNumber(args[0], "plus", line, col);
    requireNumber(args[1], "plus", line, col);
    if (args[0].isInt() && args[1].isInt()) {
        LL a = args[0].asInt(), b = args[1].asInt();
        if ((b > 0 && a > LL_MAX - b) || (b < 0 && a < LL_MIN - b)) {
            throw RuntimeError(line, col, "Integer overflow in 'plus'.");
        }
        return Value::makeInt(a + b);
    }
    return Value::makeReal(args[0].asNumber() + args[1].asNumber());
}

Value fnMinus(const std::vector<Value>& args, int line, int col) {
    requireNumber(args[0], "minus", line, col);
    requireNumber(args[1], "minus", line, col);
    if (args[0].isInt() && args[1].isInt()) {
        LL a = args[0].asInt(), b = args[1].asInt();
        if ((b < 0 && a > LL_MAX + b) || (b > 0 && a < LL_MIN + b)) {
            throw RuntimeError(line, col, "Integer overflow in 'minus'.");
        }
        return Value::makeInt(a - b);
    }
    return Value::makeReal(args[0].asNumber() - args[1].asNumber());
}

Value fnTimes(const std::vector<Value>& args, int line, int col) {
    requireNumber(args[0], "times", line, col);
    requireNumber(args[1], "times", line, col);
    if (args[0].isInt() && args[1].isInt()) {
        LL a = args[0].asInt(), b = args[1].asInt();
        bool overflows = false;
        if (a != 0 && b != 0) {
            if ((a == -1 && b == LL_MIN) || (b == -1 && a == LL_MIN)) {
                overflows = true;
            } else if (a > 0) {
                overflows = (b > 0) ? (a > LL_MAX / b) : (b < LL_MIN / a);
            } else {
                overflows = (b > 0) ? (a < LL_MIN / b) : (a < LL_MAX / b);
            }
        }
        if (overflows) {
            throw RuntimeError(line, col, "Integer overflow in 'times'.");
        }
        return Value::makeInt(a * b);
    }
    return Value::makeReal(args[0].asNumber() * args[1].asNumber());
}

Value fnDivide(const std::vector<Value>& args, int line, int col) {
    requireNumber(args[0], "divide", line, col);
    requireNumber(args[1], "divide", line, col);
    if (args[0].isInt() && args[1].isInt()) {
        LL a = args[0].asInt(), b = args[1].asInt();
        if (b == 0) {
            throw RuntimeError(line, col, "Division by zero.");
        }
        if (a == LL_MIN && b == -1) {
            throw RuntimeError(line, col, "Integer overflow in 'divide'.");
        }
        return Value::makeInt(a / b);
    }
    double b = args[1].asNumber();
    if (b == 0.0) {
        throw RuntimeError(line, col, "Division by zero.");
    }
    return Value::makeReal(args[0].asNumber() / b);
}

Value fnEqual(const std::vector<Value>& args, int line, int col) {
    const Value& a = args[0];
    const Value& b = args[1];
    if (a.isNumber() && b.isNumber()) {
        return Value::makeBool(compareNumbers(a, b) == 0);
    }
    if (a.isBool() && b.isBool()) {
        return Value::makeBool(a.asBool() == b.asBool());
    }
    throw RuntimeError(line, col,
                       "'equal' cannot compare " + valueTypeName(a) + " and " + valueTypeName(b) +
                           ".");
}

Value fnNonequal(const std::vector<Value>& args, int line, int col) {
    const Value& a = args[0];
    const Value& b = args[1];
    if (a.isNumber() && b.isNumber()) {
        return Value::makeBool(compareNumbers(a, b) != 0);
    }
    if (a.isBool() && b.isBool()) {
        return Value::makeBool(a.asBool() != b.asBool());
    }
    throw RuntimeError(line, col,
                       "'nonequal' cannot compare " + valueTypeName(a) + " and " +
                           valueTypeName(b) + ".");
}

Value fnLess(const std::vector<Value>& args, int line, int col) {
    requireNumber(args[0], "less", line, col);
    requireNumber(args[1], "less", line, col);
    return Value::makeBool(compareNumbers(args[0], args[1]) < 0);
}

Value fnLesseq(const std::vector<Value>& args, int line, int col) {
    requireNumber(args[0], "lesseq", line, col);
    requireNumber(args[1], "lesseq", line, col);
    return Value::makeBool(compareNumbers(args[0], args[1]) <= 0);
}

Value fnGreater(const std::vector<Value>& args, int line, int col) {
    requireNumber(args[0], "greater", line, col);
    requireNumber(args[1], "greater", line, col);
    return Value::makeBool(compareNumbers(args[0], args[1]) > 0);
}

Value fnGreatereq(const std::vector<Value>& args, int line, int col) {
    requireNumber(args[0], "greatereq", line, col);
    requireNumber(args[1], "greatereq", line, col);
    return Value::makeBool(compareNumbers(args[0], args[1]) >= 0);
}

Value fnAnd(const std::vector<Value>& args, int line, int col) {
    requireBool(args[0], "and", line, col);
    requireBool(args[1], "and", line, col);
    return Value::makeBool(args[0].asBool() && args[1].asBool());
}

Value fnOr(const std::vector<Value>& args, int line, int col) {
    requireBool(args[0], "or", line, col);
    requireBool(args[1], "or", line, col);
    return Value::makeBool(args[0].asBool() || args[1].asBool());
}

Value fnXor(const std::vector<Value>& args, int line, int col) {
    requireBool(args[0], "xor", line, col);
    requireBool(args[1], "xor", line, col);
    return Value::makeBool(args[0].asBool() ^ args[1].asBool());
}

Value fnNot(const std::vector<Value>& args, int line, int col) {
    requireBool(args[0], "not", line, col);
    return Value::makeBool(!args[0].asBool());
}

Value fnHead(const std::vector<Value>& args, int line, int col) {
    if (!args[0].isList()) {
        throw RuntimeError(line, col, "'head' requires a list, got " + valueTypeName(args[0]) + ".");
    }
    if (args[0].isEmptyList()) {
        throw RuntimeError(line, col, "'head' cannot be applied to an empty list.");
    }
    return args[0].asList()->head;
}

Value fnTail(const std::vector<Value>& args, int line, int col) {
    if (!args[0].isList()) {
        throw RuntimeError(line, col, "'tail' requires a list, got " + valueTypeName(args[0]) + ".");
    }
    if (args[0].isEmptyList()) {
        throw RuntimeError(line, col, "'tail' cannot be applied to an empty list.");
    }
    return Value::makeList(args[0].asList()->tail);
}

Value fnCons(const std::vector<Value>& args, int line, int col) {
    ListPtr tail;
    if (args[1].isList()) {
        tail = args[1].asList();
    } else if (args[1].isNull()) {
        tail = nullptr;
    } else {
        throw RuntimeError(line, col,
                           "Second argument of 'cons' must be a list or null, got " +
                               valueTypeName(args[1]) + ".");
    }
    return Value::makeList(std::make_shared<ConsCell>(ConsCell{args[0], std::move(tail)}));
}

Value fnIsInt(const std::vector<Value>& args, int line, int col) {
    (void)line;
    (void)col;
    return Value::makeBool(args[0].isInt());
}

Value fnIsReal(const std::vector<Value>& args, int line, int col) {
    (void)line;
    (void)col;
    return Value::makeBool(args[0].isReal());
}

Value fnIsBool(const std::vector<Value>& args, int line, int col) {
    (void)line;
    (void)col;
    return Value::makeBool(args[0].isBool());
}

Value fnIsNull(const std::vector<Value>& args, int line, int col) {
    (void)line;
    (void)col;
    return Value::makeBool(args[0].isNull() || args[0].isEmptyList());
}

Value fnIsAtom(const std::vector<Value>& args, int line, int col) {
    (void)line;
    (void)col;
    return Value::makeBool(args[0].isSymbol());
}

Value fnIsList(const std::vector<Value>& args, int line, int col) {
    (void)line;
    (void)col;
    return Value::makeBool(args[0].isList());
}

} // namespace

void registerBuiltins(Env& globalEnv) {
    auto add = [&](const char* name, int arity, BuiltinFn fn) {
        globalEnv.setLocal(name, Value::makeBuiltin(name, arity, fn));
    };

    add("plus", 2, fnPlus);
    add("minus", 2, fnMinus);
    add("times", 2, fnTimes);
    add("divide", 2, fnDivide);
    add("equal", 2, fnEqual);
    add("nonequal", 2, fnNonequal);
    add("less", 2, fnLess);
    add("lesseq", 2, fnLesseq);
    add("greater", 2, fnGreater);
    add("greatereq", 2, fnGreatereq);
    add("and", 2, fnAnd);
    add("or", 2, fnOr);
    add("xor", 2, fnXor);
    add("not", 1, fnNot);
    add("head", 1, fnHead);
    add("tail", 1, fnTail);
    add("cons", 2, fnCons);
    add("isint", 1, fnIsInt);
    add("isreal", 1, fnIsReal);
    add("isbool", 1, fnIsBool);
    add("isnull", 1, fnIsNull);
    add("isatom", 1, fnIsAtom);
    add("islist", 1, fnIsList);
    // eval требует текущее окружение и обрабатывается в Interpreter::apply.
    add("eval", 1, nullptr);
}

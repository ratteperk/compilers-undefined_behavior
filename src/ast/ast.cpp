#include "ast.hpp"
#include <sstream>

Node *ASTContext::makeNode(Kind k, int line, int col) {
    pool.push_back(std::make_unique<Node>());
    Node *n = pool.back().get();
    n->kind = k;
    n->line = line;
    n->col = col;
    return n;
}

Node *ASTContext::makeInt(long long val, int line, int col) {
    Node *n = makeNode(Kind::Int, line, col);
    n->i = val;
    return n;
}

Node *ASTContext::makeReal(double val, int line, int col) {
    Node *n = makeNode(Kind::Real, line, col);
    n->r = val;
    return n;
}

Node *ASTContext::makeBool(bool val, int line, int col) {
    Node *n = makeNode(Kind::Bool, line, col);
    n->b = val;
    return n;
}

Node *ASTContext::makeNull(int line, int col) { return makeNode(Kind::Null, line, col); }

Node *ASTContext::makeAtom(std::string name, int line, int col) {
    Node *n = makeNode(Kind::Atom, line, col);
    n->name = std::move(name);
    return n;
}

Node *ASTContext::makeList(std::vector<Node *> items, int line, int col) {
    Node *n = makeNode(Kind::List, line, col);
    n->items = std::move(items);
    return n;
}

Node *ASTContext::clone(const Node *node) {
    if (!node)
        return nullptr;
    Node *res = makeNode(node->kind, node->line, node->col);
    res->name = node->name;
    res->i = node->i;
    res->r = node->r;
    res->b = node->b;
    for (const Node *child : node->items) {
        res->items.push_back(clone(child));
    }
    return res;
}

std::string kindToString(Kind k) {
    switch (k) {
    case Kind::Atom: return "Atom";
    case Kind::Int:  return "Int";
    case Kind::Real: return "Real";
    case Kind::Bool: return "Bool";
    case Kind::Null: return "Null";
    case Kind::List: return "List";
    }
    return "Unknown";
}

std::string nodeToString(const Node *n) {
    if (!n)
        return "null";
    switch (n->kind) {
    case Kind::Atom:
        return n->name;
    case Kind::Int:
        return std::to_string(n->i);
    case Kind::Real: {
        std::ostringstream ss;
        ss << n->r;
        return ss.str();
    }
    case Kind::Bool:
        return n->b ? "true" : "false";
    case Kind::Null:
        return "null";
    case Kind::List: {
        if (n->items.size() == 2 && n->items[0]->kind == Kind::Atom && n->items[0]->name == "quote") {
            return "'" + nodeToString(n->items[1]);
        }
        std::string out = "(";
        for (size_t idx = 0; idx < n->items.size(); ++idx) {
            if (idx > 0)
                out += " ";
            out += nodeToString(n->items[idx]);
        }
        out += ")";
        return out;
    }
    }
    return "";
}

void dumpTree(const Node *n, int depth) {
    if (!n)
        return;
    std::string pad(depth * 2, ' ');
    switch (n->kind) {
    case Kind::Atom:
        std::cout << pad << "Atom " << n->name << "\n";
        break;
    case Kind::Int:
        std::cout << pad << "Int " << n->i << "\n";
        break;
    case Kind::Real:
        std::cout << pad << "Real " << n->r << "\n";
        break;
    case Kind::Bool:
        std::cout << pad << "Bool " << (n->b ? "true" : "false") << "\n";
        break;
    case Kind::Null:
        std::cout << pad << "Null\n";
        break;
    case Kind::List:
        std::cout << pad << "List (" << n->items.size() << ")\n";
        for (const Node *c : n->items)
            dumpTree(c, depth + 1);
        break;
    }
}

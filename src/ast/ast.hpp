#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

enum class Kind { Atom, Int, Real, Bool, Null, List };

struct Node {
    Kind kind;
    std::string name;          // Atom
    long long i = 0;           // Int
    double r = 0;              // Real
    bool b = false;            // Bool
    std::vector<Node *> items; // List
    int line = 0, col = 0;

    bool isLiteral() const {
        return kind == Kind::Int || kind == Kind::Real || kind == Kind::Bool || kind == Kind::Null;
    }
};

class ASTContext {
  public:
    std::vector<std::unique_ptr<Node>> pool;

    Node *makeNode(Kind k, int line = 0, int col = 0);
    Node *makeInt(long long val, int line = 0, int col = 0);
    Node *makeReal(double val, int line = 0, int col = 0);
    Node *makeBool(bool val, int line = 0, int col = 0);
    Node *makeNull(int line = 0, int col = 0);
    Node *makeAtom(std::string name, int line = 0, int col = 0);
    Node *makeList(std::vector<Node *> items = {}, int line = 0, int col = 0);

    Node *clone(const Node *node);
};

std::string kindToString(Kind k);
std::string nodeToString(const Node *n);
void dumpTree(const Node *n, int depth = 0);

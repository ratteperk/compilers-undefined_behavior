#pragma once

#include "ast.hpp"

class ASTOptimizer {
public:
    explicit ASTOptimizer(ASTContext& ctx) : ctx(ctx) {}

    Node* optimize(Node* root);
    bool hasChanged() const { return changed; }

private:
    ASTContext& ctx;
    bool changed = false;

    Node* pass(Node* node);
    Node* foldNode(Node* node);
    Node* foldBuiltin(Node* node, const std::string& op);

    static bool isPure(const Node* node);
};

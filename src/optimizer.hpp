#pragma once

#include "ast.hpp"

class ASTOptimizer {
public:
    explicit ASTOptimizer(ASTContext& ctx) : ctx(ctx) {}

    // Runs optimization passes until fixed-point (tree stops changing)
    Node* optimize(Node* root);

    bool hasChanged() const { return changed; }

private:
    ASTContext& ctx;
    bool changed = false;

    Node* pass(Node* node);
    Node* foldNode(Node* node);
    Node* foldBuiltin(Node* node, const std::string& op);

    // Helpers
    static bool isPure(const Node* node);
    Node* makeFoldedNumber(double val, bool isIntResult, int line, int col);
};
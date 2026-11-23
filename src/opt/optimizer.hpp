#pragma once

#include "ast.hpp"
#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

class ASTOptimizer {
public:
    explicit ASTOptimizer(ASTContext& ctx) : ctx(ctx) {}

    Node* optimize(Node* root);
    bool hasChanged() const { return changed; }

private:
    struct Scope {
        std::unordered_set<std::string> names;
        bool isFunctionBoundary = false;
    };

    ASTContext& ctx;
    bool changed = false;
    std::vector<Scope> scopes;

    void enterScope(bool isFunctionBoundary);
    void exitScope();
    void declareLocal(const std::string& name);
    void defineInFunctionScope(const std::string& name);
    bool isShadowed(const std::string& name) const;

    void registerFunctionsInBlock(const Node* listNode, std::size_t startIdx);
    void collectSetqTargets(const Node* node);

    Node* pass(Node* node);
    Node* passBlock(Node* listNode, std::size_t startIdx, bool allowReturnTrim);
    Node* foldNode(Node* node);
    Node* foldBuiltin(Node* node, const std::string& op);

    bool isPure(const Node* node) const;
    bool isGuaranteedBool(const Node* node) const;
    static bool staticKind(const Node* node, Kind& out);
};
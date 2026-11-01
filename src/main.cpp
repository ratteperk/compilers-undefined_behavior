#include "analyzer.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include "optimizer.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "parser.hpp"

static const int EX_OK = 0;
static const int EX_USAGE = 64;
static const int EX_DATAERR = 65;  // Lexical, Syntax, or Semantic Error
static const int EX_NOINPUT = 66;

struct Options {
    bool parseOnly = false;
    bool checkOnly = false;
    bool dumpTreeMode = false;
    std::string filename;
};

Options parseArgs(int argc, char* argv[]) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--parse-only") {
            opt.parseOnly = true;
        } else if (arg == "--check-only") {
            opt.checkOnly = true;
        } else if (arg == "--dump-tree") {
            opt.dumpTreeMode = true;
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "Unknown option: " << arg << "\n";
            std::exit(EX_USAGE);
        } else if (opt.filename.empty()) {
            opt.filename = arg;
        } else {
            std::cerr << "Too many input files specified.\n";
            std::exit(EX_USAGE);
        }
    }
    return opt;
}

int main(int argc, char* argv[]) {
    Options opt = parseArgs(argc, argv);

    std::string source;
    if (!opt.filename.empty()) {
        std::ifstream file(opt.filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error: cannot open '" << opt.filename << "'.\n";
            return EX_NOINPUT;
        }
        source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    } else {
        source.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    }

    // 1. Lexical Analysis
    Lexer lexer(std::move(source));
    std::vector<Token> tokens = lexer.scanTokens();
    if (lexer.hadError()) {
        return EX_DATAERR;
    }

    // 2. Syntax Analysis (Bison)
    ASTContext ctx;
    Node* root = nullptr;
    if (!parseProgram(tokens, ctx, root)) {
        return EX_DATAERR;
    }

    // In parse-only mode, stop right after AST construction
    if (opt.parseOnly) {
        if (opt.dumpTreeMode)
            dumpTree(root);
        return EX_OK;
    }

    // 3. Semantic Analysis (Read-Only Passes)
    SemanticAnalyzer analyzer(ctx);
    if (!analyzer.analyze(root)) {
        analyzer.printErrors();
        return EX_DATAERR;
    }

    if (opt.checkOnly) {
        return EX_OK;
    }

    // 4. AST Optimization (Mutating Passes)
    ASTOptimizer optimizer(ctx);
    Node* optRoot = optimizer.optimize(root);

    // 5. Output
    if (opt.dumpTreeMode) {
        dumpTree(optRoot);
    } else {
        for (const Node* expr : optRoot->items) {
            std::cout << nodeToString(expr) << "\n";
        }
    }

    return EX_OK;
}
#include "lexer.hpp"


// Helper to print tokens for debugging
std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::QUOTE: return "QUOTE";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::REAL: return "REAL";
        case TokenType::BOOLEAN: return "BOOLEAN";
        case TokenType::NULL_LITERAL: return "NULL_LITERAL";
        case TokenType::EOF_TOKEN: return "EOF";
        default: return "UNKNOWN";
    }
}

void printToken(const Token& token) {
    std::cout << token.line << ":" << token.column << "\t"
              << tokenTypeToString(token.type) << "\t" << token.lexeme;

    if (std::holds_alternative<long long>(token.literal)) {
        std::cout << "\t" << std::get<long long>(token.literal);
    } else if (std::holds_alternative<double>(token.literal)) {
        std::cout << "\t" << std::get<double>(token.literal);
    } else if (std::holds_alternative<bool>(token.literal)) {
        std::cout << "\t" << (std::get<bool>(token.literal) ? "true" : "false");
    }
    std::cout << "\n";
}

// ==========================================
// main (test)
// ==========================================

// implementations exit code: 64 usage, 65 bad input data, 66 unreadable input file.
static const int EX_USAGE   = 64;
static const int EX_DATAERR = 65;
static const int EX_NOINPUT = 66;

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [script.f]\n"
                  << "       reads standard input when no file is given\n";
        return EX_USAGE;
    }

    std::string source;
    if (argc == 2) {
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) {
            std::cerr << "Error: could not open '" << argv[1] << "'.\n";
            return EX_NOINPUT;
        }
        source.assign(std::istreambuf_iterator<char>(file),
                      std::istreambuf_iterator<char>());
    } else {
        source.assign(std::istreambuf_iterator<char>(std::cin),
                      std::istreambuf_iterator<char>());
    }

    Lexer lexer(std::move(source));
    std::vector<Token> tokens = lexer.scanTokens();

    for (const auto& token : tokens) {
        printToken(token);
    }

    return lexer.hadError() ? EX_DATAERR : 0;
}

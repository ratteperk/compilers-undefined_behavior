#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <cctype>
#include <stdexcept>

// ==========================================
// Token Definitions
// ==========================================

enum class TokenType {
    // Single-character tokens
    LEFT_PAREN, RIGHT_PAREN, QUOTE,

    // Literals & Identifiers
    IDENTIFIER, INTEGER, REAL, BOOLEAN, NULL_LITERAL,

    // End of file
    EOF_TOKEN
};

// Using std::variant to hold the actual parsed value of the literal
using TokenLiteral = std::variant<int, double, bool, std::monostate>;

struct Token {
    TokenType type;
    std::string lexeme;
    TokenLiteral literal;
    int line;

    Token(TokenType type, std::string lexeme, TokenLiteral literal, int line)
        : type(type), lexeme(std::move(lexeme)), literal(literal), line(line) {}
};

// ==========================================
// Lexer Class
// ==========================================

class Lexer {
public:
    explicit Lexer(std::string source) : source(std::move(source)) {}

    std::vector<Token> scanTokens() {
        while (!isAtEnd()) {
            start = current;
            scanToken();
        }

        tokens.emplace_back(TokenType::EOF_TOKEN, "", std::monostate{}, line);
        return tokens;
    }

private:
    std::string source;
    std::vector<Token> tokens;
    int start = 0;
    int current = 0;
    int line = 1;

    // Map for keywords that have specific literal types
    static const std::unordered_map<std::string, TokenType> keywords;

    // --- Helper Methods ---

    bool isAtEnd() const { return current >= source.length(); }
    
    char advance() { return source[current++]; }
    
    char peek() const {
        if (isAtEnd()) return '\0';
        return source[current];
    }
    
    char peekNext() const {
        if (current + 1 >= source.length()) return '\0';
        return source[current + 1];
    }
    
    bool match(char expected) {
        if (isAtEnd()) return false;
        if (source[current] != expected) return false;
        current++;
        return true;
    }

    void addToken(TokenType type) { addToken(type, std::monostate{}); }
    
    void addToken(TokenType type, TokenLiteral literal) {
        std::string text = source.substr(start, current - start);
        tokens.emplace_back(type, text, literal, line);
    }

    bool isAlpha(char c) const {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; 
    }
    
    bool isDigit(char c) const { return c >= '0' && c <= '9'; }
    
    bool isAlphaNumeric(char c) const { return isAlpha(c) || isDigit(c); }

    void error(const std::string& message) {
        std::cerr << "[Line " << line << "] Error: " << message << "\n";
    }

    // --- Scanning Methods ---

    void scanToken() {
        char c = peek();
        
        if (c == '+' || c == '-' || isDigit(c)) {
            scanNumber();
            return;
        }

        c = advance();
        switch (c) {
            case '(': addToken(TokenType::LEFT_PAREN); break;
            case ')': addToken(TokenType::RIGHT_PAREN); break;
            case '\'': addToken(TokenType::QUOTE); break;
            
            case '/':
                if (peek() == '/') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    error("Unexpected character '/'");
                }
                break;

            case ' ':
            case '\r':
            case '\t':
                break;

            case '\n':
                line++;
                break;

            default:
                if (isAlpha(c)) {
                    // We already advanced past the first letter, but `start` is still 
                    // at the beginning of the identifier, so scanIdentifier() will 
                    // correctly grab the rest of it.
                    scanIdentifier();
                } else {
                    error("Unexpected character.");
                }
                break;
        }
    }

    void scanNumber() {
        // Consume optional sign
        if (peek() == '+' || peek() == '-') {
            advance();
        }

        // Grammar requires at least one digit after an optional sign
        if (!isDigit(peek())) {
            error("Malformed number literal. Expected digit after sign.");
            return;
        }

        while (isDigit(peek())) advance();

        // Look for a fractional part. 
        // Grammar: Real: Integer. Integer (meaning "3." is invalid, must be "3.0")
        if (peek() == '.' && isDigit(peekNext())) {
            advance(); // consume the '.'
            while (isDigit(peek())) advance();
            
            std::string numStr = source.substr(start, current - start);
            addToken(TokenType::REAL, std::stod(numStr));
        } else {
            std::string numStr = source.substr(start, current - start);
            addToken(TokenType::INTEGER, std::stoi(numStr));
        }
    }

    void scanIdentifier() {
        while (isAlphaNumeric(peek())) advance();

        std::string text = source.substr(start, current - start);
        
        // Check if it's a keyword (true, false, null)
        auto it = keywords.find(text);
        if (it != keywords.end()) {
            TokenType type = it->second;
            if (type == TokenType::BOOLEAN) {
                addToken(type, text == "true");
            } else if (type == TokenType::NULL_LITERAL) {
                addToken(type, std::monostate{});
            }
        } else {
            addToken(TokenType::IDENTIFIER, std::monostate{});
        }
    }
};

// Initialize static keywords map
const std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"true",  TokenType::BOOLEAN},
    {"false", TokenType::BOOLEAN},
    {"null",  TokenType::NULL_LITERAL}
};

// ==========================================
// Helper to print tokens for debugging
// ==========================================
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

// ==========================================
// Main / Testing
// ==========================================
int main() {
    std::string source = R"(
        (setq x (plus 1 2)) // x gets the value of 3
        (setq y -3.14)
        (setq z '(plus minus times))
        (func Cube (arg) (times (times arg arg) arg))
        (cond (less x 0) true false)
    )";

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scanTokens();

    std::cout << "--- Project F Lexer Output ---\n";
    for (const auto& token : tokens) {
        std::cout << "Line " << token.line << " | " 
                  << tokenTypeToString(token.type) << " | " 
                  << token.lexeme;
        
        if (std::holds_alternative<int>(token.literal)) {
            std::cout << " [Value: " << std::get<int>(token.literal) << "]";
        } else if (std::holds_alternative<double>(token.literal)) {
            std::cout << " [Value: " << std::get<double>(token.literal) << "]";
        } else if (std::holds_alternative<bool>(token.literal)) {
            std::cout << " [Value: " << (std::get<bool>(token.literal) ? "true" : "false") << "]";
        }
        std::cout << "\n";
    }

    return 0;
}
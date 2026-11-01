#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

enum class TokenType {
    LEFT_PAREN,
    RIGHT_PAREN,
    QUOTE,
    IDENTIFIER,
    INTEGER,
    REAL,
    BOOLEAN,
    NULL_LITERAL,
    EOF_TOKEN
};

using TokenLiteral = std::variant<long long, double, bool, std::monostate>;

struct Token {
    TokenType type;
    std::string lexeme;
    TokenLiteral literal;
    int line;
    int column;

    Token(TokenType type, std::string lexeme, TokenLiteral literal, int line, int column);
};

std::string tokenTypeToString(TokenType type);
void printToken(const Token &token);

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> scanTokens();
    bool hadError() const { return errorFlag; }

private:
    std::string source;
    std::vector<Token> tokens;
    std::size_t start = 0;
    std::size_t current = 0;
    std::size_t lineStart = 0;
    int line = 1;
    bool errorFlag = false;

    static const std::unordered_map<std::string, TokenType> keywords;

    bool isAtEnd() const;
    char advance();
    char peek() const;
    char peekNext() const;
    int columnOf(std::size_t pos) const;

    void addToken(TokenType type);
    void addToken(TokenType type, TokenLiteral literal);

    bool isAlpha(char c) const;
    bool isNonAscii(char c) const;
    bool isDigit(char c) const;
    bool isAlphaNumeric(char c) const;
    bool isDelimiter(char c) const;

    void error(const std::string& message);
    void error(int column, const std::string& message);

    void scanToken();
    bool consumeIfGlued(const char* what);
    void scanNonAscii();
    void scanNumber();
    void scanIdentifier();
};

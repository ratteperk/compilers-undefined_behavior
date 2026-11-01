#include "lexer.hpp"
#include <iostream>
#include <stdexcept>

const std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"true", TokenType::BOOLEAN}, {"false", TokenType::BOOLEAN}, {"null", TokenType::NULL_LITERAL}};

Token::Token(TokenType type, std::string lexeme, TokenLiteral literal, int line, int column)
    : type(type), lexeme(std::move(lexeme)), literal(literal), line(line), column(column) {}

std::string tokenTypeToString(TokenType type) {
    switch (type) {
    case TokenType::LEFT_PAREN:   return "LEFT_PAREN";
    case TokenType::RIGHT_PAREN:  return "RIGHT_PAREN";
    case TokenType::QUOTE:        return "QUOTE";
    case TokenType::IDENTIFIER:   return "IDENTIFIER";
    case TokenType::INTEGER:      return "INTEGER";
    case TokenType::REAL:         return "REAL";
    case TokenType::BOOLEAN:      return "BOOLEAN";
    case TokenType::NULL_LITERAL: return "NULL_LITERAL";
    case TokenType::EOF_TOKEN:    return "EOF";
    default:                      return "UNKNOWN";
    }
}

void printToken(const Token &token) {
    std::cout << token.line << ":" << token.column << "\t" << tokenTypeToString(token.type) << "\t" << token.lexeme;
    if (std::holds_alternative<long long>(token.literal)) {
        std::cout << "\t" << std::get<long long>(token.literal);
    } else if (std::holds_alternative<double>(token.literal)) {
        std::cout << "\t" << std::get<double>(token.literal);
    } else if (std::holds_alternative<bool>(token.literal)) {
        std::cout << "\t" << (std::get<bool>(token.literal) ? "true" : "false");
    }
    std::cout << "\n";
}

Lexer::Lexer(std::string source) : source(std::move(source)) {}

std::vector<Token> Lexer::scanTokens() {
    while (!isAtEnd()) {
        start = current;
        scanToken();
    }
    tokens.emplace_back(TokenType::EOF_TOKEN, "", std::monostate{}, line, columnOf(current));
    return tokens;
}

bool Lexer::isAtEnd() const { return current >= source.length(); }

char Lexer::advance() { return source[current++]; }

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

int Lexer::columnOf(std::size_t pos) const {
    int col = 1;
    for (std::size_t i = lineStart; i < pos && i < source.length(); ++i) {
        if ((static_cast<unsigned char>(source[i]) & 0xC0) != 0x80)
            col++;
    }
    return col;
}

void Lexer::addToken(TokenType type) { addToken(type, std::monostate{}); }

void Lexer::addToken(TokenType type, TokenLiteral literal) {
    std::string text = source.substr(start, current - start);
    tokens.emplace_back(type, text, literal, line, columnOf(start));
}

bool Lexer::isAlpha(char c) const { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool Lexer::isNonAscii(char c) const { return static_cast<unsigned char>(c) >= 0x80; }
bool Lexer::isDigit(char c) const { return c >= '0' && c <= '9'; }
bool Lexer::isAlphaNumeric(char c) const { return isAlpha(c) || isDigit(c); }
bool Lexer::isDelimiter(char c) const {
    return c == '\0' || c == '(' || c == ')' || c == '\'' || c == '/' || c == ' ' ||
           c == '\t' || c == '\r' || c == '\n';
}

void Lexer::error(const std::string& message) { error(columnOf(start), message); }

void Lexer::error(int column, const std::string& message) {
    errorFlag = true;
    std::cerr << "[line " << line << ":" << column << "] Error: " << message << "\n";
}

void Lexer::scanToken() {
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
            while (peek() != '\n' && !isAtEnd())
                advance();
        } else {
            error("Unexpected character '/'.");
        }
        break;
    case ' ':
    case '\r':
    case '\t':
        break;
    case '\n':
        line++;
        lineStart = current;
        break;
    default:
        if (isAlpha(c)) {
            scanIdentifier();
        } else if (isNonAscii(c)) {
            scanNonAscii();
        } else {
            while (!isAtEnd() && !isDelimiter(peek()))
                advance();
            std::string run = source.substr(start, current - start);
            if (run.length() == 1) {
                error("Unexpected character '" + run + "'.");
            } else {
                error("Unexpected character '" + std::string(1, c) + "' in '" + run + "'.");
            }
        }
        break;
    }
}

bool Lexer::consumeIfGlued(const char* what) {
    if (isDelimiter(peek())) return false;
    while (!isAtEnd() && !isDelimiter(peek()))
        advance();
    error(std::string("Malformed ") + what + " '" + source.substr(start, current - start) +
          "': elements must be separated by whitespace.");
    return true;
}

void Lexer::scanNonAscii() {
    while (!isAtEnd() && !isDelimiter(peek()))
        advance();
    error("Non-ASCII character in '" + source.substr(start, current - start) +
          "': identifiers must use ASCII letters A-Z and a-z.");
}

void Lexer::scanNumber() {
    if (peek() == '+' || peek() == '-') {
        advance();
    }
    if (!isDigit(peek())) {
        error("Malformed number literal: expected a digit after the sign.");
        return;
    }
    while (isDigit(peek()))
        advance();

    bool isReal = false;
    if (peek() == '.' && isDigit(peekNext())) {
        isReal = true;
        advance();
        while (isDigit(peek()))
            advance();
    }

    if (consumeIfGlued("number literal")) return;

    std::string numStr = source.substr(start, current - start);
    try {
        if (isReal) {
            addToken(TokenType::REAL, std::stod(numStr));
        } else {
            addToken(TokenType::INTEGER, std::stoll(numStr));
        }
    } catch (const std::out_of_range&) {
        error("Number literal '" + numStr + "' is out of range.");
    } catch (const std::invalid_argument&) {
        error("Number literal '" + numStr + "' could not be parsed.");
    }
}

void Lexer::scanIdentifier() {
    while (isAlphaNumeric(peek()))
        advance();

    if (consumeIfGlued("identifier")) return;

    std::string text = source.substr(start, current - start);
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        TokenType type = it->second;
        if (type == TokenType::BOOLEAN) {
            addToken(type, text == "true");
        } else {
            addToken(type);
        }
    } else {
        addToken(TokenType::IDENTIFIER);
    }
}

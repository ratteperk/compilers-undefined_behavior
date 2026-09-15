#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <cctype>
#include <cstddef>
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

// Using std::variant to hold the actual parsed value of the literal.
// Integers are stored as long long: the grammar puts no bound on Integer,
// so the widest built-in type keeps more programs representable.
using TokenLiteral = std::variant<long long, double, bool, std::monostate>;

struct Token {
    TokenType type;
    std::string lexeme;
    TokenLiteral literal;
    int line;
    int column;

    Token(TokenType type, std::string lexeme, TokenLiteral literal, int line, int column)
        : type(type), lexeme(std::move(lexeme)), literal(literal), line(line), column(column) {}
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

        tokens.emplace_back(TokenType::EOF_TOKEN, "", std::monostate{}, line, columnOf(current));
        return tokens;
    }

    bool hadError() const { return errorFlag; }

private:
    std::string source;
    std::vector<Token> tokens;
    std::size_t start = 0;
    std::size_t current = 0;
    std::size_t lineStart = 0;   // byte offset of the first character of `line`
    int line = 1;
    bool errorFlag = false;

    // Map for keywords that have specific literal types.
    // Special-form keywords (quote, setq, func, ...) are deliberately absent:
    // the spec (p. 4) makes a name special only in head-of-list position, so
    // telling them apart is the parser's job, not the lexer's.
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

    // 1-based column of a byte offset, counted in characters rather than bytes
    // so that a multi-byte UTF-8 identifier does not skew error positions.
    int columnOf(std::size_t pos) const {
        int col = 1;
        for (std::size_t i = lineStart; i < pos && i < source.length(); ++i) {
            if ((static_cast<unsigned char>(source[i]) & 0xC0) != 0x80) col++;
        }
        return col;
    }

    void addToken(TokenType type) { addToken(type, std::monostate{}); }

    void addToken(TokenType type, TokenLiteral literal) {
        std::string text = source.substr(start, current - start);
        tokens.emplace_back(type, text, literal, line, columnOf(start));
    }

    // Grammar: Letter : Any Unicode character that represents a letter.
    // The source is handled as raw UTF-8 bytes, so every non-ASCII byte
    // (lead byte or continuation) is accepted as part of a letter. This admits
    // a few non-letter code points that full Unicode classification would
    // reject, but it keeps identifiers such as `переменная` intact without
    // pulling in ICU.
    bool isAlpha(char c) const {
        unsigned char uc = static_cast<unsigned char>(c);
        return (uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z') || uc >= 0x80;
    }

    bool isDigit(char c) const { return c >= '0' && c <= '9'; }

    bool isAlphaNumeric(char c) const { return isAlpha(c) || isDigit(c); }

    // Elements are "separated by whitespaces and enclosed by parentheses"
    // (spec, p. 1). Anything else directly after a literal or an identifier
    // means two elements were glued together.
    bool isDelimiter(char c) const {
        return c == '\0' || c == '(' || c == ')' || c == '\'' || c == '/' ||
               c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    void error(const std::string& message) { error(columnOf(start), message); }

    void error(int column, const std::string& message) {
        errorFlag = true;
        std::cerr << "[line " << line << ":" << column << "] Error: " << message << "\n";
    }

    // --- Scanning Methods ---

    void scanToken() {
        char c = peek();

        // A sign can only ever open a literal: the grammar gives identifiers no
        // way to start with '+' or '-'.
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
                    // We already advanced past the first letter, but `start` is still
                    // at the beginning of the identifier, so scanIdentifier() will
                    // correctly grab the rest of it.
                    scanIdentifier();
                } else {
                    error(std::string("Unexpected character '") + c + "'.");
                }
                break;
        }
    }

    // Consumes a run of characters that should have been separated from the
    // token just scanned. Reports one error for the whole run and drops it,
    // rather than emitting a plausible-looking token stream for broken input.
    bool consumeIfGlued(const char* what) {
        if (isDelimiter(peek())) return false;

        while (!isAtEnd() && !isDelimiter(peek())) advance();
        error(std::string("Malformed ") + what + " '" + source.substr(start, current - start) +
              "': elements must be separated by whitespace.");
        return true;
    }

    void scanNumber() {
        // Consume optional sign
        if (peek() == '+' || peek() == '-') {
            advance();
        }

        // Grammar requires at least one digit after an optional sign
        if (!isDigit(peek())) {
            error("Malformed number literal: expected a digit after the sign.");
            return;
        }

        while (isDigit(peek())) advance();

        // Look for a fractional part.
        // Grammar: Real : Integer . Integer (meaning "3." is invalid, must be "3.0")
        bool isReal = false;
        if (peek() == '.' && isDigit(peekNext())) {
            isReal = true;
            advance(); // consume the '.'
            while (isDigit(peek())) advance();
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

    void scanIdentifier() {
        while (isAlphaNumeric(peek())) advance();

        if (consumeIfGlued("identifier")) return;

        std::string text = source.substr(start, current - start);

        // Check if it's a keyword (true, false, null)
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
// Entry point
// ==========================================

// Exit codes follow the sysexits.h convention also used by the reference
// implementations: 64 usage, 65 bad input data, 66 unreadable input file.
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

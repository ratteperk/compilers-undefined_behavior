/* Project F -- syntax analyzer (Bison).
 *
 * Build:
 *     bison -d -o parser.cpp parser.y
 *     g++ -std=c++17 -O2 -o fparser parser.cpp
 *
 * Run:
 *     ./fparser tests/03_math.f     (or: ./fparser < tests/03_math.f)
 */

/* ---- goes into parser.hpp, so the value type is visible there ---- */
%code requires {
    #include <string>
    #include <vector>

    enum class Kind { Atom, Int, Real, Bool, Null, List };

    struct Node {
        Kind kind;
        std::string name;            // Atom
        long long   i = 0;           // Int
        double      r = 0;           // Real
        bool        b = false;       // Bool
        std::vector<Node*> items;    // List
        int line = 0, col = 0;
    };
}

/* ---- goes into parser.cpp, before the parser itself ---- */
%code {
    #include "lexer.hpp"
    #include <fstream>
    #include <iostream>
    #include <iterator>
    #include <memory>

    int  yylex(void);
    void yyerror(const char* msg);

    /* Nodes live until the program exits; a parse error can unwind at any
     * point, so individual ownership is not worth the trouble here. */
    static std::vector<std::unique_ptr<Node>> g_nodes;

    static Node* newNode(Kind k, int line = 0, int col = 0) {
        g_nodes.push_back(std::make_unique<Node>());
        Node* n = g_nodes.back().get();
        n->kind = k; n->line = line; n->col = col;
        return n;
    }

    static std::vector<Token> g_tokens;   // filled by the lexer in main()
    static std::size_t        g_pos = 0;
    static Node*              g_root = nullptr;
}

/* Every grammar symbol carries the same type, so no %union is needed. */
%define api.value.type {Node*}
%define parse.error verbose

%token ATOM INT REAL BOOL NULLVAL LPAREN RPAREN QUOTE
%start Program

%%

/* Grammar of the F language, spec p. 6. EBNF repetition `{ Element }` has no
 * Bison equivalent, so it becomes left recursion. */

Program
    : Elements                  { g_root = $1; }
    ;

/* An Elements node IS the list node -- that is what removes all the
 * bookkeeping: the rules below just keep pushing into it. */
Elements
    : Element                   { $$ = newNode(Kind::List, $1->line, $1->col);
                                  $$->items.push_back($1); }
    | Elements Element          { $$ = $1; $$->items.push_back($2); }
    ;

Element
    : ATOM                      { $$ = $1; }
    | INT                       { $$ = $1; }
    | REAL                      { $$ = $1; }
    | BOOL                      { $$ = $1; }
    | NULLVAL                   { $$ = $1; }
    | List                      { $$ = $1; }
    | QUOTE Element             { /* 'E is short for (quote E), spec p. 2 */
                                  $$ = newNode(Kind::List, $2->line, $2->col);
                                  Node* q = newNode(Kind::Atom, $2->line, $2->col);
                                  q->name = "quote";
                                  $$->items.push_back(q);
                                  $$->items.push_back($2); }
    ;

List
    : LPAREN Elements RPAREN    { $$ = $2; }
    | LPAREN RPAREN             { /* spec's grammar forbids (), its examples
                                   * use it -- see compilers.md */
                                  $$ = newNode(Kind::List); }
    ;

%%

/* ---- the bridge to the hand-written lexer -------------------------------
 * Bison needs exactly one thing: a yylex() returning an integer token code.
 * Flex is just one way to write it; here we hand over tokens the lexer has
 * already produced. The value travels separately, through yylval. */

int yylex(void) {
    if (g_pos >= g_tokens.size()) return 0;      /* 0 means end of input */
    const Token& t = g_tokens[g_pos++];

    switch (t.type) {
        case TokenType::LEFT_PAREN:  return LPAREN;
        case TokenType::RIGHT_PAREN: return RPAREN;
        case TokenType::QUOTE:       return QUOTE;
        case TokenType::EOF_TOKEN:   return 0;

        case TokenType::IDENTIFIER:
            yylval = newNode(Kind::Atom, t.line, t.column);
            yylval->name = t.lexeme;
            return ATOM;
        case TokenType::INTEGER:
            yylval = newNode(Kind::Int, t.line, t.column);
            yylval->i = std::get<long long>(t.literal);
            return INT;
        case TokenType::REAL:
            yylval = newNode(Kind::Real, t.line, t.column);
            yylval->r = std::get<double>(t.literal);
            return REAL;
        case TokenType::BOOLEAN:
            yylval = newNode(Kind::Bool, t.line, t.column);
            yylval->b = std::get<bool>(t.literal);
            return BOOL;
        case TokenType::NULL_LITERAL:
            yylval = newNode(Kind::Null, t.line, t.column);
            return NULLVAL;
    }
    return 0;
}

void yyerror(const char* msg) {
    /* Bison does not track positions; we take them from the token that could
     * not be shifted (g_pos has already moved past it). */
    if (g_pos > 0 && g_pos - 1 < g_tokens.size()) {
        const Token& t = g_tokens[g_pos - 1];
        std::cerr << "[line " << t.line << ":" << t.column << "] " << msg
                  << " (at '"
                  << (t.type == TokenType::EOF_TOKEN ? "end of input" : t.lexeme)
                  << "')\n";
    } else {
        std::cerr << msg << "\n";
    }
}

/* ---- driver ------------------------------------------------------------ */

static void dump(const Node* n, int depth) {
    std::string pad(depth * 2, ' ');
    switch (n->kind) {
        case Kind::Atom: std::cout << pad << "Atom " << n->name << "\n"; break;
        case Kind::Int:  std::cout << pad << "Int "  << n->i    << "\n"; break;
        case Kind::Real: std::cout << pad << "Real " << n->r    << "\n"; break;
        case Kind::Bool: std::cout << pad << "Bool " << (n->b ? "true" : "false") << "\n"; break;
        case Kind::Null: std::cout << pad << "Null\n"; break;
        case Kind::List:
            std::cout << pad << "List (" << n->items.size() << ")\n";
            for (const Node* c : n->items) dump(c, depth + 1);
            break;
    }
}

int main(int argc, char* argv[]) {
    std::string source;
    if (argc == 2) {
        std::ifstream f(argv[1], std::ios::binary);
        if (!f) { std::cerr << "Error: cannot open '" << argv[1] << "'.\n"; return 66; }
        source.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    } else if (argc == 1) {
        source.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    } else {
        std::cerr << "Usage: " << argv[0] << " [script.f]\n";
        return 64;
    }

    Lexer lexer(std::move(source));
    g_tokens = lexer.scanTokens();
    if (lexer.hadError()) return 65;      /* never parse a broken token stream */

    if (yyparse() != 0) return 65;

    dump(g_root, 0);
    return 0;
}

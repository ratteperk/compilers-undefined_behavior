/* Project F -- Grammar and Bison parser bridge */

%code requires {
    #include "ast.hpp"
    #include "lexer.hpp"
    #include <vector>

    // Public parser API
    bool parseProgram(const std::vector<Token>& tokens, ASTContext& ctx, Node*& outRoot);
}

%code {
    #include "parser.hpp"
    #include <iostream>

    int  yylex(void);
    void yyerror(const char* msg);

    static const std::vector<Token>* g_tokens = nullptr;
    static std::size_t               g_pos = 0;
    static ASTContext*               g_ctx = nullptr;
    static Node*                     g_root = nullptr;
}

%define api.value.type {Node*}
%define parse.error verbose

%token ATOM INT REAL BOOL NULLVAL LPAREN RPAREN QUOTE
%start Program

%%

Program
    : Elements                  { g_root = $1; }
    ;

Elements
    : Element                   { $$ = g_ctx->makeList({$1}, $1->line, $1->col); }
    | Elements Element          { $$ = $1; $$->items.push_back($2); }
    ;

Element
    : ATOM                      { $$ = $1; }
    | INT                       { $$ = $1; }
    | REAL                      { $$ = $1; }
    | BOOL                      { $$ = $1; }
    | NULLVAL                   { $$ = $1; }
    | List                      { $$ = $1; }
    | QUOTE Element             {
                                  Node* q = g_ctx->makeAtom("quote", $2->line, $2->col);
                                  $$ = g_ctx->makeList({q, $2}, $2->line, $2->col);
                                }
    ;

List
    : LPAREN Elements RPAREN    { $$ = $2; }
    | LPAREN RPAREN             { $$ = g_ctx->makeList(); }
    ;

%%

int yylex(void) {
    if (!g_tokens || g_pos >= g_tokens->size()) return 0;
    const Token& t = (*g_tokens)[g_pos++];

    switch (t.type) {
        case TokenType::LEFT_PAREN:  return LPAREN;
        case TokenType::RIGHT_PAREN: return RPAREN;
        case TokenType::QUOTE:       return QUOTE;
        case TokenType::EOF_TOKEN:   return 0;

        case TokenType::IDENTIFIER:
            yylval = g_ctx->makeAtom(t.lexeme, t.line, t.column);
            return ATOM;
        case TokenType::INTEGER:
            yylval = g_ctx->makeInt(std::get<long long>(t.literal), t.line, t.column);
            return INT;
        case TokenType::REAL:
            yylval = g_ctx->makeReal(std::get<double>(t.literal), t.line, t.column);
            return REAL;
        case TokenType::BOOLEAN:
            yylval = g_ctx->makeBool(std::get<bool>(t.literal), t.line, t.column);
            return BOOL;
        case TokenType::NULL_LITERAL:
            yylval = g_ctx->makeNull(t.line, t.column);
            return NULLVAL;
    }
    return 0;
}

void yyerror(const char* msg) {
    if (g_tokens && g_pos > 0 && g_pos - 1 < g_tokens->size()) {
        const Token& t = (*g_tokens)[g_pos - 1];
        std::cerr << "[line " << t.line << ":" << t.column << "] Syntax Error: "
                  << msg << " (at '"
                  << (t.type == TokenType::EOF_TOKEN ? "end of input" : t.lexeme)
                  << "')\n";
    } else {
        std::cerr << "Syntax Error: " << msg << "\n";
    }
}

bool parseProgram(const std::vector<Token>& tokens, ASTContext& ctx, Node*& outRoot) {
    g_tokens = &tokens;
    g_pos = 0;
    g_ctx = &ctx;
    g_root = nullptr;

    int res = yyparse();

    g_tokens = nullptr;
    g_ctx = nullptr;

    if (res == 0) {
        outRoot = g_root;
        return true;
    }
    outRoot = nullptr;
    return false;
}
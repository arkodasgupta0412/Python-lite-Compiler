# Part I - CFG

## Raw CFG

```text
Program      -> StmtList EOF
StmtList     -> Stmt StmtTail
StmtTail     -> SEP Stmt StmtTail | ε
Stmt         -> AssignStmt | PrintStmt | ForStmt | IfStmt
AssignStmt   -> IDENT AssignOp Expr
AssignOp     -> ASSIGN | PLUS_ASSIGN | MINUS_ASSIGN
PrintStmt    -> PRINT LPAREN Expr RPAREN
ForStmt      -> FOR IDENT IN Expr COLON ForBody
ForBody      -> Stmt | NEWLINE INDENT StmtList DEDENT
IfStmt       -> IF Expr COLON NEWLINE INDENT StmtList DEDENT ElseOpt
ElseOpt      -> ELSE COLON NEWLINE INDENT StmtList DEDENT | ε
Expr         -> Expr PLUS Term | Expr MINUS Term | Term
Term         -> Term STAR Factor | Term SLASH Factor | Factor
Factor       -> MINUS Factor | Primary
Primary      -> INT | FLOAT | STRING | BOOL | IDENT | RangeCall
             | LPAREN Expr RPAREN | ListLiteral
RangeCall    -> RANGE LPAREN Expr COMMA Expr RangeStepOpt RPAREN
RangeStepOpt -> COMMA Expr | ε
ListLiteral  -> LBRACKET ElementsOpt RBRACKET
ElementsOpt  -> Expr MoreElements | ε
MoreElements -> COMMA Expr MoreElements | ε
SEP          -> NEWLINE | SEMICOLON
```

## After Left Recursion Removal (core arithmetic fragment)

```text
Expr   -> Term Expr'
Expr'  -> PLUS Term Expr' | MINUS Term Expr' | ε
Term   -> Factor Term'
Term'  -> STAR Factor Term' | SLASH Factor Term' | ε
Factor -> MINUS Factor | Primary
```

Left factoring is then applied automatically in `grammar_tools.py` when useful.

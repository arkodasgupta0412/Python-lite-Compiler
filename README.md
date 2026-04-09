# Modular Mini Compiler Front-End (C++)

This project implements:

1. CFG construction (`docs/CFG.md`, `src/compiler/cfg.cpp`)
2. Lexical analysis (`src/compiler/lexer.cpp`)
3. Symbol table (`src/compiler/symbol_table.cpp`)
4. Top-down parsing + grammar modules (`src/compiler/parser.cpp`, `src/compiler/grammar_tools.cpp`)

For-loop support includes:
- inline loops: `for x in items: print(x)`
- indentation-based blocks with multiple statements
- iteration over strings, lists, and `range(start, end[, step])`

Conditional support includes:
- `if <bool-expr>: ...`
- optional `else: ...`
- indentation-based block bodies

Symbol table uses lexical scopes with shadowing (`enterScope` / `exitScope`).


## Recent Semantic + Symbol Table Behavior

- Symbol table outputs are environment-tree aware:
  - `docs/symbol_table.txt` now prints per-scope sections (`Global Scope`, `Scope Depth N (Child K)`).
  - `docs/symbol_table.svg` includes a `Scope` column and preserves sibling scopes without flattening.
- Semantic analysis is non-aborting:
  - semantic errors are reported, but symbol-table and AST artifacts are still produced when strict parse succeeds.
  - unresolved identifiers are tracked through poison/error typing so later expressions can continue analysis.
- Assignment rule for `name = expr`:
  - if `name` already resolves in scope chain, it is treated as assignment and type-checked only.
  - existing symbol declaration metadata is not overwritten.
  - if `name` does not resolve, it is declared in the current scope.



## Panic-Mode Recovery (LL(1))

This project includes an LL(1) panic-mode recovery pass (`parseLL1WithPanicMode`) that logs recovery actions (`Panic pop` / `Panic scan`) and counts recovered errors.

- Panic-mode actions are written in:
  - `docs/parse_table.txt` (section: `LL(1) Panic-Mode Recovery (Pop/Scan)`)
  - `docs/panic_recovery.svg`
- Output docs are always generated in the `docs` directory next to the executable.
  - Example: running `CD project/cd_frontend.exe` writes to `CD project/docs/*`

### Important behavior

There are two parser passes:

1. LL(1) panic-mode pass: continues and logs recoveries.
2. Strict top-down parser pass: fail-fast, stops at the first syntax error.

So for an invalid file, you may see multiple panic recoveries in reports, but only one strict parse error message in console.


## Design (modular boundaries)

- `include/compiler/io_utils.hpp`, `src/compiler/io_utils.cpp`: input/output classes.
- `include/compiler/tokens.hpp`, `src/compiler/tokens.cpp`: token model.
- `include/compiler/lexer.hpp`, `src/compiler/lexer.cpp`: lexical analyzer.
- `include/compiler/ast_nodes.hpp`, `src/compiler/ast_nodes.cpp`: AST node classes.
- `include/compiler/parser.hpp`, `src/compiler/parser.cpp`: top-down parser.
- `include/compiler/symbol_table.hpp`, `src/compiler/symbol_table.cpp`: symbol-table interface + implementation.
- `include/compiler/semantic.hpp`, `src/compiler/semantic.cpp`: semantic checks and type handling.
- `include/compiler/cfg.hpp`, `src/compiler/cfg.cpp`: raw CFG definition.
- `include/compiler/grammar_tools.hpp`, `src/compiler/grammar_tools.cpp`: left recursion removal, left factoring, FIRST/FOLLOW, parse-table construction.
- `include/compiler/pipeline.hpp`, `src/compiler/pipeline.cpp`: orchestration layer.
- `src/main.cpp`: minimal entrypoint.

To change symbol-table implementation, modify only `src/compiler/symbol_table.cpp` or add a new `ISymbolTable` implementation and swap construction in `src/compiler/pipeline.cpp`.


Run Commands:
Make sure you are at the root directory of the project

- Compilation:
```g++ -std=gnu++17 -Iinclude src/main.cpp src/compiler/*.cpp -o cd_frontend.exe```
- Execution: 
```./cd_frontend.exe examples/sample_program.cd```

Other programs in ```examples/``` directory can be tested out as well.

## Streamlit GUI (Hackathon Cyber Style)

A Streamlit frontend is included in [streamlit_app.py](streamlit_app.py).

Features:
- Input source by typing code directly.
- Input source by uploading a file.
- Input source by selecting an existing file from [examples](examples).
- Runs the same C++ backend compiler executable.
- Renders generated SVG artifacts from [docs](docs):
  - token stream,
  - FIRST/FOLLOW,
  - LL(1) parse table,
  - grammar,
  - panic recovery,
  - symbol table,
  - AST.

### Run GUI

1. Install Python dependencies:

```bash
pip install -r requirements.txt
```

2. Start the Streamlit app:

```bash
streamlit run streamlit_app.py
```

Notes:
- The GUI can rebuild the backend executable using g++ before execution.
- Compiler artifacts continue to be written to [docs](docs) by the backend.
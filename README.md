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

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/cd_frontend
```

With a custom source file:

```bash
./build/cd_frontend path/to/file.cd
```

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

### Panic-mode test sample

Use `examples/sample_program_panic_mode.cd` (intentionally contains syntax errors):

```bash
./build/cd_frontend examples/sample_program_panic_mode.cd
```

Windows CMD example:

```cmd
cd /d "C:\Users\Aneek\Downloads\CD project_Arjeesh"
"CD project\cd_frontend.exe" "CD project\examples\sample_program_panic_mode.cd"
```

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


Run Command
Set-Location "C:\Users\Nabasree\Downloads\CD project_Arjeesh"; $env:Path="C:\msys64\ucrt64\bin;$env:Path"; & ".\CD project\cd_frontend.exe" ".\CD project\examples\sample_program_panic_mode.cd"

# Zeal Lua Port Todo

## Objectives
- Port the Lua compiler/VM to Zeal 8-bit OS.
- Generate direct Z80 machine code binaries.
- Support a subset of Lua 5.1 (Numbers, Strings, Tables, Functions).

## Progress

### Phase 1: Environment & Source Porting [COMPLETED]
- [x] Set up SDCC build environment.
- [x] Port lexer and basic parser.
- [x] Implement memory allocation stubs for Zeal OS.

### Phase 2: Bytecode & Compiler [COMPLETED]
- [x] Implement AST to Bytecode translation.
- [x] Add support for basic statements (Print, Variables, If, While).
- [x] Add support for local variables and resolve scoping.

### Phase 3: Z80 Machine Code Encoder [COMPLETED]
- [x] Implement Z80 instruction encoder (`z80_encoder.c/h`).
- [x] Support LD, ADD, SUB, JP, CALL, JR, Push/Pop, etc.
- [x] Implement label and reference resolution system.

### Phase 4: Z80 Code Generator & VM [COMPLETED]
- [x] Integrate encoder with `codegen.c` to emit `.bin` directly.
- [x] Implement Stack-based VM in machine code.
- [x] Implement opcodes: `LOADCONST`, `PRINT`, `ADD`, `SUB`, `MUL`, `JUMP`, `JUMPIFFALSE`.
- [x] Implement variable access: `GETLOCAL`, `SETLOCAL`, `GETGLOBAL`, `SETGLOBAL`.
- [x] Implement Value Stack management (Push/Pop).

### Phase 5: Testing & Refinement [IN PROGRESS]
- [x] Update host tool to generate `.bin` files directly.
- [x] Verify `test_host.bin` on emulator.
- [x] Implement Logic and Comparisons (EQ, NE, LT, LE, GT, GE).
- [x] Implement Symbol Table emission (`.sym` files).
- [x] Implement Table support (`NEWTABLE`, `GETTABLE`, `SETTABLE`) for numeric and string keys.
- [x] Implement `not` and truthiness handling in machine code.
- [x] Add semantic ucsim regression coverage for arithmetic, comparisons, branches, locals, loops, tables, string-key access, and minimal `for ... in`.
- [x] Implement short-circuit logic operators (`AND`, `OR`).
- [x] Implement basic function definitions, calls, and returns (without closures).
- [x] Implement trailing BSS emission with startup zeroing for generated binaries.
- [X] Test on actual Zeal-8-bit-OS hardware.

### Phase 6: Coverage Gaps & Remaining Runtime Work

#### Missing Test Coverage
- [x] Add regression for multiple assignment (`a, b = 1, 2`).
- [x] Add regression for mixed table literals containing both array and named fields in the same literal.
- [x] Add regression proving string keys compare by content across distinct string literals, not only reused constants.
- [X] Add output-oriented regression coverage for `print(...)` semantics if stdout validation is still desired.

#### Missing Implementation
- [x] Implement exponentiation (`^`) in the VM/runtime.
- [x] Implement string concatenation (`..`) in the VM/runtime.
	- [x] Phase A: refactor runtime strings to length-prefixed objects.
	- [x] Phase A: separate the string pool from table allocation.
	- [x] Phase A: share a singleton empty string object.
	- [x] Phase A: make string helpers (`#`, compare-by-content, `tostring`, `tonumber`) length-aware.
	- [x] Phase A: implement concat allocation on top of the new string representation.
	- [x] Phase A: add concat-focused regressions.
- [x] Implement length operator (`#`) in the VM/runtime.
- [x] Implement built-ins `type`, `tostring`, and `tonumber`.
- [x] Implement closures and upvalue capture for nested functions.

### Phase 7: Diagnostics & Error Reporting [IN PROGRESS]

#### Structured Diagnostic Positions
- [x] Add `column` field to `Token` struct (`src/token.h`).
- [x] Add `column` cursor to `Lexer` struct (`src/lexer.h`); initialize to 1, increment per char consumed, reset to 1 on newline.
- [x] Stamp correct `line` and `column` on all token types (numbers, identifiers, strings, punctuation, `TOK_EOF`, `TOK_ERROR`) in `src/lexer.c`.
- [x] Add `error_line` and `error_column` fields to `Parser` struct (`src/parser.h`).
- [x] Add centralized `parser_set_error` / `parser_set_error_at_current` / `parser_set_expected_error` helpers in `src/parser.c`.
- [x] Replace all bare `p->has_error = true` sites in `src/parser.c` with helper calls carrying descriptive messages.
- [x] Propagate `TOK_ERROR` from lexer into parser diagnostic fields on `advance()` and in `parser_init`.
- [x] Add `has_error`, `error_line`, `error_column`, `error_msg` to `CompiledChunk` (`src/compiler.h`).
- [x] Add `compiler_fail` helper in `src/compiler.c`; wire silent `return false` to it where input is invalid.
- [x] Standardize `src/main.c` error output to `file:line:column: error: message` format.
- [x] Update `src/host_test_lexer.c` to emit the same diagnostic format.

#### Golden Regression Tests for Diagnostics
- [x] Add `check-diag-%` Makefile target for compiler-output golden tests (no ucsim).
- [x] `diag_unterminated_string`: lexer error for unterminated string literal.
- [x] `diag_unexpected_char`: lexer error for unknown character (`@`).
- [x] `diag_tilde_without_eq`: lexer error for `~` not followed by `=`.
- [x] `diag_missing_end`: parser expected-token error when `end` is absent.
- [x] `diag_expected_then`: parser error when `then` keyword is missing after `if` condition.
- [x] `diag_expected_do`: parser error when `do` keyword is missing after `while` condition.
- [x] `diag_expected_expr`: parser error when an expression is required but a non-expression token is found.

#### Planned: Compiler Diagnostic Source Positions (next pass)
- [x] Add `line` and `column` to `Expr` AST nodes in `src/ast.h`; stamp in parser.
- [x] Add `line` and `column` to `Stmt` AST nodes in `src/ast.h`; stamp in parser.
- [x] Route compiler-stage failures through `compiler_fail` with nearest statement position.
- [ ] Add golden error tests for compile-time failures (e.g. too many locals, constant overflow).


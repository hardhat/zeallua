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
- [ ] Implement short-circuit logic operators (`AND`, `OR`).
- [ ] Implement Function calls and returns.
- [ ] Test on actual Zeal-8-bit-OS hardware.

### Phase 6: Coverage Gaps & Remaining Runtime Work [PLANNED]

#### Missing Test Coverage
- [ ] Add regression for multiple assignment (`a, b = 1, 2`).
- [ ] Add regression for mixed table literals containing both array and named fields in the same literal.
- [ ] Add regression proving string keys compare by content across distinct string literals, not only reused constants.
- [ ] Add output-oriented regression coverage for `print(...)` semantics if stdout validation is still desired.

#### Missing Implementation
- [ ] Implement exponentiation (`^`) in the VM/runtime.
- [ ] Implement string concatenation (`..`) in the VM/runtime.
- [ ] Implement length operator (`#`) in the VM/runtime.
- [ ] Implement built-ins `type`, `tostring`, and `tonumber`.
- [ ] Implement function definitions, closures, calls, and returns in the VM/runtime.
- [ ] Decide whether to remove or downgrade unsupported feature claims in `README.md` until the above items exist.

## Next Steps
1. Implement short-circuit logic operators (`AND`, `OR`).
2. Implement function definitions, calls, and returns.
3. Close the highest-value regression gaps in Phase 6.

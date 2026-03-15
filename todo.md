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
- [/] Verify `test_host.bin` on emulator (Next Step).
- [ ] Implement Table support (`NEWTABLE`, `GETTABLE`, `SETTABLE`).
- [ ] Implement Logic operators (`AND`, `OR`, `NOT`).
- [ ] Implement Function calls and returns.
- [ ] Test on actual Zeal-8-bit-OS hardware.

## Next Steps
1. Run and verify the generated binary in a Z80 emulator.
2. Implement Table support in machine code.
3. Implement Logic and Comparisons (EQ, NE, LT, LE, GT, GE).

# Zeal Lua Porting Tasks

- [x] Phase 1: Build System & Toolchain Setup
  - [x] Update `zeallua/Makefile` to build all compiler components (`lexer.c`, `parser.c`, `compiler.c`, `codegen.c`).
  - [x] Setup `ucsim_z80` integration in the `Makefile` for automated testing.
  - [x] Create test stubs mapped to memory for simulating Zeal-8bit-OS kernel handlers in `ucsim_z80`.

- [x] Phase 2: Kernel Abstraction Integration
  - [x] Implement VFS wrappers (`open`, `read`, `write`, `close`) using `zos_vfs.h` for file I/O instead of standard libc.
  - [x] Implement standard output for `print()` using `DEV_STDOUT`.
  - [x] Add `zos_keyboard.h` and `zos_time.h` bindings if Lua API exposes them.
  - [x] Update `main.c` to properly use OS arguments and `zos_sys.h` for exit sequences.

- [x] Phase 3: Tokenizer & Parser Validation
  - [x] Ensure `lexer.c` and `parser.c` successfully tokenize and build the AST using only supported Zeal OS memory allocation (or pre-allocated buffers).
  - [x] Validate 16-bit integer limitations and string processing performance.

- [/] Phase 4: Z80 Native Code Generation
  - [ ] Implement AST/Bytecode translation to Z80 assembly in `codegen.c`.
  - [ ] Generate the proper Zeal-8bit-OS executable header (loaded at `0x4000`).
  - [ ] Implement basic arithmetic, loops, and conditional jumps in Z80 assembly generation.
  - [ ] Implement Lua function calls mapping to Z80 `CALL`.
  - [ ] Map Lua local variables to Z80 stack and global variables to a generated `.BSS` section.

- [ ] Phase 5: Testing & Execution pipeline
  - [ ] Feed `examples/simple.lua` through the `zeallua` compiler.
  - [ ] Assemble the resulting Z80 `.asm` file.
  - [ ] Run the executable in `ucsim_z80` and verify the expected print outputs.
  - [ ] Test the pipeline on actual Zeal-8-bit-OS natively.

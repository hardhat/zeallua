# Zeal Lua

## Description

Zeal Tiny Lua is a Lua compiler for the Zeal 8-bit OS.

## Building

Build the Zeal target binary with the default Makefile:

```sh
make
```

Build the native Linux host compiler used by the regression harness:

```sh
make host
```

Run the ucsim-backed semantic tests with the host compiler:

```sh
make host-test
```

Clean both target and host build outputs:

```sh
make clean
```

`Makefile.linux` is still available directly if needed:

```sh
make -f Makefile.linux
make -f Makefile.linux clean
```


## Supported Lua Features

### Data Types
- **nil** - null value
- **boolean** - true/false
- **number** - 16-bit signed integers (-32768 to 32767)
- **string** - immutable strings (compared by content)
- **table** - associative arrays with integer or string keys
- **function** - first-class functions with closures

### Operators

**Arithmetic:**
- `+` `-` `*` `/` `%` (modulo) `^` (exponentiation)
- Unary `-` (negation)

**Comparison:**
- `==` `~=` `<` `<=` `>` `>=`

**Logical:**
- `and` `or` `not`

**String:**
- `..` (concatenation)
- `#` (length)

### Control Structures

```lua
-- If statement
if condition then
    -- body
elseif other then
    -- body
else
    -- body
end

-- While loop
while condition do
    -- body
end

-- Numeric for loop
for i = start, stop do
    -- body
end

for i = start, stop, step do
    -- body
end

-- Repeat-until
repeat
    -- body
until condition
```

### Functions

```lua
-- Function declaration
function name(arg1, arg2)
    -- body
    return value
end

-- Anonymous functions
local f = function(x) return x * 2 end

-- Multiple return values (limited support)
return a, b
```

### Tables

```lua
-- Empty table
local t = {}

-- Array-style initialization
local arr = {10, 20, 30}

-- Record-style initialization
local point = {x = 10, y = 20}

-- Mixed initialization
local mixed = {100, 200, name = "test"}

-- Table access
t[1] = "first"
t["key"] = "value"
t.field = 123

-- String keys are compared by content
t["foo"] = 1
print(t["foo"])  -- works even with different string literals
```

### Built-in Functions

| Function | Description |
|----------|-------------|
| `print(...)` | Print values to serial output |
| `type(v)` | Return type name as string |
| `tostring(v)` | Convert value to string |
| `tonumber(s)` | Convert string to number |

### Variables

```lua
-- Global variables
x = 10

-- Local variables
local y = 20

-- Multiple assignment
local a, b = 1, 2
```

## Examples

### Hello World

```lua
print("Hello, Z80!")
```

### Factorial

```lua
function factorial(n)
    if n <= 1 then
        return 1
    end
    return n * factorial(n - 1)
end

print(factorial(5))  -- prints 120
```

### Fibonacci

```lua
function fib(n)
    if n <= 1 then
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

for i = 0, 10 do
    print(fib(i))
end
```

### Working with Tables

```lua
-- Create a table
local scores = {}
scores["alice"] = 100
scores["bob"] = 85

print(scores["alice"])  -- 100

-- Array-style
local primes = {2, 3, 5, 7, 11}
print(primes[1])  -- 2 (Lua arrays are 1-indexed)
print(primes[3])  -- 5
```

### Iterative Loops

```lua
-- Sum 1 to 100
local sum = 0
for i = 1, 100 do
    sum = sum + i
end
print(sum)  -- 5050

-- Countdown
local n = 10
while n > 0 do
    print(n)
    n = n - 1
end
print("Liftoff!")
```

## Limitations

### Not Supported
- Floating-point numbers (integers only, 16-bit)
- Metatables and metamethods
- Coroutines
- File I/O
- `pairs()` / `ipairs()` iterators
- Standard library functions (math, string, table, etc.)
- Multiple return value assignment beyond 2 values
- Vararg functions (`...`)
- Generic for loops (`for k, v in pairs(t)`)

### Constraints
- Maximum 256 constants per function
- Maximum 256 local variables per function
- Maximum 256 global variables
- Table capacity: 8 entries (fixed)
- String comparison: O(n) character-by-character
- No garbage collection (heap grows monotonically)
- Stack depth limited by available RAM

## Project Structure

```
zeallua/
├── Makefile
├── Makefile.linux
├── README.md
├── src/
│   ├── main.c        # CLI entry point
│   ├── lexer.c       # Tokenizer
│   ├── token.c       # Token definitions
│   ├── parser.c      # Recursive descent parser
│   ├── ast.c         # Abstract syntax tree
│   ├── compiler.c    # AST to bytecode compiler
│   ├── bytecode.c    # Bytecode definitions
│   ├── codegen.c     # Z80 native code generator
│   └── interpreter.c # Tree-walking interpreter (for -i mode)
└── examples/
    ├── simple.lua
    ├── loops.lua
    ├── factorial.lua
    ├── fibonacci.lua
    └── tables.lua
```

## Bytecode Reference

The VM uses a stack-based bytecode with the following opcodes:

| Opcode | Hex  | Description |
|--------|------|-------------|
| Nop    | 0x00 | No operation |
| Pop    | 0x01 | Pop top of stack |
| Dup    | 0x02 | Duplicate top of stack |
| Rot3   | 0x03 | Rotate: (a,b,c) -> (b,c,a) |
| LoadNil | 0x10 | Push nil |
| LoadTrue | 0x11 | Push true |
| LoadFalse | 0x12 | Push false |
| LoadConst | 0x13 | Push constant (1-byte index) |
| GetLocal | 0x20 | Push local variable |
| SetLocal | 0x21 | Pop to local variable |
| GetGlobal | 0x22 | Push global variable |
| SetGlobal | 0x23 | Pop to global variable |
| NewTable | 0x30 | Create new table |
| GetTable | 0x31 | table[key] -> value |
| SetTable | 0x32 | table[key] = value |
| GetField | 0x33 | table.field -> value |
| SetField | 0x34 | table.field = value |
| Add | 0x40 | a + b |
| Sub | 0x41 | a - b |
| Mul | 0x42 | a * b |
| Div | 0x43 | a / b |
| Mod | 0x44 | a % b |
| Pow | 0x45 | a ^ b (exponent) |
| Neg | 0x46 | -a |
| Eq | 0x50 | a == b |
| Lt | 0x52 | a < b |
| Le | 0x53 | a <= b |
| Not | 0x60 | not a |
| Concat | 0x70 | a .. b |
| Len | 0x71 | #a (length) |
| Jump | 0x80 | Unconditional jump |
| JumpIfFalse | 0x81 | Jump if top is falsy |
| Call | 0x90 | Call function |
| Return | 0x91 | Return from function |
| Print | 0xA0 | Print values |
| Type | 0xA1 | Return type as string |
| ToNumber | 0xA2 | Convert to number |
| ToString | 0xA3 | Convert to string |
| Halt | 0xFF | Stop execution |

## License

BSD-3-Clause

## Author

Dale Wick

## License

BSD-3-Clause

## Credits

This project is inspired by kz80_lua by Alex Jokela.
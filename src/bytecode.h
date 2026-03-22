#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>

typedef enum {
    OP_NOP = 0x00,
    OP_POP = 0x01,
    OP_DUP = 0x02,
    OP_ROT3 = 0x03,

    OP_LOADNIL = 0x10,
    OP_LOADTRUE = 0x11,
    OP_LOADFALSE = 0x12,
    OP_LOADCONST = 0x13,
    OP_LOADCONSTW = 0x14,

    OP_GETLOCAL = 0x20,
    OP_SETLOCAL = 0x21,
    OP_GETGLOBAL = 0x22,
    OP_SETGLOBAL = 0x23,
    OP_GETUPVAL = 0x24,
    OP_SETUPVAL = 0x25,
    OP_GETCAPTURED = 0x26,
    OP_SETCAPTURED = 0x27,

    OP_NEWTABLE = 0x30,
    OP_GETTABLE = 0x31,
    OP_SETTABLE = 0x32,

    OP_ADD = 0x40,
    OP_SUB = 0x41,
    OP_MUL = 0x42,
    OP_DIV = 0x43,
    OP_MOD = 0x44,
    OP_POW = 0x45,
    OP_NEG = 0x46,

    OP_EQ = 0x50,
    OP_NE = 0x51,
    OP_LT = 0x52,
    OP_LE = 0x53,
    OP_GT = 0x54,
    OP_GE = 0x55,

    OP_NOT = 0x60,

    OP_CONCAT = 0x70,
    OP_LEN = 0x71,

    OP_JUMP = 0x80,
    OP_JUMPIFFALSE = 0x81,
    OP_JUMPIFTRUE = 0x82,

    OP_CALL = 0x90,
    OP_RETURN = 0x91,
    OP_CLOSURE = 0x92,

    OP_PRINT = 0xA0,
    OP_TYPE = 0xA1,
    OP_TONUMBER = 0xA2,
    OP_TOSTRING = 0xA3,
    OP_INPUT = 0xA4,

    OP_HALT = 0xFF
} OpCode;

#endif

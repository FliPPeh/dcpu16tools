#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#define RAMSIZE 0x10000

/*
 * Tokens
 *
 * Most of these are actually part of the parser and don't concern the emulator,
 * it's easier however to just share common code.
 */
typedef enum {
    T_STRING, /* "string" */
    T_IDENTIFIER, /* An identifier */
    T_NUMBER, /* Any number */
    T_LBRACK, /* '[' */
    T_RBRACK, /* ']' */
    T_COMMA,  /* ',' */
    T_COLON,  /* ':' */
    T_PLUS,   /* '+' */
    T_A, T_B, T_C, T_X, T_Y, T_Z, T_I, T_J, /* Registers */
    T_POP, T_PEEK, T_PUSH, /* POP, PEEK, PUSH */
    T_SP, T_PC, T_O,
    
    T_SET, T_ADD, T_SUB, T_MUL, T_DIV, T_MOD, T_SHL,
    T_SHR, T_AND, T_BOR, T_XOR, T_IFE, T_IFN, T_IFG, T_IFB,

    T_JSR,
    T_ORG, T_DAT,  /* Assembler macros */
    T_NEWLINE
} dcpu16token;

/*
 * An entry in the global label list
 */
typedef struct {
    char *label;
    uint16_t pc;

    int defined;
} dcpu16label;

/*
 * Specifies whether an operand is an immediate
 * value or a reference inside the RAM at that
 * position
 */
typedef enum {
    IMMEDIATE, REFERENCE
} dcpu16addressing;

/*
 * Specifies the type of operand, where
 *    LITERAL           = 0x0000 - 0xFFFFF
 *    LABEL             = an alphanumeric alies for the PC it was defined at
 *    REGISTER          = A, B, C, X, Y, Z, I, J
 *                          also POP, PUSH, PEEK, PC, SP, O in immediate context
 *    REGISTER_OFFSET   = Only valid as reference: [A + 0xFFFF]
 */
typedef enum {
    LITERAL, LABEL, REGISTER, REGISTER_OFFSET
} dcpu16operandtype;

typedef struct {
    /* Reuse operandtype here.  Only allowed values are LITERAL and LABEL */
    dcpu16operandtype type;
    dcpu16token register_index;

    union {
        int offset;
        char *label;
    };
} dcpu16registeroffset;

typedef struct {
    dcpu16addressing addressing;
    dcpu16operandtype type;

    union {
        dcpu16token token;
        int numeric;
        char *label;
        dcpu16registeroffset register_offset;
    };
} dcpu16operand;

typedef struct {
    uint16_t pc;
    int line;
    dcpu16token opcode;
    dcpu16operand a;
    dcpu16operand b;
} dcpu16instruction;

typedef struct {
    uint16_t registers[8];
    uint16_t ram[RAMSIZE];
    uint16_t pc;
    uint16_t sp;
    uint16_t o;

    int skip_next;
} dcpu16;


#endif

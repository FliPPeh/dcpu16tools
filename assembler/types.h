#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#include "parse.h"

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

#endif

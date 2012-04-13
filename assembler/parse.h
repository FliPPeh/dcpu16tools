#ifndef PARSE_H
#define PARSE_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#include "../common/linked_list.h"

/*
 * Tokens
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


union {
    /* This should be big enough for any token */
    char string[1024];
    uint16_t number;
} cur_tok;


extern int pc;
extern int flag_paranoid;
extern uint16_t ram[];

void parsefile(FILE*f, list*, list*);

#endif

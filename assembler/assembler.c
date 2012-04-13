/*
 * Copyright (c) 2012
 *
 * This file is part of dcpu16tools
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "label.h"
#include "parse.h"
#include "util.h"
#include "dcpu16.h"
#include "../common/linked_list.h"
#include "../common/hexdump.h"

/*
 * Maximum length of a label
 */
#define MAXLABEL 64

/*
 * RAM size (per specification
 */
#define RAMSIZE 0x10000



void display_help();
void check_instruction(dcpu16instruction*);

void write_memory(list*, list*);

/*
 * Options and output parameters
 */
int flag_be = 0;
int flag_paranoid = 0;
list *labels;
list *instructions;
uint16_t ram[0x10000];

extern char *srcfile, *cur_line, *cur_pos;
extern int curline;

void free_label(void *d) {
    dcpu16label *l = d;

    free(l->label);
    free(l);
}

int main(int argc, char **argv) {
    int lopts_index = 0;
    char outfile[256] = "out.hex";

    FILE *input = stdin;
    FILE *output = NULL;

    labels = list_create();
    instructions = list_create();

    static struct option lopts[] = {
        {"bigendian", no_argument, NULL, 'b'},
        {"help",      no_argument, NULL, 'h'},
        {"paranoid",  no_argument, NULL, 'p'},
        {NULL,        0,           NULL,  0 }
    };

    for (;;) {
        int opt = getopt_long(argc, argv, "bho:p", lopts, &lopts_index);

        if (opt < 0)
            break;

        switch (opt) {
        case 0:
            break;

        case 'b':
            flag_be = 1;
            break;

        case 'h':
            display_help();
            return 0;

        case 'p':
            flag_paranoid = 1;
            break;

        case 'o':
            strncpy(outfile, optarg, sizeof(outfile));
            break;
        }
    }

    if (optind < argc) {
        char *fname = argv[optind++];

        if (strcmp(fname, "-")) {
            srcfile = fname;

            if ((input = fopen(fname, "r")) == NULL) {
                fprintf(stderr, "Unable to open '%s' -- aborting\n", fname);
                return 1;
            }
        }
    }

    if ((output = fopen(outfile, "w+")) == NULL) {
        fprintf(stderr, "Unable to open '%s' -- aborting\n", outfile);

        if (input != stdin)
            fclose(input);

        return 1;
    }

    /* Parse the file, storing labels and instructions as we go */
    parsefile(input, instructions, labels);

    cur_line = cur_pos = NULL;
    write_memory(instructions, labels);

    write_hexdump(output, flag_be ? BIGENDIAN : LITTLEENDIAN, ram, RAMSIZE);
    fclose(output);

    /* Release resources */
    list_dispose(&labels, &free_label);
    list_dispose(&instructions, &free);

    if (input != stdin)
        fclose(input);

    return 0;
}

void display_help() {
    printf("Usage: dcpu16asm [OPTIOMS] [FILENAME]\n"
           "where OPTIONS is any of:\n"
           "  -h, --help          Display this help\n"
           "  -b, --bigendian     Generate big endian code "
                                 "rather than little endian\n"
           "  -p, --paranoid      Turn on warnings about non-fatal (but "
                                 "potentially harmful) problems\n"
           "  -o FILENAME         Write output to FILENAME instead of "
                                 "\"out.bin\"\n"
           "\n"
           "FILENAME, if given, is the source file to read instructions from.\n"
           "Defaults to standard input if not given or filename is \"-\"\n");
}



uint16_t encode_opcode(dcpu16token t) {
    if (is_nonbasic_instruction(t)) {
            if (t == T_JSR)
                return 0x0001;
    } else {
        return (t - T_SET) + 1;
    }

    return 0;
}

uint16_t encode_value(list *labels, dcpu16operand *op, uint16_t *wop) {
    if (op->type == REGISTER) {
        if (is_register(op->token))
            return (op->token - T_A) + (op->addressing == REFERENCE 
                                            ? 0x08
                                            : 0x00);
        else if (is_stack_operation(op->token) || is_status_register(op->token))
            return (op->token - T_POP) + 0x18;

    } else if (op->type == LITERAL) {
        if (op->addressing == IMMEDIATE) {
            if (op->numeric > 0x1f) {
                *wop = op->numeric;
                return 0x1f;
            } else {
                return op->numeric + 0x20;
            }
        } else {
            *wop = op->numeric;
            return 0x1e;
        }
    } else if (op->type == LABEL) {
        dcpu16label *l = getlabel(labels, op->label);
        if ((l != NULL) && l->defined) {
            dcpu16addressing a = op->addressing;
            /*
             * TODO: Short labels
             */
            *wop = l->pc;

            /*
             * The next line is a very ugly hack to force the assembler to
             * write the resolved label into the next word even if it
             * resolves to < 0x20. This is so it can later use the numerical
             * when looking for warnings
             */
            op->addressing = REFERENCE;
            op->type = LITERAL;
            op->numeric = l->pc;

            return (a == IMMEDIATE ? 0x1f : 0x1e);
        } else {
            error("Unresolved label '%s'", op->label);
        }
    } else {
        if (op->register_offset.type == LABEL) {
            dcpu16label *l = getlabel(labels, op->register_offset.label);

            if ((l != NULL) && l->defined) {
                *wop = l->pc;
                op->register_offset.type = LITERAL;
                op->register_offset.offset = l->pc;
            } else {
                error("Unresolved label '%s'", op->register_offset.label);
            }
        } else {
            *wop = op->register_offset.offset;
        }

        return (op->register_offset.register_index - T_A) + 0x10;
    }

    return 0;
}

void encode(dcpu16instruction *i, uint16_t *wi, uint16_t *wa, uint16_t *wb,
            list *labels) {
    if (is_nonbasic_instruction(i->opcode))
        *wi = 0x00 | encode_opcode(i->opcode) << 4
                   | encode_value(labels, &(i->a), wa) << 10;
    else
        *wi = encode_opcode(i->opcode)
            | encode_value(labels, &(i->a), wa) << 4
            | encode_value(labels, &(i->b), wb) << 10;
}

void check_instruction(dcpu16instruction *instr) {
    if ((instr->opcode == T_DIV) || (instr->opcode == T_MOD))
        if ((instr->b.type == LITERAL) && (instr->b.numeric == 0))
            warning("Division or modulo by zero.");

    if ((instr->opcode >= T_SET) && (instr->opcode <= T_XOR))
        if ((instr->a.type == LITERAL) && (instr->a.addressing == IMMEDIATE))
            warning("Trying to assign to a literal value. This will be silently"
                    " ignored upon execution but still use CPU cycles.");
}

void write_memory(list *instructions, list *labels) {
    /*
     * Purposeful shadowing of the global 'pc'
     */
    uint16_t pc;

    list_node *n = NULL;

    for (n = list_get_root(instructions); n != NULL; n = n->next) {
        uint16_t op, a, b;
        dcpu16instruction *i = n->data;

        /* For the error() command */
        curline = i->line;

        pc = i->pc;
        encode(i, &op, &a, &b, labels);

        /*
         * TODO: Endianness
         */
        ram[pc++] = op;

        if (uses_next_word(&(i->a))) {
            ram[pc++] = a;
        }

        if (!is_nonbasic_instruction(i->opcode))
            if (uses_next_word(&(i->b)))
                ram[pc++] = b;

        if (flag_paranoid == 1)
            check_instruction(i);
    }
}


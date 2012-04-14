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
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <ncurses.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>

#include "gui.h"
#include "../common/hexdump.h"
#include "../common/dcpu16.h"
#include "../common/types.h"


void dcpu16_init(dcpu16*);
void dcpu16_step(dcpu16*);
void dcpu16_fetch(dcpu16instruction*, dcpu16*);
void dcpu16_execute(dcpu16instruction*, dcpu16*);
uint16_t *dcpu16_lookup_opcode(uint8_t, dcpu16*);

int dcpu16_get_operandtype(uint8_t, dcpu16operandtype*,
                                    dcpu16addressing*);

void display_help();

void emulate(dcpu16*);
void disassemble(dcpu16*);

static int flag_disassemble = 0;
static int flag_verbose = 0;
static int flag_be = 0;
static int flag_halt = 0;

/* So we can return a pointer to a literal value */
static uint16_t literals[32] = {0};

/*
 * TODO: * Improve reading in program file
 *       * Emulate clockrate (100 kHz) and clock cycles
 */
int main(int argc, char **argv) {
    int i;
    int lopts_index = 0;
    FILE *source = stdin;

    static struct option lopts[] = {
        {"verbose",      no_argument, NULL, 'v'},
        {"help",         no_argument, NULL, 'h'},
        {"bigendian",    no_argument, NULL, 'b'},
        {"disassemble",  no_argument, NULL, 'd'},
        {"halt",         no_argument, NULL, 'H'},
        {NULL,           0,           NULL,  0 }
    };

    for (;;) {
        int opt = getopt_long(argc, argv, "vdhHb", lopts, &lopts_index);

        if (opt < 0)
            break;

        switch (opt) {
        case 0:
            /* Long option */
            break;

        case 'h':
            display_help();
            return 0;

        case 'H':
            flag_halt = 1;
            break;

        case 'v':
            flag_verbose = 1;
            break;

        case 'd':
            flag_disassemble = 1;
            break;

        case 'b':
            flag_be = 1;
            break;

        case '?':
            break;
        }
    }

    /* If there are any arguments left after parsing options, there could
     * be a filename given as program input rather than using stdin */
    if (optind < argc) {
        const char *fname = argv[optind++];

        /* '-' is an optional, explicit request to use stdin */
        if (strcmp(fname, "-")) {
            if ((source = fopen(fname, "r")) == NULL) {
                fprintf(stderr, "Unable to open '%s' -- aborting\n", fname);
                return 1;
            }
        }
    }

    initgui();

    dcpu16 cpu;
    dcpu16_init(&cpu);

    /* Read program into memory */
    read_hexdump(source, flag_be ? BIGENDIAN : LITTLEENDIAN, cpu.ram, RAMSIZE);

    if (source != stdin)
        fclose(source);

    /* Initialize literal table */
    for (i = 0; i < 32; ++i)
        literals[i] = i;

    if (!flag_disassemble)
        emulate(&cpu);
    else
        printf("BRB FIXING\n"); // TODO: TODO: TODO: !!

    cleanupgui();

    return 0;
}


void display_help() {
    printf("Usage: dcpu16emu [OPTIONS] [FILENAME]\n"
           "where OPTIONS is any of:\n"
           "  -h, --help          Display this help\n"
           "  -v, --verbose       Enable verbosity\n"
           "  -b, --bigendian     Interpret the words in the source file as "
                                 "big endian\n"
           "                      rather than little endian.\n"
           "  -d, --disassemble   Print the instructions that make up "
                                 "the program instead\n"
           "                      of executing it.  Origin offsets, labels"
                                 "and data sections are not restored.\n"
           "  -H, --halt          Automatically stop executing if the PC "
                                 "did not change\n"
           "                      after an instruction\n"
           "                         e.g. loop: SET PC, loop\n"
           "\n"
           "FILENAME is a file containing the bytecode "
           "of the program to emulate.\n"
           "Defaults to standard input if no file is given.\n");
}

void dcpu16_init(dcpu16 *cpu) {
    memset(cpu, 0, sizeof(*cpu));
}

int dcpu16_get_operand(uint8_t raw, dcpu16operand *op, dcpu16 *cpu) {
    if (raw < 0x08) {
        op->type = REGISTER;
        op->addressing = IMMEDIATE;
        op->token = raw + T_A;
    } else if ((raw >= 0x08) && (raw < 0x10)) {
        op->type = REGISTER;
        op->addressing = REFERENCE;
        op->token = (raw % 0x8) + T_A;
    } else if ((raw >= 0x10) && (raw < 0x18)) {
        op->type = REGISTER_OFFSET;
        op->addressing = REFERENCE;
        op->register_offset.type = LITERAL;
        op->register_offset.register_index = (raw % 0x8) + T_A;
        op->register_offset.offset = cpu->ram[cpu->pc++];
    } else if ((raw >= 0x18) && (raw < 0x20)) {
        op->addressing = IMMEDIATE;
        op->type = REGISTER;

        switch (raw) {
        case 0x18: op->token = T_POP; break;
        case 0x19: op->token = T_PEEK; break;
        case 0x1A: op->token = T_PUSH; break;
        case 0x1B: op->token = T_SP; break;
        case 0x1C: op->token = T_PC; break;
        case 0x1D: op->token = T_O; break;
        case 0x1E: 
            op->type = LITERAL;
            op->addressing = REFERENCE;
            op->numeric = cpu->ram[cpu->pc++];

            break;

        case 0x1F:
            op->numeric = cpu->ram[cpu->pc++];
            op->type = LITERAL;

            break;
        }
    } else if ((raw >= 0x20) && (raw < 0x40)) {
        op->type = LITERAL;
        op->addressing = IMMEDIATE;
        op->numeric = raw - 0x20;
    }

    return 0;
}


void dcpu16_fetch(dcpu16instruction *instr, dcpu16 *cpu) {
    uint16_t word = cpu->ram[cpu->pc++];
    uint16_t inst = word & 0x000F;
    uint8_t a = (word & 0x03F0) >> 4;
    uint8_t b = (word & 0xFC00) >> 10;

    dcpu16operand opa, opb;

    instr->opcode = T_SET + inst - 1;

    /* Nonbasic instruction => opcode = a, a = b, b = nothing */
    if (inst == 0x0) {
        switch (a) {
            case 0x01: instr->opcode = T_JSR; break;
            default:   return;
        }

        a = b;

        dcpu16_get_operand(a, &opa, cpu);

        instr->a = opa;
    } else {
        dcpu16_get_operand(a, &opa, cpu);
        dcpu16_get_operand(b, &opb, cpu);

        instr->a = opa;
        instr->b = opb;
    }
}

uint16_t load(dcpu16operand *op, dcpu16 *cpu, int *cost) {
#define TOREG(r) ((r) - T_A)
    switch (op->type) {
    case REGISTER:
        if (op->addressing == IMMEDIATE) {
            switch (op->token) {
            case T_POP: return cpu->ram[cpu->sp++]; break;
            case T_PEEK: return cpu->ram[cpu->sp]; break;
            case T_PUSH: return cpu->ram[--cpu->sp]; break;
            case T_SP: return cpu->sp; break;
            case T_PC: return cpu->pc; break;
            case T_O: return cpu->o; break;
            default: return cpu->registers[TOREG(op->token)];
            }
        } else {
            return cpu->ram[cpu->registers[TOREG(op->token)]];
        }

    case REGISTER_OFFSET:
        return cpu->ram[cpu->registers[TOREG(op->register_offset.register_index)]
                       + op->register_offset.offset];

    case LITERAL:
        if (op->addressing == IMMEDIATE)
            return op->numeric;
        else
            return cpu->ram[op->numeric];
    }

    return 0;
#undef TOREG
}

void store(dcpu16operand *op, uint16_t val, dcpu16 *cpu) {
#define TOREG(r) ((r) - T_A)
    fprintf(stderr, "Store %04X to %d (%d)\n", val, op->token, op->type);

    switch (op->type) {
    case REGISTER:
        if (op->addressing == IMMEDIATE) {
                switch (op->token) {
                case T_POP: cpu->ram[cpu->sp++] = val; break;
                case T_PEEK: cpu->ram[cpu->sp] = val; break;
                case T_PUSH: cpu->ram[--cpu->sp] = val; break;
                case T_SP: cpu->sp = val; break;
                case T_PC: cpu->pc = val; break;
                case T_O: cpu->o = val; break;
                default: cpu->registers[TOREG(op->token)] = val;
                }
        } else {
            cpu->ram[cpu->registers[TOREG(op->token)]] = val;
        }

        break;

    case REGISTER_OFFSET:
        cpu->ram[cpu->registers[TOREG(op->register_offset.register_index)] +
                 cpu->ram[op->register_offset.offset]] = val;
        break;

    case LITERAL:
        if (op->addressing == IMMEDIATE)
            ; /* Skip but don't complain */
        else
            cpu->ram[op->numeric] = val;

        break;
    }

    return;
#undef TOREG
}

void dcpu16_execute(dcpu16instruction *instr, dcpu16 *cpu) {
    /* If any instruction tries to set a literal value, fail silently */
    uint16_t result;
    int costa, costb;
    uint16_t a = load(&(instr->a), cpu, &costa),
             b = load(&(instr->b), cpu, &costb);

    fprintf(stderr, "%04X\n", instr->opcode);

    switch (instr->opcode) {
    case T_JSR:
        fprintf("JUMP! %04X!\n", a);
        cpu->ram[--cpu->sp] = cpu->pc;
        cpu->pc = a;
        break;

    case T_SET:
        result = b;
        break;

    case T_ADD:
        result = a + b;
        cpu->o = ((result < a) || (result < b));
        break;

    case T_SUB:
        result = a - b;
        cpu->o = (result > a) ? 0xFFFF : 0;
        break;

    case T_MUL:
        result = a * b;
        cpu->o = (result >> 16) & 0xFFFF;
        break;

    case T_DIV:
        if (b != 0) {
            result = a / b;
            cpu->o = ((a << 16) / b) & 0xFFFF;
        } else {
            result = 0;
            cpu->o = 0;
        }

        break;

    case T_MOD:
        result = (b != 0) ? (a % b) : 0;
        break;

    case T_SHL:
        result = a << b;
        cpu->o = ((a << b) >> 16) & 0xFFFF;
        break;

    case T_SHR:
        result = a >> b;
        cpu->o = ((a << 16) >> b) & 0xFFFF;
        break;

    case T_AND:
        result = a & b;
        break;

    case T_BOR:
        result = a | b;
        break;

    case T_XOR:
        result = a ^ b;
        break;

    case T_IFE:
        cpu->skip_next = !(a == b);
        break;

    case T_IFN:
        cpu->skip_next =  (a == b);
        break;

    case T_IFG:
        cpu->skip_next = !(a > b);
        break;
    case T_IFB:
        cpu->skip_next =  (a & b) == 0;
        break;

    default: return;
    }

    fprintf(stderr, "Res %04X\n", result);
    if (!is_nonbasic_instruction(instr->opcode) && 
        ((instr->opcode >= T_SET) && (instr->opcode <= T_XOR)))
            store(&(instr->a), result, cpu);
}

void dcpu16_step(dcpu16 *cpu) {
    dcpu16instruction instr = {0};

    dcpu16_fetch(&instr, cpu);

    if (!cpu->skip_next)
        dcpu16_execute(&instr, cpu);
    else
        cpu->skip_next = 0;

}

void emulate(dcpu16 *cpu) {
    uint16_t last_pc = 0xFFFF;
    uint16_t keybuffer = 0;
    int c;
    int f,b;

    for(;;) {
        if ((c = getch()) > 0) {
            switch (c) {
            case KEY_LEFT: c = 1; break;
            case KEY_RIGHT: c = 2; break;
            case KEY_UP: c = 3; break;
            case KEY_DOWN: c = 4; break;
            }

            /*
             * This is actually wrong per observation, the actual
             * keyboard buffer is a 16 word ringbuffer, not a
             * single memory location.  This is a compatibility
             * decission.
             */
            cpu->ram[0x9000 + (keybuffer++ % 0x1)] = c;
        }

        dcpu16_step(cpu);
        write_hexdump(stderr, BIGENDIAN, cpu->ram, RAMSIZE);
        fprintf(stderr, "============== %04X =============\n", cpu->pc);
        updategui(cpu);

        /* Add clock cycles, then enable */
        usleep(10);

        if ((last_pc == cpu->pc) && flag_halt)
            break;

        last_pc = cpu->pc;
    }
}

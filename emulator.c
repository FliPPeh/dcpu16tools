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

#define RAMSIZE 0x10000

typedef enum {
    A, B, C, X, Y, Z, I, J
} dcpu16register;

typedef enum {
    JSR, SET, ADD, SUB,
    MUL, DIV, MOD, SHL,
    SHR, AND, BOR, XOR,
    IFE, IFN, IFG, IFB,
    INV
} dcpu16opcode;

typedef enum {
    REGISTER, REGISTER_INDIRECT, NEXT_WORD_PLUS_REGISTER,
    POP, PEEK, PUSH,
    SP, PC, O,
    NEXT_WORD, NEXT_WORD_LITERAL, LITERAL
} dcpu16addrmeth;

typedef struct {
    uint8_t opcode;
    int allow_store;
    uint8_t a_raw;
    uint16_t *a;
    uint8_t b_raw;
    uint16_t *b;
} dcpu16instr;

typedef struct {
    uint16_t registers[8];
    uint16_t ram[RAMSIZE];
    uint16_t pc;
    uint16_t sp;
    uint16_t o;

    int skip_next;
} dcpu16;


void dcpu16_init(dcpu16*);
void dcpu16_loadprogram(dcpu16*, uint16_t*, uint16_t);
void dcpu16_step(dcpu16*);
void dcpu16_fetch(dcpu16instr*, dcpu16*);
void dcpu16_execute(dcpu16instr*, dcpu16*);
uint16_t *dcpu16_lookup_opcode(uint8_t, dcpu16*);
dcpu16addrmeth dcpu16_get_addressmethod(uint8_t);

void display_help();

void emulate(dcpu16*);
void disassemble(dcpu16*);

void updatescreen(dcpu16*);

static int flag_disassemble = 0;
static int flag_verbose = 0;
static int flag_littleendian = 0;
static int flag_halt = 0;
static int flag_vga = 0;

/* So we can return a pointer to a literal value */
static uint16_t literals[32] = {0};


/*
 * TODO: * Improve reading in program file
 *       * Emulate clockrate (100 kHz) and clock cycles
 */
int main(int argc, char **argv) {
    int lopts_index = 0;
    FILE *source = stdin;
    uint8_t *program = NULL;
    int programsize = 0;

    static struct option lopts[] = {
        {"verbose",      no_argument, NULL, 'v'},
        {"help",         no_argument, NULL, 'h'},
        {"littleendian", no_argument, NULL, 'l'},
        {"disassemble",  no_argument, NULL, 'd'},
        {"vga",          no_argument, NULL, 'V'},
        {"halt",         no_argument, NULL, 'H'},
        {NULL,           0,           NULL,  0 }
    };

    for (;;) {
        int opt = getopt_long(argc, argv, "vVdhHl", lopts, &lopts_index);

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

        case 'V':
            flag_vga = 1;
            break;

        case 'd':
            flag_disassemble = 1;
            break;

        case 'l':
            flag_littleendian = 1;
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

    if (flag_vga) {
        initscr();
        cbreak();
        nodelay(stdscr, TRUE);
        noecho();
        start_color();
    }

    /* Read program into memory */
    do {
        uint8_t buf[256];
        size_t n = 0;

        if ((n = fread(buf, sizeof(uint8_t), sizeof(buf), source)) > 0) {
            program = realloc(program, programsize + n * sizeof(uint8_t));
            if (program != NULL) {
                memcpy(program + programsize, buf, n * sizeof(uint8_t));
                programsize = programsize + n * sizeof(uint8_t);
            } else {
                printf("reallocation failed -- aborting\n");
                return 1;
            }
        } else {
            break;
        }
    } while (!feof(source));

    if (source != stdin)
        fclose(source);

    /* Every pair of bytes we read has to be converted to a 16 bit integer.
     * We could not do this by reading in an unsigned short because system
     * endianness will affect the outcome. */
    if (programsize % 2 != 0) {
        fprintf(stderr, "invalid program file (uneven size) -- aborting\n");
        free(program);
        return 1;
    } else {
        uint16_t *programcode = malloc((programsize / 2) * sizeof(uint16_t));
        int i = 0;

        for (i = 0; i < programsize / 2; ++i)
            if (flag_littleendian)
                programcode[i] = (program[i*2 + 1] << 8) + program[i*2];
            else
                programcode[i] = (program[i*2] << 8) + program[i*2 + 1];
    
        dcpu16 cpu;
        dcpu16_init(&cpu);
        dcpu16_loadprogram(&cpu, programcode, programsize / 2);

        /* Initialize literal table */
        for (i = 0; i < 32; ++i)
            literals[i] = i;

        if (!flag_disassemble)
            emulate(&cpu);
        else
            disassemble(&cpu);

        free(program);
        free(programcode);
    }

    if (flag_vga)
        endwin();

    return 0;
}

void display_help() {
    printf("Usage: dcpu16emu [OPTIONS] [FILENAME]\n"
           "where OPTIONS is any of:\n"
           "  -h, --help          Display this help\n"
           "  -v, --verbose       Enable verbosity\n"
           "  -l, --littleendian  Interpret the words in the source file as "
                                 "little endian\n"
           "                      rather than big endian.\n"
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

void dcpu16_loadprogram(dcpu16 *cpu, uint16_t *program, uint16_t n) {
    memcpy(cpu->ram, program, n*2 > RAMSIZE ? RAMSIZE : n*2);
}

static char *val_to_registername(uint8_t r) {
    switch (r % 0x08) {
    case A: return "A";
    case B: return "B";
    case C: return "C";
    case X: return "X";
    case Y: return "Y";
    case Z: return "Z";
    case I: return "I";
    case J: return "J";
    }

    return "?";
}

char *addrmeth_to_string(dcpu16 *cpu,
                         uint8_t val,
                         uint16_t *ptr,
                         char *str,
                         size_t n) {
    memset(str, 0, n);

    switch (dcpu16_get_addressmethod(val)) {
    case REGISTER:
        snprintf(str, n, "%s", val_to_registername(val));
        break;
    case REGISTER_INDIRECT:
        snprintf(str, n, "[%s]", val_to_registername(val));
        break;
    case NEXT_WORD_PLUS_REGISTER:
        snprintf(str, n, "[0x%04X + %s]",
                 (uint16_t)(ptr - cpu->ram), val_to_registername(val));
        break;
    case POP:
        strcpy(str, "POP");
        break;
    case PEEK:
        strcpy(str, "PEEK");
        break;
    case PUSH:
        strcpy(str, "PUSH");
        break;
    case SP:
        strcpy(str, "SP");
        break;
    case PC:
        strcpy(str, "PC");
        break;
    case O:
        strcpy(str, "O");
        break;
    case NEXT_WORD:
        snprintf(str, n, "[0x%04X]", (uint16_t)(ptr - cpu->ram));
        break;
    case NEXT_WORD_LITERAL:
        snprintf(str, n, "0x%04X", *ptr);
        break;
    case LITERAL:
        snprintf(str, n, "0x%02X", *ptr);
    }

    return str;
}

int is_basic_instruction(dcpu16opcode op) {
    return op != JSR;
}

void inspect_instruction(dcpu16 *cpu, dcpu16instr *instr) {
    const char *opcodename;
    char a[64] = {0};
    char b[64] = {0};

    switch (instr->opcode) {
    case JSR: opcodename = "JSR"; break;
    case SET: opcodename = "SET"; break;
    case ADD: opcodename = "ADD"; break;
    case SUB: opcodename = "SUB"; break;
    case MUL: opcodename = "MUL"; break;
    case DIV: opcodename = "DIV"; break;
    case MOD: opcodename = "MOD"; break;
    case SHL: opcodename = "SHL"; break;
    case SHR: opcodename = "SHR"; break;
    case AND: opcodename = "AND"; break;
    case BOR: opcodename = "BOR"; break;
    case XOR: opcodename = "XOR"; break;
    case IFE: opcodename = "IFE"; break;
    case IFN: opcodename = "IFN"; break;
    case IFG: opcodename = "IFG"; break;
    case IFB: opcodename = "IFB"; break;
    default:  return;
    }

    if (is_basic_instruction(instr->opcode))
        printf("%s   %s, %s\n", opcodename,
                addrmeth_to_string(cpu, instr->a_raw, instr->a, a, 64),
                addrmeth_to_string(cpu, instr->b_raw, instr->b, b, 64));
    else
        printf("%s   %s\n", opcodename,
                addrmeth_to_string(cpu, instr->a_raw, instr->a, a, 64));
}

dcpu16addrmeth dcpu16_get_addressmethod(uint8_t val) {
    if (val < 0x08) {
        return REGISTER;
    } else if ((val >= 0x08) && (val < 0x10)) {
        return REGISTER_INDIRECT;
    } else if ((val >= 0x10) && (val < 0x18)) {
        return NEXT_WORD_PLUS_REGISTER;
    } else if ((val >= 0x18) && (val < 0x20)) {
        switch (val) {
        case 0x18: return POP;
        case 0x19: return PEEK;
        case 0x1A: return PUSH;
        case 0x1B: return SP;
        case 0x1C: return PC;
        case 0x1D: return O;
        case 0x1E: return NEXT_WORD;
        case 0x1F: return NEXT_WORD_LITERAL;
        }
    } else if ((val >= 0x20) && (val < 0x40)) {
        return LITERAL;
    }

    return -1;
}

uint16_t *dcpu16_lookup_operand(uint8_t val, dcpu16 *cpu) {
    switch (dcpu16_get_addressmethod(val)) {
    case REGISTER:
        return &(cpu->registers[val]);
    case REGISTER_INDIRECT:
        return cpu->ram + cpu->registers[val % 0x08];
    case NEXT_WORD_PLUS_REGISTER:
        return cpu->ram + cpu->ram[cpu->pc++] + cpu->registers[val % 0x08];
    case POP:
        return cpu->ram + cpu->sp++;
    case PEEK:
        return cpu->ram + cpu->sp;
    case PUSH:
        return cpu->ram + --cpu->sp;
    case SP:
        return &(cpu->sp);
    case PC:
        return &(cpu->pc);
    case O:
        return &(cpu->o);
    case NEXT_WORD:
        return cpu->ram + cpu->ram[cpu->pc++];
    case NEXT_WORD_LITERAL:
        return cpu->ram + cpu->pc++;
    case LITERAL:
        return &literals[val - 0x20];
    }

    return NULL;
}

void dcpu16_fetch(dcpu16instr *instr, dcpu16 *cpu) {
    uint16_t word = cpu->ram[cpu->pc++];
    uint8_t a = (word & 0x03F0) >> 4;
    uint8_t b = (word & 0xFC00) >> 10;

    instr->opcode = word & 0x000F;

    /* If the first parameter is a literal or the opcode is IFx then don't
     * store the result */
    if ((a == 0x1F) || ((a >= 0x20) && (a >= 0x3F)) 
                    || ((instr->opcode >= 0xC) && (instr->opcode <= 0xF))
                    || (instr->opcode == 0x00))
       instr->allow_store = 0;
    else
       instr->allow_store = 1;

    /* Nonbasic instruction => opcode = a, a = b, b = nothing */
    if (instr->opcode == 0x0) {
        switch (a) {
            case 0x01: instr->opcode = JSR; break;
            default:   instr->opcode = INV; break;
        }

        a = b;

        instr->a = dcpu16_lookup_operand(a, cpu);
        instr->a_raw = a;

        instr->b = NULL;
        instr->b_raw = 0;
    } else {
        instr->a = dcpu16_lookup_operand(a, cpu);
        instr->a_raw = a;

        instr->b = dcpu16_lookup_operand(b, cpu);
        instr->b_raw = b;
    }
}

void dcpu16_execute(dcpu16instr *instr, dcpu16 *cpu) {
    /* If any instruction tries to set a literal value, fail silently */
    uint16_t result;
    uint16_t *a = instr->a, *b = instr->b;

    switch (instr->opcode) {
    case JSR: cpu->ram[--cpu->sp] = cpu->pc; cpu->pc = *a; break;
    case SET: result = *b; break;
    case ADD: result = *a + *b; cpu->o = ((result < *a)||(result < *b)); break;
    case SUB: result = *a - *b; cpu->o = (result > *a) ? 0xFFFF : 0;     break;
    case MUL: result = *a * *b; cpu->o = (result >> 16) & 0xFFFF;        break;
    case DIV: if (*b != 0) {
                  result = *a / *b;
                  cpu->o = ((*a << 16) / *b) & 0xFFFF;
              } else {
                  result = 0;
                  cpu->o = 0;
              }

              break;
    case MOD: result = (*b != 0) ? (*a % *b) : 0; break;
    case SHL: result = *a << *b; cpu->o = ((*a << *b) >> 16) & 0xFFFF; break;
    case SHR: result = *a >> *b; cpu->o = ((*a << 16) >> *b) & 0xFFFF; break;
    case AND: result = *a & *b; break;
    case BOR: result = *a | *b; break;
    case XOR: result = *a ^ *b; break;
    case IFE: cpu->skip_next = !(*a == *b); break;
    case IFN: cpu->skip_next =  (*a == *b); break;
    case IFG: cpu->skip_next = !(*a > *b); break;
    case IFB: cpu->skip_next =  (*a & *b) == 0; break;
    default: return;
    }

    if (instr->allow_store)
        *a = result;
}

void dump_cpu(dcpu16 *cpu) {
    int i;
    printf("----------------------------------------------------------------------------\n"
           "%04X | %04X | %04X | %04X | %04X | %04X | %04X | %04X | %04X | %04X | %04X |\n"
           "A    | B    | C    | X    | Y    | Z    | I    | J    | O    | SP   | PC   |\n"
           "----------------------------------------------------------------------------\n",
           cpu->registers[A], cpu->registers[B], cpu->registers[C],
           cpu->registers[X], cpu->registers[Y], cpu->registers[Z],
           cpu->registers[I], cpu->registers[J], cpu->o, cpu->sp, cpu->pc);

    printf("Program:\n");

    for (i = 0; i < 0x100; i += 8) {
        int j;

        printf("%04X:  ", i);
        for (j = i; j < i+8; ++j)
            if (j >= 0x100)
                break;
            else
                if (cpu->pc == j)
                    printf(" \x1B[31m%04X\x1B[0m", cpu->ram[j]);
                else
                    printf(" %04X", cpu->ram[j]);

        printf("\n");
    }
}

void dcpu16_step(dcpu16 *cpu) {
    dcpu16instr instr = {0};

    dcpu16_fetch(&instr, cpu);

    if (flag_verbose)
        inspect_instruction(cpu, &instr);

    if (!cpu->skip_next)
        dcpu16_execute(&instr, cpu);
    else
        cpu->skip_next = 0;

}

void emulate(dcpu16 *cpu) {
    uint16_t last_pc = 0xFFFF;
    uint16_t keybuffer = 0;
    char c;

    while (cpu->pc < RAMSIZE) {
        if (flag_verbose)
            dump_cpu(cpu);

        if ((c = getch()) > 0) {
            cpu->ram[0x9000 + (keybuffer++ % 0x1)] = c;
        }

        dcpu16_step(cpu);

        updatescreen(cpu);

        /* Add clock cycles, then enable
        usleep(10); */

        if ((last_pc == cpu->pc) && flag_halt)
            break;

        last_pc = cpu->pc;
    }
}

void updatescreen(dcpu16 *cpu) {
    uint16_t x, y;
    uint16_t *vram = cpu->ram + 0x8000;

    //clear();

    for (x = 0; x < 32; ++x) {
        for (y = 0; y < 13; ++y) {
            char asciival = (vram[y * 32 + x] & 0x00FF);
            uint8_t fg = (vram[y * 32 + x] & 0xF000) >> 12;
            uint8_t bg = (vram[y * 32 + x] & 0x0F00) >> 8;

            /* Do something with fb and bg... */
            move(y, x);

            if (isprint(asciival))
                addch(asciival);
            else
                addch(' ');
        }
    }

    refresh();
}

void disassemble(dcpu16 *cpu) {
    dcpu16instr instr = {0};

    while (instr.opcode != INV) {
        dcpu16_fetch(&instr, cpu);

        if (flag_verbose)
            inspect_instruction(cpu, &instr);
    }
}

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

#include "../common/hexdump.h"

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

void init_gui();
void update(dcpu16*);
void updatescreen(dcpu16*);

void hndlresize(int);

static int flag_disassemble = 0;
static int flag_verbose = 0;
static int flag_be = 0;
static int flag_halt = 0;

/* So we can return a pointer to a literal value */
static uint16_t literals[32] = {0};

WINDOW *cpuscreen;
WINDOW *status;

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

    signal(SIGWINCH, hndlresize);

    initscr();
    clear();
    cbreak();
    start_color();
    if (!can_change_color()) {
        mvprintw(0, 0, "Your terminal does not support 256 colors, the screen "
                       "output will not look like it's supposed to look like!");
    }

    nodelay(stdscr, TRUE);
    noecho();
    keypad(stdscr, TRUE);

    init_gui();

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
        disassemble(&cpu);

    delwin(cpuscreen);
    endwin();

    return 0;
}

void init_gui() {
    init_pair(1, COLOR_BLUE, COLOR_WHITE);

    cpuscreen = newwin(14, 34, 2, 1);
    status = newwin(14, 24, 2, 37);

    wbkgd(status, COLOR_PAIR(1));
    wbkgd(cpuscreen, COLOR_PAIR(1));

    refresh();
    wrefresh(cpuscreen);

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

void dcpu16_step(dcpu16 *cpu) {
    dcpu16instr instr = {0};

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

    curs_set(0);

    init_color(127, 0,    0,    0);
    init_color(128, 0,    0,    666);
    init_color(129, 0,    666,  0);
    init_color(130, 0,    666,  666);
    init_color(131, 666,  0,    0);
    init_color(132, 666,  0,    666);
    init_color(133, 666,  333,  0);
    init_color(134, 1000, 1000, 1000);
    init_color(135, 333,  333,  333);
    init_color(136, 333,  333,  1000);
    init_color(137, 333,  1000, 333);
    init_color(138, 333,  1000, 1000);
    init_color(139, 1000, 333,  1000);
    init_color(140, 1000, 1000, 333);
    init_color(141, 1000, 1000, 1000);

    /* Initialize all possible color combinations */
    for (f = 0; f < 16; ++f)
        for (b = 0; b < 16; b++)
                init_pair(((f << 4) | b) + 16, f+127, b+127);

    for(;;) {
        if ((c = getch()) > 0) {
            switch (c) {
            case KEY_LEFT: c = 1; break;
            case KEY_RIGHT: c = 2; break;
            case KEY_UP: c = 3; break;
            case KEY_DOWN: c = 4; break;
            }

            cpu->ram[0x9000 + (keybuffer++ % 0x1)] = c;
        }

        dcpu16_step(cpu);

        update(cpu);

        /* Add clock cycles, then enable */
        usleep(10);

        if ((last_pc == cpu->pc) && flag_halt)
            break;

        last_pc = cpu->pc;
    }
}

void hndlresize(int sig) {
    wresize(stdscr, LINES, COLS);

    clear();
}

void updatestatus(dcpu16 *cpu) {
    box(status, 0, 0);
    mvwprintw(status, 0, 2, " CPU ");

    mvwprintw(status, 1, 2, "PC: %04X    SP: %04X", cpu->pc, cpu->sp);
    mvwprintw(status, 2, 2, " O: %04X", cpu->o);

    mvwprintw(status, 4, 2, "A: %04X", cpu->registers[0]);
    mvwprintw(status, 5, 2, "B: %04X", cpu->registers[1]);
    mvwprintw(status, 6, 2, "C: %04X", cpu->registers[2]);
    mvwprintw(status, 7, 2, "X: %04X", cpu->registers[3]);
    mvwprintw(status, 8, 2, "Y: %04X", cpu->registers[4]);
    mvwprintw(status, 9, 2, "Z: %04X", cpu->registers[5]);
    mvwprintw(status, 10, 2, "I: %04X", cpu->registers[6]);
    mvwprintw(status, 11, 2, "J: %04X", cpu->registers[7]);

    wrefresh(status);
}

void update(dcpu16 *cpu) {
    mvprintw(0, 0, "dcpu16emu");
    mvhline(1, 0, ACS_BULLET, 10);


    updatescreen(cpu);
    updatestatus(cpu);
}

void updatescreen(dcpu16 *cpu) {
    uint16_t x, y;
    uint16_t *vram = cpu->ram + 0x8000;

    box(cpuscreen, 0, 0);
    mvwprintw(cpuscreen, 0, 2, " Screen ");

    for (x = 0; x < 32; ++x) {
        for (y = 0; y < 12; ++y) {
            uint16_t word = vram[y * 32 + x];
            char asciival = word & 0x7F;

            uint8_t fg = (word & 0xF000) >> 12;
            uint8_t bg = (word & 0x0F00) >> 8;
            int id = ((fg << 4) | bg) + 16;

            /* Do something with fb and bg... */
            wmove(cpuscreen, y + 1, x + 1);
            if (isprint(asciival)) {
                mvwaddch(cpuscreen, y+1, x+1, asciival | COLOR_PAIR(id));
            } else {
                mvwaddch(cpuscreen, y+1, x+1, ' ' | COLOR_PAIR(id));
            }
        }
    }

    wrefresh(cpuscreen);
}

void disassemble(dcpu16 *cpu) {
    endwin();

    dcpu16instr instr = {0};

    for (;;) {
        dcpu16_fetch(&instr, cpu);
        inspect_instruction(cpu, &instr);
    }
}

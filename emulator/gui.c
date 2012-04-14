#include <signal.h>
#include <ncurses.h>

#include "gui.h"
#include "../common/types.h"

void updatescreen(dcpu16*);
void updatestatus(dcpu16*);
void handleresize(int sig);


void handleresize(int sig) {
    wresize(stdscr, LINES, COLS);

    clear();
}

void initgui() {
    int f, b;

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

    signal(SIGWINCH, handleresize);

    init_pair(1, COLOR_BLUE, COLOR_WHITE);

    cpuscreen = newwin(14, 34, 2, 1);
    status = newwin(14, 24, 2, 37);

    wbkgd(status, COLOR_PAIR(1));
    wbkgd(cpuscreen, COLOR_PAIR(1));

    curs_set(0);

    /*
     * Initialize colors
     */
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

    refresh();
    wrefresh(cpuscreen);
}

void cleanupgui() {
    delwin(status);
    delwin(cpuscreen);

    endwin();
}

void updategui(dcpu16 *cpu) {
    mvprintw(0, 0, "dcpu16emu");
    mvhline(1, 0, ACS_BULLET, 10);


    updatescreen(cpu);
    updatestatus(cpu);
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

#ifndef GUI_H
#define GUI_H

#include "../common/types.h"


WINDOW *status;
WINDOW *cpuscreen;

void initgui();
void cleanupgui();

void updategui(dcpu16*);

#endif

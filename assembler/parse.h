#ifndef PARSE_H
#define PARSE_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#include "../common/linked_list.h"


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

#ifndef DCPU16_H
#define DCPU16_H

#include "types.h"
#include "parse.h"


int uses_next_word(dcpu16operand*);
int instruction_length(dcpu16instruction*);

int is_stack_operation(dcpu16token);
int is_status_register(dcpu16token);
int is_instruction(dcpu16token);
int is_nonbasic_instruction(dcpu16token);
int is_macro(dcpu16token);
int is_register(dcpu16token);

#endif

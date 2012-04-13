#include "types.h"
#include "dcpu16.h"

int uses_next_word(dcpu16operand *op) {
    if (op->type == LITERAL) {
        if (op->addressing == IMMEDIATE)
            return op->numeric > 0x1f;
        else
            return 1;
    } else if (op->type == REGISTER_OFFSET) {
        return 1;
    } else if (op->type == LABEL) {
        /*
         * TODO: Short label support
         */
        return 1;
    } else {
        return 0;
    }
}

int instruction_length(dcpu16instruction *instr) {
    if (is_nonbasic_instruction(instr->opcode))
        return 1 + uses_next_word(&(instr->a));
    else
        return 1 + uses_next_word(&(instr->a)) + uses_next_word(&(instr->b));
}


int is_stack_operation(dcpu16token t) {
    return ((t >= T_POP) && (t <= T_PUSH));
}

int is_status_register(dcpu16token t) {
    return ((t >= T_SP) && (t <= T_O));
}

int is_instruction(dcpu16token t) {
    return ((t >= T_SET) && (t <= T_JSR));
}

int is_macro(dcpu16token t) {
    return ((t >= T_ORG) && (t <= T_DAT));
}

int is_nonbasic_instruction(dcpu16token t) {
    return is_instruction(t) && (t == T_JSR);
}

int is_register(dcpu16token t) {
    return ((t >= T_A) && (t <= T_J));
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "dcpu16.h"
#include "util.h"
#include "types.h"
#include "label.h"
#include "parse.h"
#include "../common/linked_list.h"

int curline = 0;
char *cur_line = NULL;
char *cur_pos  = NULL;
char *srcfile  = "<stdin>";

void readfile(FILE *f, list *lines);
void parseline(list*, list*);
dcpu16operand parseoperand(list*);

uint16_t parsenumeric();
dcpu16token parsestring();
dcpu16token parseregister(char r);

dcpu16token nexttoken();

char *toktostr(dcpu16token);
int pc;


void readfile(FILE *f, list *lines) {
    do {
        /* 1024 characters should do */
        char buffer[1024] = {0};
        char *loc = NULL;

        if (fgets(buffer, sizeof(buffer), f) != NULL) {
            int n;
            if ((loc = strstr(buffer, ";")) != NULL) {
                *loc = '\n';

                while (buffer < loc && isspace(*loc))
                    loc--;

                *(++loc) = '\0';
            }

            n = strlen(buffer);
            
            char *line = malloc(n * sizeof(char) + 1);
            memset(line, 0, n * sizeof(char) + 1);
            memcpy(line, buffer, n * sizeof(char));

            list_push_back(lines, line);
        }
    } while (!feof(f));
}

void parsefile(FILE *f, list *instructions, list *labels) {
    list *lines = list_create();
    list_node *n;
    readfile(f, lines);

    for (n = list_get_root(lines); n != NULL; n = n->next) {
        cur_pos = cur_line = n->data;
        curline++;

        parseline(instructions, labels);
    }

    list_dispose(&lines, &free);
}

void parseline(list *instructions, list *labels) {
start:
    ;;
    dcpu16token tok = nexttoken();

    if (tok == T_COLON) {
        /* Label definition */
        if ((tok = nexttoken()) == T_IDENTIFIER) {
            dcpu16label *l = getnewlabel(labels, cur_tok.string);

            if (l->defined)
                error("Redefinition of label '%s' (%04X -> %04X) forbidden",
                        l->label, l->pc, pc);

            l->pc = pc;
            l->defined = 1;

            goto start;
        } else {
            error("Expected label, got %s", toktostr(tok));
        }
    } else if (is_instruction(tok)) {
        dcpu16instruction instr, *newinstr;

        instr.opcode = tok;
        instr.a = parseoperand(labels);

        if (!is_nonbasic_instruction(tok)) {
            if ((tok = nexttoken()) == T_COMMA) {
                instr.b = parseoperand(labels);
            } else {
                error("Expected ',', got %s", toktostr(tok));
            }
        }

        if ((tok = nexttoken()) != T_NEWLINE)
            error("Expected EOL, got %s", toktostr(tok));

        /*
         * All tokens valid, store instructions, advance PC
         */
        newinstr = malloc(sizeof(dcpu16instruction));
        memcpy(newinstr, &instr, sizeof(dcpu16instruction));

        newinstr->pc = pc;
        newinstr->line = curline;

        list_push_back(instructions, newinstr);

        pc += instruction_length(newinstr);

    } else if (is_macro(tok)) {
        if (tok == T_ORG) {
            if ((tok = nexttoken()) == T_NUMBER) {
                if (flag_paranoid && (cur_tok.number < pc)) {
                    warning("new origin precedes old origin! "
                            "This could cause already written code to be "
                            "overriden.");
                }

                pc = cur_tok.number;
            } else {
                error("Expected numeric, got %s", toktostr(tok));
            }
        } else if (tok == T_DAT) {
            /* All of the stuff we are about to read, neatly packed
             * into words */
            list_node *p = NULL;
            list *data = list_create();

            do {
                if ((tok = nexttoken()) == T_STRING) {
                    char *ptr = cur_tok.string;

                    while (*ptr != '\0') {
                        uint16_t *dat = malloc(sizeof(uint16_t));
                        *dat = *ptr++;

                        list_push_back(data, dat);
                    }
                } else if (tok == T_NUMBER) {
                    uint16_t *dat = malloc(sizeof(uint16_t));
                    *dat = cur_tok.number;

                    list_push_back(data, dat);
                } else {
                    error("Expected string or numeric, got %s", toktostr(tok));
                }

                if ((tok = nexttoken()) == T_COMMA)
                    continue;
                else if (tok == T_NEWLINE)
                    break;
                else
                    error("Expected ',' or EOL, got %s", toktostr(tok));
            } while (1);

            /*
            ram[pc++] = cur_tok.number;
            */
            for (p = list_get_root(data); p != NULL; p = p->next) {
                ram[pc++] = *((uint16_t*)p->data);
            }

            list_dispose(&data, &free);
        }
    } else if (tok == T_NEWLINE) {
        return;
    } else {
        error("Expected label-definition or opcode, got %s", toktostr(tok));
    }
}

dcpu16operand parseoperand(list *labels) {
    dcpu16operand op = {0};
    dcpu16token tok;

    op.addressing = IMMEDIATE;

    if ((tok = nexttoken()) == T_IDENTIFIER) {
        /* label */
        op.type = LABEL;
        op.label = getnewlabel(labels, cur_tok.string)->label;
    } else if (tok == T_NUMBER) {
        /* numeric */
        op.type = LITERAL;
        op.numeric = cur_tok.number;
    } else if (is_register(tok) || is_status_register(tok)
                                || is_stack_operation(tok)) {
        /* register */

        /* 
         * For all intents and purposes, stack operations, registers and
         * status registers are equivalent in usage for immediate addressing,
         * so we might as well classify them all as REGISTER
         */
        op.type = REGISTER;
        op.token = tok;
    } else if (tok == T_LBRACK) {
        /* [reference] */
        dcpu16token a = nexttoken();

        op.addressing = REFERENCE;

        if (a == T_NUMBER) {
            /* [numeric] */
            op.type = LITERAL;
            op.numeric = cur_tok.number;
        } else if (is_register(a)) {
            /* [register] */
            op.type = REGISTER;
            op.token = a;
        } else if (a == T_IDENTIFIER) {
            /* [label] */
            op.type = LABEL;
            op.label = getnewlabel(labels, cur_tok.string)->label;
        } else {
            error("Expected numeric, register or label, got %s", toktostr(a));
        }

        /* First part parsed correctly so far, now either ']' or '+'
         * follows */
        if (((tok = nexttoken()) != T_RBRACK) && (tok != T_PLUS))
            error("Expected '+' or ']', got %s", toktostr(tok));

        if (tok == T_RBRACK) {
            /* [numeric], [register], [label] checks out */
            return op;
        } else {
            dcpu16token b = nexttoken();

            op.type = REGISTER_OFFSET;

            if (is_register(a)) {
                /* [register + label], [register + numeric] */
                if (b == T_IDENTIFIER) {
                    op.register_offset.type = LABEL;
                    op.register_offset.register_index = a;
                    op.register_offset.label =
                        getnewlabel(labels, cur_tok.string)->label;

                } else if (b == T_NUMBER) {
                    op.register_offset.type = LITERAL;
                    op.register_offset.register_index = a;
                    op.register_offset.offset = cur_tok.number;

                } else {
                    error("Expected numeric or label, got %s", toktostr(b));
                }
            } else {
                /* [numeric + register], [label + register] */
                if (is_register(b)) {
                    op.register_offset.register_index = b;

                    if (a == T_NUMBER) {
                        op.register_offset.type = LITERAL;
                        op.register_offset.offset = cur_tok.number;
                    } else {
                        op.register_offset.type = LABEL;
                        op.register_offset.label =
                            getnewlabel(labels, cur_tok.string)->label;
                    }
                } else  {
                    error("Expected register, got %s", toktostr(b));
                }
            }

            if (nexttoken() != T_RBRACK)
                error("Expected ']'");
        }
    } else {
        error("Expected register, label, numeric or '[', got %s",
                toktostr(tok));
    }

    return op;
}

uint16_t parsenumeric() {
    unsigned int num;

    if (!strncmp(cur_pos, "0x", 2))
       sscanf(cur_pos, "0x%X", (unsigned int *)&num);
    else if ((isdigit(*cur_pos)))
       sscanf(cur_pos, "%d", (int *)&num);
    else
       error("Expected numeric, got '%s'", cur_pos);

    while (isxdigit(*cur_pos) || (*cur_pos == 'x'))
       cur_pos++;

    if (num > 0xFFFF)
        warning("Literal value %X too big (> 0xFFFF) -- will wrap around.",
                num);

    return num;
}

dcpu16token parsestring() {
    char chr;
    size_t i = 0;

    while (*cur_pos != '\"') {
        if (*cur_pos == '\0') {
           error("Expected '\"', got EOL");
        } else if (*cur_pos == '\\') {
            switch (*++cur_pos) {
            case '\"': chr = '\"'; break;
            case '\\': chr = '\\'; break;
            case '\t': chr = '\t'; break;
            case '\r': chr = '\r'; break;
            default: error("Unknown escape character '%c'", *cur_pos);
            }

            cur_pos++;
        } else {
            chr = *cur_pos++;
        }

        if (i >= (sizeof(cur_tok.string) - 1))
            break;

        cur_tok.string[i++] = chr;
    }

    cur_pos++;
    cur_tok.string[i] = '\0';

    return T_STRING;
}

dcpu16token parseregister(char reg) {
    switch (toupper(reg)) {
    case 'A': return T_A;
    case 'B': return T_B;
    case 'C': return T_C;
    case 'X': return T_X;
    case 'Y': return T_Y;
    case 'Z': return T_Z;
    case 'I': return T_I;
    case 'J': return T_J;
    default:
        error("Expected 'A', 'B', 'C', 'X', 'Y', 'Z', 'I' or 'J', got '%c'",
                reg);
    }

    /* Since this can't happen, just make the compiler shut up */
    return 0xFF;
}

dcpu16token nexttoken() {
#define return_(x) /*printf("%d:%s\n", curline, #x);*/ return x;
    /* Skip all spaces TO the next token */
    while (isspace(*cur_pos))
        cur_pos++;

    /* Test some operators */
    switch (*cur_pos++) {
    case '\0':
    case '\n': return_(T_NEWLINE);
    case ',': return_(T_COMMA);
    case '[': return_(T_LBRACK);
    case ']': return_(T_RBRACK);
    case '+': return_(T_PLUS);
    case ':': return_(T_COLON);
    case '\"': return parsestring(&cur_pos);
    default: break;
    }

    cur_pos--;

#define TRY(i) if (!strncasecmp(cur_pos, #i, strlen(#i))) {     \
    if (!isalnum(*(cur_pos + strlen(#i)))) {                    \
        cur_pos += strlen(#i);                                  \
        return_(T_ ## i);                                       \
    }                                                           \
}

#define TRYM(i) if (!strncasecmp(cur_pos, "." #i, strlen(#i) + 1)) { \
    cur_pos += strlen(#i) + 1;                                       \
    return_(T_ ## i);                                                \
}

    /* Try assembler pseudo instructions */
    TRYM(ORG); TRYM(DAT);

    /* Try instructions */
    TRY(SET); TRY(ADD); TRY(SUB); TRY(MUL); TRY(DIV); TRY(MOD); TRY(SHL);
    TRY(SHR); TRY(AND); TRY(BOR); TRY(XOR); TRY(IFE); TRY(IFN); TRY(IFG);
    TRY(IFB); TRY(JSR);

    /* Compatibility for other assemblers */
    TRY(DAT); TRY(ORG);

    /* And some "special" registers */
    TRY(POP); TRY(PEEK); TRY(PUSH); TRY(SP); TRY(PC); TRY(O);

#undef TRY
#undef TRYM

    if (isalpha(*cur_pos)) {
        int strlength = 0;

        /* Register or label */
        while (isalnum(*cur_pos) || (*cur_pos == '_')) {
            cur_pos++;
            strlength++;
        }

        if (strlength > 1) {
            strncpy(cur_tok.string, cur_pos - strlength, strlength);
            cur_tok.string[strlength] = '\0';

            return_(T_IDENTIFIER);
        } else {
            return parseregister(*(cur_pos - strlength));
        }
    } else if (isdigit(*cur_pos)) {
        cur_tok.number = parsenumeric();

        return_(T_NUMBER);
    }

    error("Unrecognized input '%c'", *cur_pos);
    return T_NEWLINE;
#undef return_
}

char *toktostr(dcpu16token t) {
    switch (t) {
        case T_A: case T_B: case T_C: case T_X: case T_Y: case T_Z:
        case T_I: case T_J:
            return "register";
        case T_IDENTIFIER:
            return "label";
        case T_NUMBER:
            return "numeric";
        case T_LBRACK:
            return "'['";
        case T_RBRACK:
            return "']'";
        case T_COLON:
            return "':'";
        case T_COMMA:
            return "','";
        case T_POP: case T_PUSH: case T_PEEK:
            return "stack-operation";

        case T_PC: case T_O: case T_SP:
            return "status-register";

        case T_NEWLINE:
            return "EOL";

        default:
            return "unknown";
    }
}

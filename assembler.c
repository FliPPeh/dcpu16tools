#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>

#include "linked_list.h"

/*
 * Maximum length of a label
 */
#define MAXLABEL 64

/*
 * Tokens
 */
typedef enum {
    T_STRING, /* Any string */
    T_NUMBER, /* Any number */
    T_LBRACK, /* '[' */
    T_RBRACK, /* ']' */
    T_COMMA,  /* ',' */
    T_COLON,  /* ':' */
    T_PLUS,   /* '+' */
    T_A, T_B, T_C, T_X, T_Y, T_Z, T_I, T_J, /* Registers */
    T_POP, T_PEEK, T_PUSH, /* POP, PEEK, PUSH */
    T_SP, T_PC, T_O,
    
    T_SET, T_ADD, T_SUB, T_MUL, T_DIV, T_MOD, T_SHL,
    T_SHR, T_AND, T_BOR, T_XOR, T_IFE, T_IFN, T_IFG, T_IFB,

    T_JSR,
    T_ORG  /* Assembler macros */
    T_NEWLINE
} dcpu16token;


/*
 * Parser state
 */
static int curline = 0;
static char *cur_line;
static char *cur_pos;
char *srcfile = "<stdin>";

union {
    char string[256];
    uint16_t number;
} cur_tok;

/*
 * An entry in the global label list
 */
typedef struct {
    char *label;
    uint16_t pc;

    int defined;
} dcpu16label;

/*
 * Specifies whether an operand is an immediate
 * value or a reference inside the RAM at that
 * position
 */
typedef enum {
    IMMEDIATE, REFERENCE
} dcpu16addressing;

/*
 * Specifies the type of operand, where
 *    LITERAL           = 0x0000 - 0xFFFFF
 *    LABEL             = an alphanumeric alies for the PC it was defined at
 *    REGISTER          = A, B, C, X, Y, Z, I, J
 *                          also POP, PUSH, PEEK, PC, SP, O in immediate context
 *    REGISTER_OFFSET   = Only valid as reference: [A + 0xFFFF]
 */
typedef enum {
    LITERAL, LABEL, REGISTER, REGISTER_OFFSET
} dcpu16operandtype;

typedef struct {
    /* Reuse operandtype here.  Only allowed values are LITERAL and LABEL */
    dcpu16operandtype type;
    dcpu16token register_index;

    union {
        uint16_t offset;
        char *label;
    };
} dcpu16registeroffset;

typedef struct {
    dcpu16addressing addressing;
    dcpu16operandtype type;

    union {
        dcpu16token token;
        uint16_t numeric;
        char *label;
        dcpu16registeroffset register_offset;
    };
} dcpu16operand;

typedef struct {
    uint16_t pc;
    dcpu16token opcode;
    dcpu16operand a;
    dcpu16operand b;
} dcpu16instruction;

void error(const char*, ...);

void display_help();
void strip_comments(list*);

void read_file(FILE*, list*);

void assemble_line();
dcpu16operand assemble_operand();

int uses_next_word(dcpu16operand*);

dcpu16label *get_labeling(const char*);
dcpu16label *get_label(const char*);

int parse(list*);
dcpu16token next_token();

void write_memory();

/*
 * Options and output parameters
 */
static int flag_littleendian = 0;
static list *labels;
static list *instructions;
static uint16_t ram[0x10000];

/*
 * To keep track of label positions
 */
static uint16_t pc;


void free_label(void *d) {
    dcpu16label *l = d;

    free(l->label);
    free(l);
}

/*
 * TODO: 
 *   Data sections (dat)
 *     - :data dat "str", 0xXXXXXX, "str2"
 *     - Write the data at that position (data-> beginning of data, dat = macro)
 *   Constrain labels to [_a-zA-Z][a-zA-Z0-9_]+ (C-Style)
 */
int main(int argc, char **argv) {
    int lopts_index = 0;
    char outfile[256] = "out.bin";

    FILE *input = stdin;
    FILE *output = NULL;

    list *file_contents = list_create();
    labels = list_create();
    instructions = list_create();

    static struct option lopts[] = {
        {"littleendian", no_argument, NULL, 'l'},
        {"help",         no_argument, NULL, 'h'},
        {NULL,           0,           NULL,  0 }
    };

    for (;;) {
        int opt = getopt_long(argc, argv, "lho:", lopts, &lopts_index);

        if (opt < 0)
            break;

        switch (opt) {
        case 0:
            break;

        case 'l':
            flag_littleendian = 1;
            break;

        case 'h':
            display_help();
            return 0;

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

    /* Read and cleanup */
    read_file(input, file_contents);

    /* Parse the file, storing labels and instructions as we go */
    parse(file_contents);

    curline = -1;

    list_node *t;
    for (t = list_get_root(labels); t != NULL; t = t->next) {
        dcpu16label *l = t->data;

        printf("Label: '%s' (%04X) (%d)\n", l->label, l->pc, l->defined);
    }

    write_memory();

    fwrite(ram, sizeof(uint16_t), pc, output);

    /* Release resources */
    list_dispose(&labels, &free_label);
    list_dispose(&file_contents, &free);
    list_dispose(&instructions, &free);

    if (input != stdin)
        fclose(input);

    fclose(output);

    return 0;
}

void error(const char *fmt, ...) {
    va_list args;
    char errmsg[512] = {0};
    va_start(args, fmt);

    vsnprintf(errmsg, sizeof(errmsg) - 1, fmt, args);
    va_end(args);

    if (curline > 0)
        fprintf(stderr, "%s:%d:%d: %s\n", 
                srcfile, curline, cur_pos - cur_line, errmsg);
    else
        fprintf(stderr, "%s: %s\n", srcfile, errmsg);

    /* Let's have the operating system handle our unclosed files */
    exit(1);
}

void read_file(FILE *f, list *lines) {
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

uint16_t encode_opcode(dcpu16token t) {
    if (is_nonbasic_instruction(t)) {
            if (t == T_JSR)
                return 0x0001;
    } else {
        return (t - T_SET) + 1;
    }
}

uint16_t encode_value(dcpu16operand *op, uint16_t *wop) {
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
        dcpu16label *l = get_labeling(op->label);
        if ((l != NULL) && l->defined) {
            /*
             * TODO: Short labels
             */
            *wop = l->pc;
            return (op->addressing == IMMEDIATE ? 0x1f : 0x1e);
        } else {
            error("Unresolved label '%s'", op->label);
        }
    } else {
        if (op->register_offset.type == LABEL) {
            dcpu16label *l = get_labeling(op->register_offset.label);

            if ((l != NULL) && l->defined)
                *wop = l->pc;
            else
                error("Unresolved label '%s'", op->register_offset.label);

        } else {
            *wop = op->register_offset.offset;
        }

        return (op->register_offset.register_index - T_A) + 0x10;
    }
}

void encode(dcpu16instruction *i, uint16_t *wi, uint16_t *wa, uint16_t *wb) {
    if (is_nonbasic_instruction(i->opcode))
        *wi = 0x00 | encode_opcode(i->opcode) << 4
                   | encode_value(&(i->a), wa) << 10;
    else
        *wi = encode_opcode(i->opcode)
            | encode_value(&(i->a), wa) << 4
            | encode_value(&(i->b), wb) << 10;
}

void write_memory() {
    /*
     * Purposeful shadowing of the global 'pc'
     */
    uint16_t pc;

    list_node *n = NULL;

    for (n = list_get_root(instructions); n != NULL; n = n->next) {
        uint16_t op, a, b;
        dcpu16instruction *i = n->data;

        pc = i->pc;
        encode(i, &op, &a, &b);

        ram[pc++] = op;

        if (uses_next_word(&(i->a)))
            ram[pc++] = a;

        if (!is_nonbasic_instruction(i->opcode))
            if (uses_next_word(&(i->b)))
                ram[pc++] = b;
    }
}

char *toktostr(dcpu16token t) {
    switch (t) {
        case T_A: case T_B: case T_C: case T_X: case T_Y: case T_Z:
        case T_I: case T_J:
            return "register";
        case T_STRING:
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

        default:
            return "unknown";
    }
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
    return (t == T_ORG);
}

int is_nonbasic_instruction(dcpu16token t) {
    return is_instruction(t) && (t == T_JSR);
}

int is_register(dcpu16token t) {
    return ((t >= T_A) && (t <= T_J));
}

dcpu16label *get_labeling(const char *label) {
    list_node *node = NULL;

    for (node = list_get_root(labels); node != NULL; node = node->next) {
        dcpu16label *ptr = node->data;

        if (!(strcmp(ptr->label, label)))
            return ptr;
    }
    
    return NULL;
}

dcpu16label *get_label(const char *label) {
    dcpu16label *ptr = get_labeling(label);
    
    if (ptr != NULL)
        return ptr;

    /*
     * Label was not found, so we create it
     */
    ptr = malloc(sizeof(dcpu16label));
    ptr->label = malloc(strlen(label) * sizeof(char) + 1);
    strcpy(ptr->label, label);

    ptr->pc = 0;
    ptr->defined = 0;

    list_push_back(labels, ptr);

    return ptr;
}

dcpu16operand assemble_operand() {
    dcpu16operand op = {0};
    dcpu16token tok;

    op.addressing = IMMEDIATE;

    if ((tok = next_token()) == T_STRING) {
        /* label */
        op.type = LABEL;
        op.label = get_label(cur_tok.string)->label;
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
        dcpu16token a = next_token();

        op.addressing = REFERENCE;

        if (a == T_NUMBER) {
            /* [numeric] */
            op.type = LITERAL;
            op.numeric = cur_tok.number;
        } else if (is_register(a)) {
            /* [register] */
            op.type = REGISTER;
            op.token = a;
        } else if (a == T_STRING) {
            /* [label] */
            op.type = LABEL;
            op.label = get_label(cur_tok.string)->label;
        } else {
            error("Expected numeric, register or label, got %s", toktostr(a));
        }

        /* First part parsed correctly so far, now either ']' or '+'
         * follows */
        if (((tok = next_token()) != T_RBRACK) && (tok != T_PLUS))
            error("Expected '+' or ']', got %s", toktostr(tok));

        if (tok == T_RBRACK) {
            /* [numeric], [register], [label] checks out */
            return op;
        } else {
            dcpu16token b = next_token();

            op.type = REGISTER_OFFSET;

            if (is_register(a)) {
                /* [register + label], [register + numeric] */
                if (b == T_STRING) {
                    op.register_offset.type = LABEL;
                    op.register_offset.register_index = a;
                    op.register_offset.label = get_label(cur_tok.string)->label;

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
                            get_label(cur_tok.string)->label;
                    }
                } else  {
                    error("Expected register, got %s", toktostr(b));
                }
            }

            if (next_token() != T_RBRACK)
                error("Expected ']'");
        }
    } else {
        error("Expected register, label, numeric or '[', got %s",
                toktostr(tok));
    }

    return op;
}

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

void assemble_line() {
start:
    ;;
    dcpu16token tok = next_token();

    if (tok == T_COLON) {
        /* Label definition */
        if ((tok = next_token()) == T_STRING) {
            dcpu16label *l = get_label(cur_tok.string);

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
        instr.a = assemble_operand();

        if (!is_nonbasic_instruction(tok)) {
            if ((tok = next_token()) == T_COMMA) {
                instr.b = assemble_operand();
            } else {
                error("Expected ',', got %s", toktostr(tok));
            }
        }

        if ((tok = next_token()) != T_NEWLINE)
            error("Expected EOL, got %s", toktostr(tok));

        /*
         * All tokens valid, store instructions, advance PC
         */
        newinstr = malloc(sizeof(dcpu16instruction));
        memcpy(newinstr, &instr, sizeof(dcpu16instruction));

        newinstr->pc = pc;

        list_push_back(instructions, newinstr);

        pc += instruction_length(newinstr);

    } else if (is_macro(tok)) {
        if (tok == T_ORG) {
            if ((tok = next_token()) == T_NUMBER)
                pc = cur_tok.number;
            else
                error("Expected numeric, got %s", toktostr(tok));
        }
    } else if (tok == T_NEWLINE) {
        return;
    } else {
        error("Expected label-definition or opcode, got %s", toktostr(tok));
    }
}

int parse(list *lines) {
    list_node *n;

    for (n = list_get_root(lines); n != NULL; n = n->next) {
        char *start = cur_pos = cur_line = n->data;
        curline++;

        assemble_line();        
    }

    return 0;
}

uint16_t read_numeric(char *op) {
    unsigned int num;

    if (!strncmp(op, "0x", 2))
       sscanf(op, "0x%X", (unsigned int *)&num);
    else if ((isdigit(*op)))
       sscanf(op, "%d", (int *)&num);
    else
       error("Expected numeric, got '%s'", op);    

    return num;
}

uint8_t parse_register(char reg) {
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

dcpu16token next_token() {
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
    default: break;
    }

    cur_pos--;

#define TRY(i) if (!strncasecmp(cur_pos, #i, strlen(#i))) { \
    cur_pos += strlen(#i);                                  \
    return_(T_ ## i);                                       \
}

#define TRYM(i) if (!strncasecmp(cur_pos, "." #i, strlen(#i) + 1)) { \
    cur_pos += strlen(#i) + 1;                                       \
    return_(T_ ## i);                                                \
}

    /* Try assembler pseudo instructions */
    TRYM(ORG);

    /* Try instructions */
    TRY(SET); TRY(ADD); TRY(SUB); TRY(MUL); TRY(DIV); TRY(MOD); TRY(SHL);
    TRY(SHR); TRY(AND); TRY(BOR); TRY(XOR); TRY(IFE); TRY(IFN); TRY(IFG);
    TRY(IFB); TRY(JSR);

    /* And some "special" registers */
    TRY(POP); TRY(PEEK); TRY(PUSH); TRY(SP); TRY(PC); TRY(O);

#undef TRY
#undef TRYM

    if (isalpha(*cur_pos)) {
        int strlength = 0;

        /* Register or label */
        while (isalnum(*cur_pos++))
            strlength++;

        cur_pos--;

        if (strlength > 1) {
            strncpy(cur_tok.string, cur_pos - strlength, strlength);
            cur_tok.string[strlength] = '\0';

            return_(T_STRING);
        } else {
            return parse_register(*(cur_pos - strlength));
        }
    } else if (isdigit(*cur_pos)) {
        cur_tok.number = read_numeric(cur_pos);

        while (isxdigit(*cur_pos) || (*cur_pos == 'x'))
            cur_pos++;

        return_(T_NUMBER);
    }

    error("Unrecognized input '%c'", *cur_pos);
    return T_NEWLINE;
#undef return_
}

void display_help() {
    printf("Usage: dcpu16asm [OPTIOMS] [FILENAME]\n"
           "where OPTIONS is any of:\n"
           "  -h, --help          Display this help\n"
           "  -l, --littleendian  Generate little endian bytecode "
                                 "rather than big endian\n"
           "  -o FILENAME         Write output to FILENAME instead of "
                                 "\"out.bin\"\n"
           "\n"
           "FILENAME, if given, is the source file to read instructions from.\n"
           "Defaults to standard input if not given or filename is \"-\"\n");
}

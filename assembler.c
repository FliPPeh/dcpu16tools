#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>

#include "linked_list.h"

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
    T_OP, T_PC, T_O, T_SP,
    T_SET, T_ADD, T_SUB, T_MUL, T_DIV, T_MOD, T_SHL, T_SHR,
    T_AND, T_BOR, T_XOR, T_IFE, T_IFN, T_IFG, T_IFB, T_JSR,
    T_NEWLINE
} dcpu16token;

static char *cur_pos;
static int line_start;

union {
    char string[256];
    uint16_t number;
} cur_tok;

typedef struct {
    char label[256];
    uint16_t pc;
} dcpu16label;

void error(const char*, ...);

void display_help();
void strip_comments(list*);
void replace_labels(list*, list*);

void read_file(FILE*, list*);
int read_operand(char**, const char*);

int parse(list*);
void parse_operand(char*);
dcpu16token next_token();

void write_bin(list*, FILE*);

char *srcfile = "<stdin>";
int curline = 0;

static int flag_littleendian = 0;

/*
 * TODO: 
 *   Data sections (dat)
 *     - :data dat "str", 0xXXXXXX, "str2"
 *     - Write the data at that position (data-> beginning of data, dat = macro)
 *   Maybe build a real tokenizer
 */
int main(int argc, char **argv) {
    int lopts_index = 0;
    char outfile[256] = "out.bin";

    FILE *input = stdin;
    FILE *output = NULL;

    list *file_contents = list_create();
    list *labels = list_create();

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
    strip_comments(file_contents);

    /* Parse the file, storing labels and instructions as we go */
    parse(file_contents);

    curline = -1;

    /* Release resources */
    list_dispose(&labels, &free);
    list_dispose(&file_contents, &free);

    if (input != stdin)
        fclose(input);

    fclose(output);

    return 0;
}

uint16_t get_label_position(list *lbls, const char *lbl) {
    list_node *node = list_get_root(lbls);

    while (node != NULL) {
        dcpu16label *ptr = node->data;

        if (!strcmp(ptr->label, lbl))
            return ptr->pc;

        node = node->next;
    }

    error("Unresolved label '%s'\n", lbl);
    return 0xFFFF;
}

void error(const char *fmt, ...) {
    va_list args;
    char errmsg[512] = {0};
    va_start(args, fmt);

    vsnprintf(errmsg, sizeof(errmsg) - 1, fmt, args);
    va_end(args);

    if (curline > 0)
        fprintf(stderr, "%s:%d: %s\n", srcfile, curline, errmsg);
    else
        fprintf(stderr, "%s: %s\n", srcfile, errmsg);

    /* Let's have the operating system handle our unclosed files */
    exit(1);
}

void read_file(FILE *f, list *lines) {
    do {
        /* 1024 characters should do */
        char buffer[1024] = {0};

        if (fgets(buffer, sizeof(buffer), f) != NULL) {
            int n = strlen(buffer);
            
            char *line = malloc(n * sizeof(char) + 1);
            memset(line, 0, n * sizeof(char) + 1);
            memcpy(line, buffer, n * sizeof(char));

            list_push_back(lines, line);
        }
    } while (!feof(f));
}

void strip_comments(list *lines) {
    list_node *n;

    for (n = list_get_root(lines); n != NULL; n = n->next) {
        char *loc = NULL;
        if ((loc = strstr(n->data, ";")) != NULL) {
            *loc = '\n';

            while ((char*)n->data < loc && isspace(*loc))
                loc--;

            ++loc;
            *loc = 0;
        }
    }
}

char *toktostr(dcpu16token t) {
    switch (t) {
        case T_A: case T_B: case T_C:
        case T_X: case T_Y: case T_Z:
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

        default:
            return "unknown";
    }
}

int is_instruction(dcpu16token t) {
    switch (t) {
    case T_SET: case T_ADD: case T_SUB: case T_MUL:
    case T_DIV: case T_MOD: case T_SHL: case T_SHR:
    case T_AND: case T_BOR: case T_XOR: case T_IFE:
    case T_IFN: case T_IFG: case T_IFB: case T_JSR: return 1;
    default: return 0;
    }
}

int is_nonbasic_instruction(dcpu16token t) {
    return is_instruction(t) && (t == T_JSR);
}

int is_register(dcpu16token t) {
    switch (t) {
    case T_A: case T_B: case T_C:
    case T_X: case T_Y: case T_Z:
    case T_I: case T_J:
    case T_POP: case T_PEEK: case T_PUSH:
    case T_OP: case T_PC: case T_O: case T_SP:
        return 1;
    default:
        return 0;
    }
}

void assemble_operand() {
    dcpu16token tok;

    if ((tok = next_token()) == T_STRING) {
        /* label */
    } else if (tok == T_NUMBER) {
        /* numeric */
    } else if (is_register(tok)) {
        /* register */
    } else if (tok == T_LBRACK) {
        dcpu16token a = next_token();
        /* [reference] */
        if (a == T_NUMBER) {
            /* [numeric] */
        } else if (is_register(a)) {
            /* [register] */
        } else if (a == T_STRING) {
            /* [label] */
        } else {
            error("Expected numeric, register or label, got %s", toktostr(a));
        }

        /* First part parsed correctly so far, now either ']' or '+'
         * follows */
        if (((tok = next_token()) != T_RBRACK) && (tok != T_PLUS))
            error("Expected '+' or ']', got %s", toktostr(tok));

        if (tok == T_RBRACK) {
            /* [numeric], [register], [label] checks out */
            return;
        } else {
            dcpu16token b = next_token();

            if (is_register(a)) {
                /* [register + label], [register + numeric] */
                if (b == T_STRING) {

                } else if (b == T_NUMBER) {

                } else {
                    error("Expected numeric or label, got %s", toktostr(b));
                }
            } else {
                /* [numeric + register], [label + register] */
                if (is_register(b)) {

                } else  {
                    error("Expected register, got %s", toktostr(b));
                }
            }

            if (next_token() != T_RBRACK)
                error("Expected ']'");
        }
    }
}

void assemble_line() {
start:
    ;;
    dcpu16token tok = next_token();

    if (tok == T_COLON) {
        /* Label definition */
        if ((tok = next_token()) == T_STRING) {
            printf("Label: %s\n", cur_tok.string);

            goto start;
        } else {
            error("Expected string, got %s", toktostr(tok));
        }
    } else if (is_instruction(tok)) {
        dcpu16token instructions = tok;

        assemble_operand();

        if (!is_nonbasic_instruction(tok)) {
            if ((tok = next_token()) == T_COMMA) {
                assemble_operand();
            } else {
                error("Expected ',', got %s", toktostr(tok));
            }
        }

        if ((tok = next_token()) != T_NEWLINE)
            error("Expected EOL, got %s", toktostr(tok));

    } else if (tok == T_NEWLINE) {
        /* Nothing to do */
    } else {
        error("Expected label-definition or opcode, got %s", toktostr(tok));
    }
}

int parse(list *lines) {
    list_node *n;
    uint16_t instrlen = 0;

    for (n = list_get_root(lines); n != NULL; n = n->next) {
        char *start = n->data;
        cur_pos = start;
        curline++;

        line_start = 1;

        assemble_line();        
    }

    return instrlen;
}

uint16_t read_numeric(char *op) {
    unsigned int num;

    if (!strncmp(op, "0x", 2)) {
        sscanf(op, "0x%x", (unsigned int *)&num);
    } else if ((isdigit(*op)) && (*op != '0')) {
        /* Since normal numbers don't begin with a zero */
        sscanf(op, "%d", (int *)&num);
    } else {
        error("Expected numeric, got '%s'", op);
    }

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
#define return_(x) printf("%d:%s\n", curline, #x); return x;
    /*
    if (!line_start) {
        while (!isspace(*cur_pos))
            if (*cur_pos == '\0')
                return T_NEWLINE;
            else
                cur_pos++;
    }
    */
    line_start = 0;

    /* Then skip all spaces TO the next token */
    while (isspace(*cur_pos)) {
        cur_pos++;
    }

    printf("%d: '%s'\n", curline, cur_pos);

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
    return T_ ## i;                                         \
}

    /* Try instructions */
    TRY(SET); TRY(ADD); TRY(SUB); TRY(MUL);
    TRY(DIV); TRY(MOD); TRY(SHL); TRY(SHR);
    TRY(AND); TRY(BOR); TRY(XOR); TRY(IFE); 
    TRY(IFN); TRY(IFG); TRY(IFB); TRY(JSR);

    /* And some "special" registers */
    TRY(POP); TRY(PEEK); TRY(PUSH);
    TRY(SP); TRY(PC); TRY(O);

#undef TRY

    if (isalpha(*cur_pos)) {
        int strlength = 0;

        /* Register or label */
        while (isalnum(*cur_pos++))
            strlength++;

        cur_pos--;

        if (strlength > 1) {
            strncpy(cur_tok.string, cur_pos - strlength - 1, strlength);
            cur_tok.string[strlength] = '\0';

            return_(T_STRING);
        } else {
            return parse_register(*(cur_pos - strlength));
        }
    } else if (isdigit(*cur_pos)) {
        cur_tok.number = read_numeric(cur_pos);

        while (isdigit(*cur_pos) || (*cur_pos == 'x'))
            cur_pos++;

        return_(T_NUMBER);
    }

    error("Unrecognized input '%s'", cur_pos);
    return T_NEWLINE;
#undef return
}

dcpu16token parse_opcode(const char *opstr) {
    /* Sorry... */
#define TRY(op) if (!strcasecmp(opstr, #op)) return T_ ## op;

    TRY(SET); TRY(ADD); TRY(SUB); TRY(MUL);
    TRY(DIV); TRY(MOD); TRY(SHL); TRY(SHR);
    TRY(AND); TRY(BOR); TRY(XOR); TRY(IFE); 
    TRY(IFN); TRY(IFG); TRY(IFB); TRY(JSR);

    return -1;
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

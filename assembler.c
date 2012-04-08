#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>


typedef enum {
    JSR, SET, ADD, SUB,
    MUL, DIV, MOD, SHL,
    SHR, AND, BOR, XOR,
    IFE, IFN, IFG, IFB,
    INV
} dcpu16opcode;

typedef enum {
    REGISTER,               /*  A  */
    REGISTER_INDIRECT,      /* [A] */
    REGISTER_PLUS_LITERAL,  /* [0xXXXX + A] & [A + 0xXXXX] */
    POP, PEEK, PUSH, SP, PC, O,
    LITERAL,                /* 
                             * 0x0 - 0xFFFF
                             *   0x0 - 0x1f = 0x20 + literal
                             *   >0x1f      = next word
                             */
    LITERAL_INDIRECT        /* [0xXXXX] */
} dcpu16operandtype;

typedef struct {
    dcpu16operandtype type;

    /* TODO: "value" is a relict, should be "a" and "b". Too lazy to rename */
    uint16_t value;
    uint16_t param;
} dcpu16operand;

void error(const char*, ...);
void display_help();
void read_file(FILE*, char***);
void strip_comments(char**);
void parse(char**);
int read_operand(char**, int, const char*);
dcpu16opcode parse_opcode(const char*);
void parse_operand(const char*, dcpu16operand*);


char *srcfile = "<stdin>";
int curline = 0;

static int flag_littleendian = 0;

/*
 * TODO: 
 *   Data sections (dat)
 *   Code generation
 *   Maybe build a real tokenizer
 */
int main(int argc, char **argv) {
    int lopts_index = 0;
    int i;
    char outfile[256] = "out.bin";
    FILE *input = stdin;
    FILE *output = NULL;
    char **file_contents = NULL;

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

    printf("Assembling '%s'...\n", outfile);
    if ((output = fopen(outfile, "w+")) == NULL) {
        fprintf(stderr, "Unable to open '%s' -- aborting\n", outfile);

        if (input != stdin)
            fclose(input);

        return 1;
    }

    read_file(input, &file_contents);
    strip_comments(file_contents);

    for (i = 0;; ++i) {
        if (file_contents[i] == NULL)
            break;

        printf("% 3d  %s",  i+1, file_contents[i]);
    }

    parse(file_contents);


    /* Release resources */
    for (i = 0;; ++i) {
        if (file_contents[i] == NULL)
            break;

        free(file_contents[i]);
    }

    free(file_contents);

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

    fprintf(stderr, "%s:%d: %s\n", srcfile, curline, errmsg);

    /* Let's have the operating system handle our unclosed files */
    exit(1);
}

void read_file(FILE *f, char ***store) {
    int lines = 0;

    do {
        /* 1024 characters should do */
        char buffer[1024] = {0};

        if (fgets(buffer, sizeof(buffer), f) != NULL) {
            int n = strlen(buffer);

            *store = realloc(*store, (lines + 1) * sizeof(char*));
            if (*store != NULL) {
                char *line = malloc(n * sizeof(char) + 1);
                memset(line, 0, n * sizeof(char) + 1);
                memcpy(line, buffer, n * sizeof(char));
                (*store)[lines++] = line;
            } else {
                printf("reallocation failed -- aborting\n");
                return;
            }
        }
    } while (!feof(f));

    *store = realloc(*store, (lines + 1) * sizeof(char*));
    (*store)[lines] = NULL;
}

void strip_comments(char **lines) {
    int i;

    for (i = 0;; ++i) {
        char *loc = NULL;

        if (lines[i] == NULL)
            break;

        if ((loc = strstr(lines[i], ";")) != NULL) {
            *loc = '\n';
            *(loc + 1) = 0;
        }
    }
}

void parse(char **lines) {
    int i;

    for (i = 0;; ++i) {
        char *start = lines[i];
        char *token = NULL;
        dcpu16opcode opcode = INV;
        char *a = NULL, *b = NULL;
        dcpu16operand opa = {0}, opb = {0};

        ++curline;

        if (start == NULL)
            break;

        /* Skip any leading spaces */
        while (isspace(*start))
            start++;
        
        /* Anything left? */
        if (strlen(start) == 0)
            continue;

        token = strtok(start, " ");
        if ((token == NULL) || ((opcode = parse_opcode(token)) == INV)) {
            error("Expected opcode, got '%s'", start);
        }


        /* get a and b */
        read_operand(&a, i+1, ",");
        parse_operand(a, &opa);

        read_operand(&b, i+1, "\n");
        parse_operand(b, &opb);

        printf("%s\t%s,\t%s\n", start, a, b);
    }
}

int read_operand(char **src, int lineno, const char *sep) {
    if ((*src = strtok(NULL, sep)) != NULL) {
        while (isspace(**src))
            (*src)++;

        if (strlen(*src) == 0) {
            goto error;
        }

        return 0;
    }

error:
    error("Expected OPERAND, got '%s'", *src);
    return 1;
}

uint16_t read_numeric(const char *op) {
    unsigned int num;

    while (isspace(*op))
        op++;

    if (!strncmp(op, "0x", 2)) {
        /* Read hex digit */
        op += 2; /* Skip "0x" */

        sscanf(op, "%x", (unsigned int *)&num);
    } else if ((isdigit(*op)) && (*op != '0')) {
        /* Since normal numbers don't begin with a zero */
        sscanf(op, "%d", (int *)&num);
    } else {
        error("Expected NUMERIC, got '%s'", op);
    }

    return num;
}

uint8_t parse_register(const char **reg) {
    while (isspace(**reg))
        (*reg)++;

    switch (toupper(**reg)) {
    case 'A': (*reg)++; return 0x0;
    case 'B': (*reg)++; return 0x1;
    case 'C': (*reg)++; return 0x2;
    case 'X': (*reg)++; return 0x3;
    case 'Y': (*reg)++; return 0x4;
    case 'Z': (*reg)++; return 0x5;
    case 'I': (*reg)++; return 0x6;
    case 'J': (*reg)++; return 0x7;
    default: error("Expected REGISTER, got '%s'", *reg);
    }

    /* Since this can't happen, just make the compiler shut up */
    return 0xFF;
}

void parse_operand(const char *op, dcpu16operand *store) {
    /* Start of with the simple things */
    if (!strncasecmp(op, "POP", 3)) {
        store->type = POP;
    } else if (!strncasecmp(op, "PEEK", 4)) {
        store->type = PEEK;
    } else if (!strncasecmp(op, "PUSH", 4)) {
        store->type = PUSH;
    } else if (!strncasecmp(op, "SP", 2)) {
        store->type = SP;
    } else if (!strncasecmp(op, "PC", 2)) {
        store->type = PC;
    } else if (!strncasecmp(op, "O", 1)) {
        store->type = O;
    } else if (isalpha(*op)) {
        /* Register */
        store->type = REGISTER;
        store->value = parse_register(&op);
    } else if (isdigit(*op)) {
        uint16_t num = read_numeric(op);
        store->type = LITERAL;
        store->value = num;
    } else if (*op == '[') {
        /* Anything indirect */
        op++;

        while (isspace(*op))
            op++;

        /*
         * [Register + Literal], [Literal + Register], [Register], [Literal]
         *
         * TODO: Strip '[' and ']' after checking balance - prettier errors
         */
        if (isalpha(*op)) {
            /* [Register + Literal], [Register] */
            uint8_t register_base = parse_register(&op);

            while (isspace(*op))
                op++;

            if (*op == '+') {
                /* [Register + Literal] */
                op++;
                uint16_t n = read_numeric(op);

                store->type = REGISTER_PLUS_LITERAL;
                store->value = register_base;
                store->param = n;

                while (isspace(*op))
                    op++;

                if (*op != ']')
                    error("Expected ']', got '%s'", op);
            } else if (*op == ']') {
                /* [Register] */

                store->type = REGISTER_INDIRECT;
                store->value = register_base;
            } else {
                error("Expected '+' or ']', got '%s'", op);
            }
        } else if (isdigit(*op)) {
            /* [Literal + Register], [Literal] */
            uint16_t n = read_numeric(op);

            while (isdigit(*op) || (*op == 'x') || isspace(*op))
                op++;

            if (*op == '+') {
                /* [Literal + Register] */
                op++;
                uint8_t register_base = parse_register(&op);

                store->type = REGISTER_PLUS_LITERAL;
                store->value = register_base;
                store->param = n;

                while (isspace(*op))
                    op++;

                if (*op != ']')
                    error("Expected ']', got '%s'", op);

            } else if (*op == ']') {
                /* [Literal] */

                store->type = LITERAL_INDIRECT;
                store->value = n;
            } else {
                error("Expected '+' or ']', got '%s'", op);
            }
        }

        while (isspace(*(++op)));

        if (strlen(op) != 0)
            error("Expected ',' or EOL, got '%s'", op);

    } else {
        error("Expected OPERAND, got '%s'", op);
    }

    return;
}

dcpu16opcode parse_opcode(const char *opstr) {
    /* Sorry... */
#define TRY(op) if (!strcmp(opstr, #op)) return op;

    TRY(SET); TRY(ADD); TRY(SUB); TRY(MUL);
    TRY(DIV); TRY(MOD); TRY(SHL); TRY(SHR);
    TRY(AND); TRY(BOR); TRY(XOR); TRY(IFE); 
    TRY(IFN); TRY(IFG); TRY(IFB); TRY(JSR);

    return INV;
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

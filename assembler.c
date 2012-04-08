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

    uint16_t value;
    uint16_t next;
} dcpu16operand;

void error(const char*, ...);
void display_help();
void read_file(FILE*, char***);
void strip_comments(char**);
uint16_t assemble(char**, uint16_t**);
int read_operand(char**, int, const char*);
dcpu16opcode parse_opcode(const char*);
void parse_operand(char*, dcpu16operand*);


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
    uint16_t *program = NULL;
    uint16_t programlen = 0;

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

    programlen = assemble(file_contents, &program);

    if (flag_littleendian == 1) {
        for (i = 0; i < programlen; ++i) {
            program[i] = ((program[i] & 0x00FF) << 8) | ((program[i] & 0xFF00) >> 8);
        }
    }

    fwrite(program, sizeof(uint16_t), programlen, output);

    /* Release resources */
    free(program);

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

int is_basic_instruction(dcpu16opcode op) {
    return op != JSR;
}

uint16_t encode_opcode(dcpu16opcode op) {
    if (is_basic_instruction(op)) {
        switch (op) {
        case SET: return 0x1;
        case ADD: return 0x2;
        case SUB: return 0x3;
        case MUL: return 0x4;
        case DIV: return 0x5;
        case MOD: return 0x6;
        case SHL: return 0x7;
        case SHR: return 0x8;
        case AND: return 0x9;
        case BOR: return 0xA;
        case XOR: return 0xB;
        case IFE: return 0xC;
        case IFN: return 0xD;
        case IFG: return 0xE;
        case IFB: return 0xF;
        default: break;
        }
    } else {
        switch (op) {
        case JSR: return 0x0 | (0x1 << 4);
        default: break;
        }
    }

    return 0xFFFF;
}

uint16_t encode_value(dcpu16operand *op, uint16_t *w, int shift) {
    uint16_t bv; /* base value */

    switch (op->type) {
    case REGISTER:
        bv = op->value;
        break;

    case REGISTER_INDIRECT:
        /* Indirect register = registernum + 8 */
        bv = op->value + 0x8;
        break;

    case REGISTER_PLUS_LITERAL:
        /* Likewise, with + 16 */
        bv = op->value + 0x10;
        *w = op->next;
        break;

    case POP:
        bv = 0x18;
        break;

    case PEEK:
        bv = 0x19;
        break;

    case PUSH:
        bv = 0x1A;
        break;

    case SP:
        bv = 0x1B;
        break;

    case PC:
        bv = 0x1C;
        break;

    case O:
        bv = 0x1D;
        break;

    case LITERAL:
        /* If the literal value is smaller than 32 (0x20) we can encode it
         * in the same word */
        if (op->value < 0x20) {
            bv = 0x20 + op->value;
        } else {
            bv = 0x1f; /* 0x1f = literal is in the next word */
            *w = op->value;
        }

        break;

    case LITERAL_INDIRECT:
        /* Literal references are always in the next word, no matter how
         * small */
        bv = 0x1e;
        *w = op->value;
        break;

    default: break;
    }

    return bv << shift;
}

void encode(dcpu16opcode op, dcpu16operand *a, dcpu16operand *b,
               uint16_t *instr, uint16_t *wa, uint16_t *wb) {
    if (is_basic_instruction(op))
        *instr = encode_opcode(op)
               | encode_value(a, wa, 4)
               | encode_value(b, wb, 10);
    else
        *instr = encode_opcode(op)
               | encode_value(a, wa, 10);
}

int has_next(dcpu16operand *op) {
    switch (op->type) {
    case REGISTER_PLUS_LITERAL:
    case LITERAL_INDIRECT: return 1;
    case LITERAL: return (op->value > 0x1f);
    default: return 0;
    }
}

int get_instruction_length(dcpu16operand *a, dcpu16operand *b) {
    return has_next(a) + has_next(b);
}

uint16_t assemble(char **lines, uint16_t **program) {
    int i;
    uint16_t programlength = 0;
    uint16_t instr, worda, wordb;
    uint8_t instrlen = 0, words;

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


        /* get a and possibly b */
        if (is_basic_instruction(opcode)) {
            read_operand(&a, i+1, ",");
            parse_operand(a, &opa);

            read_operand(&b, i+1, "\n");
            parse_operand(b, &opb);

            printf("%s\t%s,\t%s\n", start, a, b);
        } else {
            read_operand(&a, i+1, "\n");
            parse_operand(a, &opb);

            printf("%s\t%s\n", start, a);
        }

        encode(opcode, &opa, &opb, &instr, &worda, &wordb);
        words = 1 + get_instruction_length(&opa, &opb);

        printf("Writing %d words\n", words);

        *program = realloc(*program, (instrlen + words) * 2);
        (*program)[instrlen++] = instr;

        if (has_next(&opa))
            (*program)[instrlen++] = worda;

        if (has_next(&opb))
            (*program)[instrlen++] = wordb;
        
    }

    return instrlen;
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
    error("Expected operand, got '%s'", *src);
    return 1;
}

uint16_t read_numeric(char **op) {
    unsigned int num;

    while (isspace(**op))
        (*op)++;

    if (!strncmp(*op, "0x", 2)) {
        /* Read hex digit */
        *op += 2; /* Skip "0x" */

        sscanf(*op, "%x", (unsigned int *)&num);
    } else if ((isdigit(**op)) && (**op != '0')) {
        /* Since normal numbers don't begin with a zero */
        sscanf(*op, "%d", (int *)&num);
    } else {
        error("Expected numeric, got '%s'", *op);
    }

    while (isdigit(**op) || (**op == 'x'))
        (*op)++;

    return num;
}

uint8_t parse_register(char **reg) {
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
    default:
        error("Expected 'A', 'B', 'C', 'X', 'Y', 'Z', 'I' or 'J', got '%c'",
               **reg);
    }

    /* Since this can't happen, just make the compiler shut up */
    return 0xFF;
}

void parse_operand(char *op, dcpu16operand *store) {
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
        uint16_t num = read_numeric(&op);
        store->type = LITERAL;
        store->value = num;
    } else if (*op == '[') {
        char *end = op + strlen(op) - 1;
        char *plus = NULL;
        /* Anything indirect */
        op++;

        /* Check for matching ']' */
        while (isspace(*end))
            --end;
        
        if (*end != ']')
            error("Unclosed '[', got '%c' instead", *end);
 
        *(++end) = '\0';

        while (isspace(*op))
            op++;

        /*
         * "[A + B]" is now "A + B]" with no trailing or leading whitespace
         */
        if ((plus = strstr(op, "+")) != NULL) {
            uint16_t num = 0;
            uint8_t reg  = 0;

            /* Move pointer behind the '+' */
            plus++;

            /* [Literal], [Register] */
            if (isalpha(*op)) {
                /* [Register + Literal] */
                reg = parse_register(&op);
                num = read_numeric(&plus);
            } else if (isdigit(*op)) {
                /* [Literal + Register] */
                reg = parse_register(&plus);
                num = read_numeric(&op);
            } else {
                error("Expected register or literal, got '%s'", op);
            }

            while (isspace(*op))
                op++;
            while (isspace(*plus))
                plus++;

            if (*op != '+')
                error("Expected '+', got '%c'", *op);

            if (*plus != ']')
                error("Expected ']', got '%c'", *plus);

            op = plus;

            store->type = REGISTER_PLUS_LITERAL;
            store->value = reg;
            store->next = num;

            printf("Numeric = %04X\n", num);
        } else {
            /* [Literal], [Register] */
            if (isalpha(*op)) {
                /* [Register] */
                store->type = REGISTER_INDIRECT;
                store->value = parse_register(&op);
            } else if (isdigit(*op)) {
                /* [Literal] */
                store->type = LITERAL_INDIRECT;
                store->value = read_numeric(&op);
            } else {
                error("Expected register or literal, got '%c'", op);
            }

            while (isspace(*op))
                op++;

            if (*op != ']')
                error("Expected ']', got '%c'", *op);
        }

        while (isspace(*op))
            op++;

        if (strlen(op) != 1)
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

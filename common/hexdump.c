#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>

#include "hexdump.h"


endianness_t get_endianness() {
    union {
        uint16_t i;
        char b[2];
    } bo = { 0xBEEF };

    return bo.b[0] == 1;
}

uint16_t swaps(uint16_t s) {
    return ((s & 0xFF00) >> 8) | ((s & 0x00FF) << 8);
}

int write_hexdump(FILE *f, endianness_t dstend, uint16_t *mem, size_t msize) {
    unsigned int i;
    uint16_t last[8] = {0};
    int skipping = 0;

    for (i = 0; i < msize;) {
        unsigned int j = i, g = 0, r = 1;

        /* Check if this row of words is the same as the last row */
        for (g = 0; g < 8; ++g) {
            r = r && (last[g] == mem[i + g]);
        }

        /* If yes, [continue] skip[ping] next line[s]*/
        if (r) {
            if (!skipping) {
                fprintf(f, "*\n");
                skipping = 1;
            }

            /* Make sure the last line is always displayed */
            if (skipping && ((msize - i) <= 8)) {
                skipping = 0;
            } else {
                i += 8;
                continue;
            }
        }

        skipping = 0;
        fprintf(f, "%04X: ", i);

        for (i = i; i < j+8; ++i) {
            if (i >= msize)
                break;

            /*
             * No need to check for system endianness, because
             * printf write the word out the same regardless of
             * endianness, in big endian. Thus to get little endian
             * we _always_ have to switch it around.
             */
            if (dstend == LITTLEENDIAN)
                fprintf(f, "%04X ", swaps(mem[i]));
            else
                fprintf(f, "%04X ", mem[i]);

            last[i % 8] = mem[i];
        }

        fprintf(f, "\n");
    }

    return 0;
}

int read_hexdump(FILE *f, endianness_t srcend, uint16_t *mem, size_t memsize) {
    char buffer[512] = {0};
    char *p = buffer;

    int continue_last = 0;
    uint16_t last[8] = {0};
    int lastoff = -8;

#define SKIP(p) while (isspace(*p)) p++;
#define SKIPX(p) while (isxdigit(*p)) p++;

    while (!feof(f)) {
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), f);
        p = buffer;

        if (*p == '*') {
            /*
             * Continuation from last line
             */
            continue_last = 1;
        } else if (isxdigit(*p)) {
            /*
             * OOOO: NNNN [NNNN...]{0 .. 7}
             */
            int offset;
            sscanf(buffer, "%x", &offset);

            if (!continue_last && ((offset - lastoff) != 8)) {
                return -1;
            }

            if (continue_last) {
                int i = 0;
                int off = lastoff + 8;
                continue_last = 0;
                
                while ((off+i) < offset) {
                    mem[off + i] = last[i % 8];
                    ++i;
                }
            }

            lastoff = offset;

            SKIPX(p);
            SKIP(p);
            if (*p++ == ':') {
                unsigned int i;

                for (i = 0; i < 8; ++i) {
                    SKIP(p);
                    if (isxdigit(*p)) {
                        unsigned int n;

                        if ((offset + i) > memsize) {
                            return -1;
                        }

                        sscanf(p, "%04X", &n);
                        if (n > 0xFFFF) {
                            exit(1);
                        }

                        if (srcend != get_endianness())
                            n = swaps(n);

                        last[i] = n;
                        mem[offset + i] = n;

                        SKIPX(p);
                    } else {
                        break;
                    }
                }
            } else {
                return -1;
            }
        }
    }
    
    return 0;
}

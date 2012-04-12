#ifndef HEXDUMP_H
#define HEXDUMP_H

typedef enum {
    BIGENDIAN,
    LITTLEENDIAN
} endianness_t;

int write_hexdump(FILE *f, endianness_t, uint16_t*, size_t);
int read_hexdump(FILE *f, endianness_t, uint16_t*, size_t);

#endif

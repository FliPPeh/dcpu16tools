#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

extern int curline;
extern char *cur_line, *cur_pos, *srcfile;

void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *tmp = malloc(strlen(fmt) + strlen("Error: ") + 1);
    strcpy(tmp, "Error: ");
    strcat(tmp, fmt);

    logmsg(tmp, args);
    free(tmp);

    va_end(args);

    exit(1);
}

void warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *tmp = malloc(strlen(fmt) + strlen("Warning: ") + 1);
    strcpy(tmp, "Warning: ");
    strcat(tmp, fmt);

    logmsg(tmp, args);
    free(tmp);

    va_end(args);
}

void logmsg(const char *fmt, va_list args) {
    char errmsg[512] = {0};

    vsnprintf(errmsg, sizeof(errmsg) - 1, fmt, args);

    if (curline > 0)
        fprintf(stderr, "%s:%d:%ld: %s\n", 
                srcfile, curline, cur_pos - cur_line, errmsg);
    else
        fprintf(stderr, "%s: %s\n", srcfile, errmsg);

    return;
}

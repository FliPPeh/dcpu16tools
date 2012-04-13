#ifndef UTIL_H
#define UTIL_H

#include <stdarg.h>

void logmsg(const char*, va_list);
void error(const char*, ...);
void warning(const char*, ...);

#endif

#ifndef LABEL_H
#define LABEL_H

#include "types.h"
#include "../common/linked_list.h"

dcpu16label *getnewlabel(list*, const char*);
dcpu16label *getlabel(list*, const char*);

#endif

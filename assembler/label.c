#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "../common/linked_list.h"

dcpu16label *getlabel(list *labels, const char *label) {
    list_node *node = NULL;

    for (node = list_get_root(labels); node != NULL; node = node->next) {
        dcpu16label *ptr = node->data;

        if (!(strcmp(ptr->label, label)))
            return ptr;
    }
    
    return NULL;
}

dcpu16label *getnewlabel(list *labels, const char *label) {
    dcpu16label *ptr = getlabel(labels, label);
    
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


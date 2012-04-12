/*
 * Copyright (c) 2012
 *
 * This file is part of dcpu16tools
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef LIBLL_H
#define LIBLL_H

/*
 * To have the library accessible from C++, too 
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct linked_list linked_list;
typedef struct list_node list_node;

typedef linked_list list;

struct linked_list {
    list_node *root;
    
    size_t length;
};

struct list_node {
    void *data;

    list_node *next;
};

linked_list *list_create();
list_node *list_get_root(linked_list*);
list_node *list_push_front(linked_list*, void*);
list_node *list_push_back(linked_list*, void*);
list_node *list_insert_before(linked_list*, list_node*, void *);
list_node *list_insert_after(linked_list*, list_node*, void *);
void list_dispose(linked_list**, void (*)(void*));


/*
 * End "extern" declaration for C++
 */
#ifdef __cplusplus
}
#endif

#endif /* ifndef LIBLL_H */


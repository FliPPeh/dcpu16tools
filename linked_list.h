/******************************************************************************
 *
 *  Linked List library (libll.h)
 *  ---
 *  Library interface
 *
 *  Created: 05.01.2011 14:54:16
 *  Author:  Lukas Niederbremer
 *
 ******************************************************************************/
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

struct linked_list
{
    list_node *root;
    
    size_t length;
};

struct list_node
{
    void *data;

    list_node *next;
};

linked_list *list_create();
list_node *list_get_root(linked_list *);
list_node *list_push_front(linked_list *, void *);
list_node *list_push_back(linked_list *, void *);
list_node *list_insert_before(linked_list *, list_node *, void *);
list_node *list_insert_after(linked_list *, list_node *, void *);
void list_dispose(linked_list **, void (*)(void *));


/*
 * End "extern" declaration for C++
 */
#ifdef __cplusplus
}
#endif

#endif /* ifndef LIBLL_H */


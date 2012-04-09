/******************************************************************************
 *
 *  Simple linked list implementation (linked_list.c)
 *  ---
 *  Construct, free and maintain a singly linked list
 *
 *  Created: 06.01.2011 20:38:07
 *  Author:  Lukas Niederbremer
 *
 ******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "linked_list.h"


linked_list *list_create()
{
    linked_list *list = malloc(sizeof(linked_list));
    if (list)
    {
        list->root = NULL;
        list->length = 0;
    }

    return list;
}

list_node *list_get_root(linked_list *list)
{
    if (list)
        return list->root;
    else
        return NULL;
}

list_node *list_push_front(linked_list *list, void *data)
{
    list_node *tmp = NULL;

    if (list == NULL)
        return NULL;

    tmp = malloc(sizeof(list_node));
    if (tmp)
    {
        list_node *root = list_get_root(list);

        memset(tmp, 0, sizeof(*tmp));
        tmp->data = data;

        if (root)
        {
            list->root = tmp;
            tmp->next = root;
        }
        else
        {
            list->root = tmp;
        }
    }

    return tmp;
}

list_node *list_insert_before(linked_list *list, list_node *pos, void *data)
{
    list_node *tmp = NULL;
    list_node *pre = NULL;

    if ((list == NULL) || (pos == NULL))
        return NULL;

    if ((list_get_root(list) == NULL) || (pos == list_get_root(list)))
        return list_push_front(list, data);

    pre = list_get_root(list);
    while (pre->next != pos)
    {
        pre = pre->next;

        if (!pre)
            return NULL;
    }

    tmp = malloc(sizeof(list_node));
    if (tmp)
    {
        memset(tmp, 0, sizeof(*tmp));

        tmp->data = data;
        tmp->next = pos;
        pre->next = tmp;
    }

    return tmp;
}

list_node *list_insert_after(linked_list *list, list_node *pos, void *data)
{
    if ((list == NULL) || (pos == NULL))
        return NULL;

    if (pos->next == NULL)
        return list_push_back(list, data);
    else
        return list_insert_before(list, pos->next, data);
}

list_node *list_push_back(linked_list *list, void *data)
{
    list_node *tmp = NULL;

    if (list == NULL)
        return NULL;

    tmp = malloc(sizeof(list_node));
    if (tmp)
    {
        list_node *root = list_get_root(list);

        memset(tmp, 0, sizeof(*tmp));
        tmp->data = data;

        if (root)
        {
            list_node *current = root;

            while (current->next != NULL)
                current = current->next;

            current->next = tmp;
        }
        else
        {
            list->root = tmp;
        }
    }

    return tmp;
}

void list_dispose(linked_list **list, void (*free_content)(void *))
{
    list_node *current = NULL;

    if ((list == NULL) || (*list == NULL))
        return;

    current = (*list)->root;

    while (current != NULL)
    {
        list_node *next = current->next;

        if (free_content != NULL)
            free_content(current->data);

        free(current);
        current = next;
    }

    free(*list);
    *list = NULL;

    return;
}

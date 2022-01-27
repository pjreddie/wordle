#ifndef LIST_H
#define LIST_H

typedef struct node{
    void *val;
    struct node *next;
    struct node *prev;
} node;

typedef struct list{
    size_t size;
    node *front;
    node *back;
} list;

list *make_list();

list *list_push(list *, void *);
void *list_pop(list *l);

void **list_to_array(list *l);

void free_list(list *l);
void free_list_contents(list *l);

#endif

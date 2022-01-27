#ifndef MAP_H
#define MAP_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "list.h"
#include "vector.h"


typedef struct kvp{
    vector key;
    void *val;
} kvp;

typedef struct map{
    size_t size;
    size_t load;
    list **data;
} map;

map *make_map();
void *set_map(map *d, const vector key, void *val);
void *get_map(map *d, const vector key, void *def);
void *del_map(map *d, const vector key); 
void free_map(map *d);
void print_map(map *d);
void free_map(map *d);

#endif

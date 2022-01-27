#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "vector.h"

void error(const char *s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    va_end(args);
    exit(1);
}

vector make_vector(size_t capacity)
{
    if (!capacity) capacity = 16;
    vector v = {0};
    v.data = calloc(1, sizeof(void **));
    v.data[0] = calloc(capacity, sizeof (void *));
    v.capacity = calloc(1, sizeof(size_t));
    *v.capacity = capacity;
    v.size = calloc(1, sizeof(size_t));
    return v;
}

vector copy_vector(vector v)
{
    vector c = make_vector(*v.size);
    *c.size = *v.size;
    int i;
    for(i = 0; i < *v.size; ++i){
        c.data[0][i] = v.data[0][i];
    }
    return c;
}

void free_vector(vector v)
{
    free(v.data[0]);
    free(v.data);
    free(v.capacity);
    free(v.size);
}

void *get_vector(const vector v, const size_t i)
{
    if ( i < 0 || *v.size <= i ) {
        error("Out of bounds vector get: %d, size %d", i, *v.size);
    }
    return v.data[0][i];
}

void set_vector(vector v, const size_t i, void *p)
{
    if ( i < 0 || *v.size <= i ) {
        error("Out of bounds vector get: %d, size %d", i, *v.size);
    }

    v.data[0][i] = p;
}

void append_vector(vector v,  void *p)
{
    if ( *v.size == *v.capacity ){
        *v.capacity *= 2;
        v.data[0] = realloc(v.data[0], *v.capacity * sizeof(void *));
    }
    v.data[0][*v.size] = p;
    ++*v.size;
}


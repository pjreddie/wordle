#ifndef VECTOR_H
#define VECTOR_H

#include "stddef.h"

typedef struct {
    void **data;
    size_t size;
    size_t capacity;
} vector;


vector *make_vector(size_t capacity);
vector *copy_vector(const vector *v);
vector *concat_vectors(vector *a, vector *b);
void free_vector(vector *v);
void *get_vector(const vector *v, const size_t i);
void set_vector(vector *v, const size_t i, void * p);
void append_vector(vector *v, void *p);

#endif

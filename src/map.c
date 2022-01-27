#include "map.h"


//djb2 hash function
//taken from http://www.cse.yorku.ca/~oz/hash.html
size_t hash_vector(const vector v)
{
    size_t hash = 5381;
    size_t c;

    int i = 0;
    for(i = 0; i < *v.size; ++i){
        c = (size_t) v.data[0][i];
        hash = ((hash << 5) + hash) ^ c; /* hash * 33 ^ c */
    }

    return hash;
}

inline int compare_vector(const vector v1, const vector v2)
{
    if(*v1.size != *v2.size) return 0;
    int i;
    for(i = 0; i < *v1.size; ++i){
        if (v1.data[0][i] != v2.data[0][i]) return 0;
    }
    return 1;
}

map *make_map()
{
    map *d = calloc(1, sizeof(map));
    d->size = 63;
    d->load = 0;
    d->data = calloc(d->size, sizeof(list*));
    return d;
}

kvp *kvp_list_find(list *l, const vector key)
{
    if(!l) return 0;
    node *n = l->front;
    while(n){
        kvp *pair = (kvp*) n->val;
        if(compare_vector(key, pair->key)){
            return pair;
        }
        n = n->next;
    }
    return 0;
}

void expand_map(map *d)
{
    size_t i;
    size_t old_size = d->size;
    list **old_data = d->data;
    d->size = (d->size+1)*2 - 1;
    d->data = calloc(d->size, sizeof(list*));
    for(i = 0; i < old_size; ++i){
        list *l = old_data[i];
        if(l){
            node *n = l->front;
            while(n){
                kvp *pair = (kvp *) n->val;
                size_t h = hash_vector(pair->key)%d->size;
                d->data[h] = list_push(d->data[h], pair);
                n = n->next;
            }
        }
        free_list(old_data[i]);
    }
    free(old_data);
}

void *set_map(map *d, const vector key, void *val)
{
    void *old = 0;
    if((double)d->load / d->size > .7) expand_map(d);

    size_t h = hash_vector(key) % d->size;
    list *l = d->data[h];
    kvp *pair = kvp_list_find(l, key);
    if(pair){
        old = pair->val;
        pair->val = val;
    }else{
        pair = calloc(1, sizeof(kvp));
        pair->key = copy_vector(key);
        pair->val = val;
        d->data[h] = list_push(d->data[h], pair);
        ++d->load;
    }
    return old;
}

void *get_map(map *d, const vector key, void *def)
{
    size_t h = hash_vector(key) % d->size;
    list *l = d->data[h];
    kvp *pair = kvp_list_find(l, key);
    if(pair){
        return pair->val;
    }else{
        return def;
    }
    return def;
}

void free_map(map *d)
{
    size_t i;
    for(i = 0; i < d->size; ++i){
        list *l = d->data[i];
        if(l){
            node *n = l->front;
            while(n){
                kvp *pair = (kvp *) n->val;
                free_vector(pair->key);
                free(pair);
                n = n->next;
            }
            free_list(l);
        }
    }
    free(d->data);
    free(d);
}

void print_map(map *d)
{
    size_t i;
    for(i = 0; i < d->size; ++i){
        list *l = d->data[i];
        if(l){
            node *n = l->front;
            while(n){
                kvp *pair = (kvp *) n->val;
                printf("%s: %p\n", pair->key, pair->val);
                n = n->next;
            }
        }
    }
}


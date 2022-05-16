#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "jcr.h"

#define WSIZE 5
#define NWORDS 2315
#define NALL 12972
#define STOPEARLY 1

vector *TRUTHS = 0;
vector *GUESSES = 0;
uint8_t *SCORES = 0;

vector *read_word_file(char *f)
{
    FILE *fp = fopen(f, "r");
    char *word = 0;
    vector *v = make_vector(0);
    while((word = fgetl(fp))){
        append_vector(v, word);
    }
    fclose(fp);
    return v;
}

static inline int ipow(double a, double b)
{
    return ((int) (pow(a, b) + 1e-8));
}

int score_guess(char *guess_, char *truth_)
{
    char truth[WSIZE];
    char guess[WSIZE];
    memcpy(truth, truth_, WSIZE);
    memcpy(guess, guess_, WSIZE);
    int i, j;
    int score = 0;
    for (i = 0; i < WSIZE; ++i) {
        char c = guess[i];
        if (truth[i] == c) {
            truth[i] = 0;
            guess[i] = 0;
            score += ipow(3, i)*2;
        }
    }
    for (i = 0; i < WSIZE; ++i) {
        char c = guess[i];
        if (!c) continue;
        for (j = 0; j < WSIZE; ++j) {
            if (c == truth[j]) {
                truth[j] = 0;
                score += ipow(3, i);
                break;
            }
        }
    }

    return score;
}

void fill_scores(vector *guesses, vector *truths)
{
    int i, j;
    int n = guesses->size;
    SCORES = calloc(n*n, sizeof(uint8_t));
    for(i = 0; i < guesses->size; ++i){
        for(j = 0; j < truths->size; ++j){
            size_t gi = (size_t) guesses->data[i];
            size_t tj = (size_t) truths->data[j];
            SCORES[gi*n + tj] =  score_guess(GUESSES->data[gi], GUESSES->data[tj]);
        }
    }
}

static inline uint8_t get_score(size_t guess, size_t truth)
{
    return SCORES[guess*GUESSES->size + truth];
}

void free_splits(vector **splits)
{
    if(!splits) return;
    size_t i;
    for(i = 0; i < ipow(3, WSIZE); ++i){
        if(splits[i]) free_vector(splits[i]);
    }
    free(splits);
}

vector **split(vector *truths, size_t guess)
{
    vector **splits = calloc(ipow(3, WSIZE), sizeof(vector *));

    size_t i;
    for(i = 0; i < truths->size; ++i){
        size_t truth = (size_t) get_vector(truths, i);
        uint8_t score = get_score(guess,truth);
        if (!splits[score]) splits[score] = make_vector(0);
        append_vector(splits[score], (void*) truth);
    }
    return splits;
}

char *decode(size_t i)
{
    return GUESSES->data[i];
}

float avg_split_size(vector *truths, size_t guess)
{
    int *counts = calloc(ipow(3, WSIZE), sizeof(int));
    int i;
    int sum = 0;
    int total = 0;
    for(i = 0; i < truths->size; ++i){
        size_t truth = (size_t) truths->data[i];
        ++counts[get_score(guess, truth)];
    }
    for(i = 0; i < ipow(3, WSIZE); ++i){
        if(counts[i]){
            sum += counts[i];
            total += 1;
        }
    }
    if(counts[ipow(3, WSIZE)-1]) sum -= 1;
    free(counts);
    return 1.0 * sum / total;
}


typedef struct{
    float score;
    size_t index;
} score_pair;

int score_pair_cmp(const void *a, const void *b)
{
    float fa = ((score_pair *)a)->score;
    float fb = ((score_pair *)b)->score;
    return (fa > fb) - (fa < fb);
}

score_pair *best_splits(vector *truths, vector *guesses)
{
    score_pair *scores = calloc(guesses->size, sizeof(score_pair));
    int i = 0;
    int cutoff = guesses->size;
    if(STOPEARLY && truths->size < 13){
        for(i = 0; i < truths->size; ++i){
            size_t guess = (size_t) truths->data[i];
            float avg = avg_split_size(truths, guess);
            if (avg < 1) {
                scores[0].score = avg;
                scores[0].index = guess;
                return scores;
            }
        }
    }
    for(i = 0; i < guesses->size; ++i){
        size_t guess = (size_t) guesses->data[i];
        float avg = avg_split_size(truths, guess);

        scores[i].score = avg;
        scores[i].index = guess;
        if (STOPEARLY && avg < 1){
            cutoff = i + 1;
            break;
        }
    }
    qsort(scores, cutoff, sizeof(score_pair), score_pair_cmp);
    return scores;
}

typedef struct{
    int leaf;
    size_t index;
    vector *children;
    vector *leaves;
    int depth_sum;
    int count_sum;
    int max_depth;
    int optimal;
} tree;

void free_tree(tree *t)
{
    if (t->children){
        int i;
        for(i = 0; i < t->children->size; ++i){
            free_tree((tree *)t->children->data[i]);
        }
        free_vector(t->children);
    }
    if (t->leaves){
        free_vector(t->leaves);
    }
    free(t);
}

size_t index_of(char *s)
{
    size_t i;
    for(i = 0; i < NALL; ++i){
        if(0 == strcmp(s, GUESSES->data[i])) return i;
    }
    return 0;
}

void free_tree_shallow(tree *t)
{
    if (t->children){
        free_vector(t->children);
    }
    if(t->leaves){
        free_vector(t->leaves);
    }
    free(t);
}

typedef struct {
    int n;
    int depths;
} depth_counts;

tree *make_leaves(vector *truths, size_t guess)
{
    tree *t = calloc(1, sizeof(tree));
    t->leaves = make_vector(truths->size);
    t->optimal = 1;
    t->index = guess;
    t->depth_sum = 2*truths->size;
    t->count_sum = 1*truths->size;
    t->max_depth = 2;
    int i;
    for(i = 0; i < truths->size; ++i){
        if((size_t) truths->data[i] == guess){
            t->leaf = 1;
            t->depth_sum -= 1;
        } else {
            append_vector(t->leaves, truths->data[i]);
        }
    }
    return t;
}

tree *optimal_tree(vector *truths, vector *guesses, int depth, int n, int hard, map *memo, int depth_cutoff, char *start)
{
    int i = 0;
    for(i = 0; i < truths->size; ++i){
        size_t guess = (size_t) truths->data[i];
        float avg = avg_split_size(truths, guess);
        if (avg < 1) {
            return make_leaves(truths, guess);
        }
    }
    for(i = 0; i < guesses->size; ++i){
        size_t guess = (size_t) guesses->data[i];
        float avg = avg_split_size(truths, guess);
        if (avg == 1) {
            return make_leaves(truths, guess);
        }
    }
    return 0;
}

tree *make_tree(vector *truths, vector *guesses, int depth, int n, int hard, map *memo, int depth_cutoff, char *start)
{
    int n_orig = n;
    vector *key = 0;
    if (hard){
        key = concat_vectors(truths, guesses);
    } else {
        key = copy_vector(truths);
    }
    tree *cache = get_map(memo, key, 0);
    if (cache != 0){
        if (depth_cutoff && depth + cache->max_depth > depth_cutoff) {
            // cache miss
            int needed = depth;
            append_vector(key, (void *) (size_t) -needed);
            tree *cache2 = get_map(memo, key, 0);
            if (cache2){
                free_vector(key);
                return cache2;
            } else {
                --key->size;
            }
        }else{
            free_vector(key);
            return cache;
        }
    }

    if (guesses->size < n) n = guesses->size;
    score_pair *scores = best_splits(truths, guesses);
    if (STOPEARLY && scores[0].score < 1) n = 1;

    tree **trees = calloc(n, sizeof(tree *));
    int i, j;
    for(i = 0; i < n; ++i){
        size_t guess = scores[i].index;
        if(depth == 0 && start){
            guess = index_of(start);
            n = 1;
        }
        vector **splits = split(truths, guess);
        vector **next_guesses = 0;
        if (hard){
            next_guesses = split(guesses, guess);
        }

        trees[i] = calloc(1, sizeof(tree));
        trees[i]->children = make_vector(0);
        trees[i]->leaves = make_vector(0);
        trees[i]->index = guess;

        for (j = 0; j < ipow(3, WSIZE); ++j){
            if(splits[j]){
                if (splits[j]->size == 1){
                    size_t index = (size_t) splits[j]->data[0];
                    if(index == guess){
                        trees[i]->leaf = 1;
                        trees[i]->count_sum += 1;
                        trees[i]->depth_sum += 1;
                        if (1 > trees[i]->max_depth) trees[i]->max_depth = 1;
                    } else {
                        append_vector(trees[i]->leaves, (void *)index);
                        trees[i]->count_sum += 1;
                        trees[i]->depth_sum += 2;
                        if (2 > trees[i]->max_depth) trees[i]->max_depth = 2;
                    }
                } else {
                    tree *child = make_tree(splits[j], hard ? next_guesses[j] : guesses, depth+1, n_orig, hard, memo, depth_cutoff, start);
                    append_vector(trees[i]->children, child);
                    trees[i]->count_sum += child->count_sum;
                    trees[i]->depth_sum += child->count_sum + child->depth_sum;
                    if((child->max_depth + 1) > trees[i]->max_depth) {
                        trees[i]->max_depth = child->max_depth + 1;
                        if (depth == 0) {
                            //printf("hey %s %d %d\n", decode(guess), child->max_depth, trees[i]->max_depth);
                        }
                        if(depth_cutoff && trees[i]->max_depth + depth > depth_cutoff) break;
                    }
                }
            }
        }
        free_splits(splits);
        free_splits(next_guesses);
    }

    float best = 1.0 * trees[0]->depth_sum / trees[0]->count_sum;
    if (depth_cutoff && trees[0]->max_depth + depth > depth_cutoff) best += 1000;
    for(i = 1; i < n; ++i){
        float avg = 1.0 * trees[i]->depth_sum / trees[i]->count_sum;
        if (depth_cutoff && trees[i]->max_depth + depth > depth_cutoff) avg += 1000;
        if(depth == 0) printf("%s %f %d\n", decode(trees[i]->index), avg, trees[i]->max_depth);
        if(avg < best){
            best = avg;
            tree *swap = trees[0];
            trees[0] = trees[i];
            trees[i] = swap;
        }
    }
    tree *t = trees[0];
    for(i = 1; i < n; ++i){
        free_tree_shallow(trees[i]);
    }
    if(cache && t->max_depth < cache->max_depth){
        //printf("curr: %d, cache: %d, found: %d\n", depth, cache->max_depth, t->max_depth);
        append_vector(key, (void *) (size_t) -depth);
        set_map(memo, key, t);
    }
    if(!cache) {
        if(!depth_cutoff || t->max_depth + depth <= depth_cutoff){
            set_map(memo, key, t);
        }
    }
    free(trees);
    free(scores);
    free_vector(key);
    return t;
}

void print_split(vector **splits)
{
    size_t i, j;
    for(i = 0; i < ipow(3, WSIZE); ++i){
        if (!splits[i]) continue;
        for(j = 0; j < splits[i]->size; ++j){
            printf("%s,", decode((size_t) splits[i]->data[j]));
        }
        printf("\n");
    }
}


void print_tree(tree *t, char *buff)
{
    char *w = decode(t->index);   
    int i;
    char b2[1024] = {0};
    sprintf(b2, "%s%s%s", buff, strlen(buff)?",":"", w);

    if (t->children){
        for(i = 0; i < t->children->size; ++i){
            print_tree((tree *)t->children->data[i], b2);
        }
    }

    if (t->leaves){
        for(i = 0; i < t->leaves->size; ++i){
            char *l = decode((size_t) t->leaves->data[i]);
            printf("%s,%s\n", b2, l);
        }
    }

    if(t->leaf){
        printf("%s\n", b2);
    }
}

void free_memo(map *m)
{
    int i;
    for(i = 0; i < m->capacity; ++i){
        list *l = m->data[i];
        if(l){
            node *n = l->front;
            while(n){
                kvp *pair = (kvp *) n->val;
                free_tree_shallow((tree *) pair->val);
                n = n->next;
            }
        }
    }
}

int main(int argc, char **argv)
{
    char *guess_f = find_char_arg(argc, argv, "-g", "all.txt");
    char *truth_f = find_char_arg(argc, argv, "-t", "words.txt");
    int hard = find_arg(argc, argv, "-h");
    int width = find_int_arg(argc, argv, "-w", 8);
    int depth = find_int_arg(argc, argv, "-d", 6);
    char *start = find_char_arg(argc, argv, "-s", 0);

    GUESSES = read_word_file(guess_f);
    TRUTHS  = read_word_file(truth_f);

    double time = now();
    vector *truth_indexes = make_vector(TRUTHS->size);
    size_t i, j;
    j = 0;
    for(i = 0; i < TRUTHS->size; ++i){
        for(; j < GUESSES->size; ++j){
            if (0 == strncmp(TRUTHS->data[i], GUESSES->data[j], WSIZE)){
                append_vector(truth_indexes, (void*) j);
                break;
            }
        }
    }
    printf("truth_indexes filled %f sec\n", now() - time);

    vector *guess_indexes = make_vector(GUESSES->size);
    for(j = 0; j < GUESSES->size; ++j){
        append_vector(guess_indexes, (void*) j);
    }

    fill_scores(guess_indexes, hard ? guess_indexes : truth_indexes);


    printf("%ld %ld\n", truth_indexes->size, truth_indexes->capacity);

    if(0){
        printf("%d\n", score_guess("words", "words"));
        return 0;
    }
    //vector *splits = split(word_indexes, 6180);
    //print_split(splits);

    if (0){
        score_pair *scores = best_splits(truth_indexes, guess_indexes);
        for(i = 0; i < 40; ++i){
            printf("%f %s\n", scores[i].score, decode(scores[i].index));
        }
        return 0;
    }


    map *memo = make_map();
    tree *t =  make_tree(truth_indexes, guess_indexes, 0, width, hard, memo, depth, start);
    print_tree(t, "");
    printf("%d %d %d %f\n", t->depth_sum, t->count_sum, t->max_depth, 1.0*t->depth_sum / t->count_sum);
    free_memo(memo);
    free_vector(truth_indexes);
    free_vector(guess_indexes);
    free_map(memo);
}

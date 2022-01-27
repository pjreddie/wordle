#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "vector.h"
#include "map.h"

#define WSIZE 5
#define NWORDS 2315
#define NALL 12972
#define STOPEARLY 1

char WORDS[NWORDS][WSIZE + 1];
char ALL[NALL][WSIZE + 1];
uint8_t SCORES[NALL][NALL];

void read_data()
{
    FILE *fp = fopen("words.txt", "r");
    int i;
    for(i = 0; i < NWORDS; ++i){
        fgets(WORDS[i], 7, fp);
        WORDS[i][WSIZE] = 0;
    }
    fclose(fp);

    fp = fopen("all.txt", "r");
    for(i = 0; i < NALL; ++i){
        fgets(ALL[i], 7, fp);
        ALL[i][5] = 0;
    }
    fclose(fp);
}

static inline int ipow(double a, double b)
{
    return ((int) (pow(a, b) + 1e-8));
}

uint8_t score_guess(char *guess_, char *truth_)
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

    return (uint8_t) score;
}

void fill_scores()
{
    int i, j;
    for(i = 0; i < NALL; ++i){
        for(j = 0; j < NALL; ++j){
            SCORES[i][j] = score_guess(ALL[i], ALL[j]);
        }
    }
}

void fill_scores_partial(vector v)
{
    int i, j;
    for(i = 0; i < *v.size; ++i){
        for(j = 0; j < *v.size; ++j){
            int i1, j1;
            i1 = v.data[0][i];
            j1 = v.data[0][j];
            SCORES[i1][j1] = score_guess(ALL[i1], ALL[j1]);
        }
    }
}

void free_splits(vector *splits)
{
    size_t i;
    for(i = 0; i < ipow(3, WSIZE); ++i){
        if(splits[i].data) free_vector(splits[i]);
    }
    free(splits);
}

vector *split(vector truths, size_t guess)
{
    vector *splits = calloc(ipow(3, WSIZE), sizeof(vector));

    size_t i;
    for(i = 0; i < *truths.size; ++i){
        size_t truth = (size_t) get_vector(truths, i);
        uint8_t score = SCORES[guess][truth];
        //uint8_t score = score_guess(WORDS[guess], WORDS[truth]);
        if (!splits[score].data) splits[score] = make_vector(0);
        append_vector(splits[score], (void*) truth);
    }
    return splits;
}

char *decode(size_t i)
{
    return ALL[i];
}

float avg_split_size(vector *splits)
{
    double sum = 0;
    size_t n = 0;
    size_t i;
    size_t lim = ipow(3, WSIZE);
    for(i = 0; i < lim; ++i){
        if (!splits[i].data) continue;
        size_t ssize = *splits[i].size;
        if (0){
            if (i == lim-1){
                n += ssize;
            } else {
                n += ssize;
                sum += ssize*ssize;
            }
        } else {
            if (i == lim-1){
                //printf("hey\n");
                n += 1;
            } else {
                n += 1;
                sum += ssize;
            }
        }
    }
    return sum / n;
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

score_pair *best_splits(vector truths, vector guesses)
{
    score_pair *scores = calloc(*guesses.size, sizeof(score_pair));
    int i = 0;
    int cutoff = *guesses.size;
    if(STOPEARLY && *truths.size < 13){
        for(i = 0; i < *truths.size; ++i){
            size_t guess = (size_t) truths.data[0][i];
            vector *splits = split(truths, guess);
            float avg = avg_split_size(splits);
            free_splits(splits);
            if (avg < 1) {
                scores[0].score = avg;
                scores[0].index = guess;
                return scores;
            }
        }
    }
    for(i = 0; i < *guesses.size; ++i){
        size_t guess = (size_t) guesses.data[0][i];
        vector *splits = split(truths, guess);
        float avg = avg_split_size(splits);
        free_splits(splits);

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
    vector children;
    vector leaves;
    int depth_sum;
    int count_sum;
} tree;

void free_tree(tree *t)
{
    if (t->children.data){
        int i;
        for(i = 0; i < t->children.size[0]; ++i){
            free_tree((tree *)t->children.data[0][i]);
        }
        free_vector(t->children);
    }
    if (t->leaves.data){
        free_vector(t->leaves);
    }
    free(t);
}

void free_tree_shallow(tree *t)
{
    if (t->children.data){
        free_vector(t->children);
    }
    if(t->leaves.data){
        free_vector(t->leaves);
    }
    free(t);
}

typedef struct {
    int n;
    int depths;
} depth_counts;

/*
depth_counts depth_counts_tree(tree *t, int depth, size_t prev)
{
    depth_counts c = {0};
    if(!t->children.data){
        c.n = 1;
        c.depths = depth+1;
        if(prev == t->index) c.depths = depth;
        //if(c.depths > 6) c.depths = 10000;
    } else {
        int i;
        for(i = 0; i < t->children.size[0]; ++i){
            depth_counts sub = depth_counts_tree((tree *) t->children.data[0][i], depth + 1, t->index);
            c.n += sub.n;
            c.depths += sub.depths;
        }
    }
    return c;
}
float avg_depth_tree(tree *t)
{
    depth_counts c = depth_counts_tree(t, 0, -1);
    return 1.0 * c.depths / c.n;
}
*/

tree *make_tree(vector truths, vector guesses, int depth, int n, int hard, map *memo)
{
    tree *cache = get_map(memo, truths, 0);
    if (cache != 0){
        return cache;
    }
    if (*guesses.size < n) n = *guesses.size;
    score_pair *scores = best_splits(truths, guesses);
    if (STOPEARLY && scores[0].score < 1) n = 1;

    tree **trees = calloc(n, sizeof(tree *));
    int i, j;
    for(i = 0; i < n; ++i){
        size_t guess = scores[i].index;
        vector *splits = split(truths, guess);

        trees[i] = calloc(1, sizeof(tree));
        trees[i]->children = make_vector(0);
        trees[i]->leaves = make_vector(0);
        trees[i]->index = guess;

        for (j = 0; j < ipow(3, WSIZE); ++j){
            if(splits[j].data){
                if (*splits[j].size == 1){
                    size_t index = (size_t) splits[j].data[0][0];
                    if(index == guess){
                        trees[i]->leaf = 1;
                        trees[i]->count_sum += 1;
                        trees[i]->depth_sum += 1;
                    } else {
                        append_vector(trees[i]->leaves, (void *)index);
                        trees[i]->count_sum += 1;
                        trees[i]->depth_sum += 2;
                    }
                } else {
                    tree *child = make_tree(splits[j], hard ? splits[j] : guesses, depth+1, n, hard, memo);
                    append_vector(trees[i]->children, child);
                    trees[i]->count_sum += child->count_sum;
                    trees[i]->depth_sum += child->count_sum + child->depth_sum;
                }
            }
        }
        free_splits(splits);
    }

    float best = 1.0 * trees[0]->depth_sum / trees[0]->count_sum;
    for(i = 1; i < n; ++i){
        float avg = 1.0 * trees[0]->depth_sum / trees[0]->count_sum;
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
    set_map(memo, truths, t);
    free(trees);
    free(scores);
    return t;
}

void print_split(vector *splits)
{
    size_t i, j;
    for(i = 0; i < ipow(3, WSIZE); ++i){
        if (!splits[i].data) continue;
        for(j = 0; j < *splits[i].size; ++j){
            printf("%s,", decode((size_t) splits[i].data[0][j]));
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

    if (t->children.data){
        for(i = 0; i < t->children.size[0]; ++i){
            print_tree((tree *)t->children.data[0][i], b2);
        }
    }

    if (t->leaves.data){
        for(i = 0; i < t->leaves.size[0]; ++i){
            char *l = decode((size_t) t->leaves.data[0][i]);
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
    for(i = 0; i < m->size; ++i){
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

int main()
{
    read_data();
    //fill_scores();
    vector word_indexes = make_vector(NWORDS);
    size_t i, j;
    for(i = 0; i < NWORDS; ++i){
        for(j = 0; j < NALL; ++j){
            if (0 == strncmp(WORDS[i], ALL[j], 5)){
                append_vector(word_indexes, (void*) j);
            }
        }
    }
    fill_scores_partial(word_indexes);
    vector all_indexes = make_vector(NALL);
    for(j = 0; j < NALL; ++j){
        append_vector(all_indexes, (void*) j);
    }

    printf("%ld %ld\n", *word_indexes.size, *word_indexes.capacity);

    if(0){
        printf("%d\n", score_guess("words", "words"));
        return 0;
    }
    //vector *splits = split(word_indexes, 6180);
    //print_split(splits);

    if (0){
        score_pair *scores = best_splits(word_indexes, all_indexes);
        for(i = 0; i < 40; ++i){
            printf("%f %s\n", scores[i].score, decode(scores[i].index));
        }
        return 0;
    }

    printf("%d\n", SCORES[0][0]);

    map *memo = make_map();
    tree *t =  make_tree(word_indexes, word_indexes, 0, 2, 1, memo);
    print_tree(t, "");
    printf("%f\n", 1.0*t->depth_sum / t->count_sum);
    free_memo(memo);
    free_vector(word_indexes);
    free_vector(all_indexes);
    free_map(memo);
}

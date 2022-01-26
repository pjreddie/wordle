#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "vector.h"

#define WSIZE 5
#define NWORDS 2315
#define NALL 12972

char WORDS[NWORDS][WSIZE + 1];
char ALL[NALL][WSIZE + 1];
int SCORES[NALL][NALL];

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

int score_guess(char *truth_, char *guess_)
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
            score += pow(3, i)*2;
        }
    }
    for (i = 0; i < WSIZE; ++i) {
        char c = guess[i];
        if (!c) continue;
        for (j = 0; j < WSIZE; ++j) {
            if (c == truth[j]) {
                truth[j] = 0;
                score += pow(3, i);
                break;
            }
        }
    }

    return score;
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

void free_splits(vector *splits)
{
    size_t i;
    for(i = 0; i < pow(3, WSIZE); ++i){
        if(splits[i].data) free_vector(splits[i]);
    }
    free(splits);
}

vector *split(vector truths, size_t guess)
{
    vector *splits = calloc(pow(3, WSIZE), sizeof(vector));

    size_t i;
    for(i = 0; i < *truths.size; ++i){
        size_t truth = (size_t) get_vector(truths, i);
        int score = SCORES[truth][guess];
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
    float sum = 0;
    int n = 0;
    size_t i;
    for(i = 0; i < pow(3, WSIZE); ++i){
        if (!splits[i].data) continue;
        ++n;
        sum += *splits[i].size;
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
    for(i = 0; i < *guesses.size; ++i){
        size_t guess = (size_t) guesses.data[0][i];
        vector *splits = split(truths, guess);
        float avg = avg_split_size(splits);
        free_splits(splits);

        scores[i].score = avg;
        scores[i].index = guess;
    }
    qsort(scores, *guesses.size, sizeof(score_pair), score_pair_cmp);
    return scores;
}

typedef struct{
    size_t index;
    vector children;
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
    free(t);
}

typedef struct {
    int n;
    int depths;
} depth_counts;

depth_counts depth_counts_tree(tree *t, int depth, size_t prev)
{
    depth_counts c = {0};
    if(!t->children.data){
        c.n = 1;
        c.depths = depth+1;
        if(prev == t->index) c.depths = depth;
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

tree *make_tree(vector truths, vector guesses, int depth, int n, int hard)
{
    if(*truths.size == 1){
        tree *t = calloc(1, sizeof(tree));
        t->index = (size_t) truths.data[0][0];
        return t;
    }else{
        if (*guesses.size < n) n = *guesses.size;
        tree **trees = calloc(n, sizeof(tree *));
        score_pair *scores = best_splits(truths, guesses);
        int i, j;
        for(i = 0; i < n; ++i){
            size_t guess = scores[i].index;
            vector *splits = split(truths, guess);

            trees[i] = calloc(1, sizeof(tree));
            trees[i]->children = make_vector(0);
            trees[i]->index = guess;
            
            for (j = 0; j < pow(3, WSIZE); ++j){
                if(splits[j].data){
                    append_vector(trees[i]->children, make_tree(splits[j], hard ? splits[j] : guesses, depth+1, n, hard));
                }
            }
            free_splits(splits);
        }
        
        float best = avg_depth_tree(trees[0]);
        for(i = 1; i < n; ++i){
            float avg = avg_depth_tree(trees[i]);
            if(avg < best){
                best = avg;
                tree *swap = trees[0];
                trees[0] = trees[i];
                trees[i] = swap;
            }
        }
        tree *t = trees[0];
        for(i = 1; i < n; ++i){
            free_tree(trees[i]);
        }
        free(trees);
        free(scores);
        return t;
    }
}

void print_split(vector *splits)
{
    size_t i, j;
    for(i = 0; i < pow(3, WSIZE); ++i){
        if (!splits[i].data) continue;
        for(j = 0; j < *splits[i].size; ++j){
            printf("%s,", decode((size_t) splits[i].data[0][j]));
        }
        printf("\n");
    }
}


void print_tree(tree *t, char *buff, size_t prev)
{
    char *w = decode(t->index);   
    if (!t->children.data){
        if(prev == t->index){
            printf("%s\n", buff);       
        }else{
            printf("%s%s%s\n", buff, strlen(buff)?",":"", w);
        }
    } else {
        int i;
        for(i = 0; i < t->children.size[0]; ++i){
            char b2[1024] = {0};
            sprintf(b2, "%s%s%s", buff, strlen(buff)?",":"", w);
            print_tree((tree *)t->children.data[0][i], b2, t->index);
        }
    }
}

int main()
{
    read_data();
    fill_scores();
    vector word_indexes = make_vector(NWORDS);
    size_t i, j;
    for(i = 0; i < NWORDS; ++i){
        for(j = 0; j < NALL; ++j){
            if (0 == strncmp(WORDS[i], ALL[j], 5)){
                append_vector(word_indexes, (void*) j);
            }
        }
    }
    vector all_indexes = make_vector(NALL);
    for(j = 0; j < NALL; ++j){
        append_vector(all_indexes, (void*) j);
    }

    printf("%ld %ld\n", *word_indexes.size, *word_indexes.capacity);

    //vector *splits = split(word_indexes, 6180);
    //print_split(splits);

    best_splits(word_indexes, word_indexes);
    tree *t =  make_tree(word_indexes, word_indexes, 0, 10, 0);
    print_tree(t, "", -1);
    printf("%f\n", avg_depth_tree(t));
    free_tree(t);
    free_vector(word_indexes);
    free_vector(all_indexes);
}

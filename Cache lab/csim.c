#include "cachelab.h"
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Name: Leo Lin
// AndrewID: hungfanl

int opt, s, b, E;
unsigned long S, B;
char *file_directory = NULL;
unsigned long long z = 0;
FILE *pFile;
int *capacity;
int counthit = 0, countmiss = 0, evict_dirty = 0, cache_dirty = 0, eviction = 0;
struct line {
    unsigned long long tag;
    int dirty;
    struct line *next;
    struct line *pre;
};
typedef struct line line;

typedef line *set_;
typedef set_ *cache_;
cache_ cache;
csim_stats_t *result;

// Build a new cache, each set with E+2 lines (one dummy head and one dummy
// tail) Set dirty to 0 and tag to -1 Set head.next to tail and tail.pre to head
void buildCache() {
    result = (csim_stats_t *)malloc(sizeof(csim_stats_t));
    unsigned long line_size = (unsigned long)E + 2;
    cache = (struct line **)malloc(sizeof(struct line) * S * line_size);
    int i;
    int j;
    for (i = 0; i < (int)S; i++) {
        cache[i] = (struct line *)malloc(sizeof(struct line) * line_size);
        for (j = 1; j <= E; j++) {
            cache[i][j].tag = z - 1;
            cache[i][j].dirty = 0;
        }
        cache[i][0].dirty = 0;
        cache[i][0].tag = z - 1;
        cache[i][E + 1].dirty = 0;
        cache[i][E + 1].tag = z - 1;
        cache[i][0].next = &(cache[i][E + 1]);
        cache[i][E + 1].pre = &(cache[i][0]);
        capacity[i] = 0;
    }
}
// Free all the set in the cache and the cache itself
// while counting the number of dirty bytes in the cache
void freeCache() {
    int i, j;
    for (i = 0; i < (int)S; i++) {
        for (j = 0; j < E + 2; j++) {
            cache_dirty += (unsigned long)(cache[i][j].dirty) * B;
        }
        free(cache[i]);
    }
    free(cache);
}

// Whenever the cache is read or write, move the cache after head.
void addToFirst(int i, int j) {
    cache[i][j].next = cache[i][0].next;
    (*cache[i][j].next).pre = &(cache[i][j]);
    cache[i][0].next = &(cache[i][j]);
    cache[i][j].pre = &(cache[i][0]);
}

// Choose a memory that has tagbit equals -1 (empty) to input new byte.
int addBit(int i, unsigned long long tbit) {
    int j;
    for (j = 1; j <= E; j++) {
        if (cache[i][j].tag == z - 1) {
            cache[i][j].tag = tbit;
            capacity[i]++;
            addToFirst(i, j);
            return j;
        }
    }
    return j;
}

// remove the cache from the linkedlist, and call addToFirst function to put the
// node in the first place
void renew(int i, int j) {
    (*cache[i][j].pre).next = cache[i][j].next;
    (*cache[i][j].next).pre = cache[i][j].pre;
    addToFirst(i, j);
}

// Clear the last byte in the line (the one before tail), change the tag to -1
// add an eviction, add one more capacity for a new input
void removeLast(int i) {
    (*cache[i][E + 1].pre).tag = z - 1;
    if ((*cache[i][E + 1].pre).dirty == 1)
        evict_dirty += B;
    (*cache[i][E + 1].pre).dirty = 0;
    (*(*cache[i][E + 1].pre).pre).next = &(cache[i][E + 1]);
    cache[i][E + 1].pre = (*cache[i][E + 1].pre).pre;
    eviction++;
    capacity[i]--;
}

// Search in each line to see if the byte is already in cache
// If so, renew and add a hit
// If not, add a miss and check if line is full
// If the line is full, remove last from the line and add one evict
// add the byte into the cache
int searchBit(unsigned long long tagbit, int i) {
    int j;
    for (j = 1; j <= E; j++) {
        if (cache[i][j].tag == tagbit) {
            counthit++;
            renew(i, j);
            return j;
        }
    }
    countmiss++;
    if (capacity[i] == E)
        removeLast(i);
    return addBit(i, tagbit);
}

// calculate the block number, tag number and set number
// and pass to search function
void readCache(unsigned long long address, int size) {
    int block = ((1 << b) - 1) & (int)address;
    int blockcount = 0;
    while (block + size > blockcount) {
        int i = ((1 << s) - 1) & (int)(address >> b);
        unsigned long long tagbit = address >> (b + s);
        searchBit(tagbit, i);
        address += 1 << b;
        blockcount += 1 << b;
    }
}

// The only difference between read and write is to change the dirty bit in the
// cache
void writeCache(unsigned long long address, int size) {
    int block = ((1 << b) - 1) & (int)address;
    int blockcount = 0;
    int j;
    while (block + size > blockcount) {
        int i = ((1 << s) - 1) & (int)(address >> b);
        unsigned long long tagbit = address >> (b + s);
        j = searchBit(tagbit, i);
        cache[i][j].dirty = 1;
        address += 1 << b;
        blockcount += 1 << b;
    }
}

// void printSummary(const csim_stats_t *stats);

void readfile(char *file_directory) {
    buildCache();
    pFile = fopen(file_directory, "r");
    char access_type;
    unsigned long address;
    int size;
    // Choose read or write based on the access_type
    while (fscanf(pFile, " %c %lx, %d", &access_type, &address, &size) > 0) {
        if (access_type == 'L') {
            readCache(address, size);
        } else {
            writeCache(address, size);
        }
    }
    fclose(pFile);
    freeCache();
    printf("Hit: %d\n", counthit);
    printf("Miss: %d\n", countmiss);
    printf("Eviction: %d\n", eviction);
    printf("Evict_dirty: %d\n", evict_dirty);
    printf("Cache_dirty: %d\n", cache_dirty);
    result->hits = (unsigned long)counthit;
    result->misses = (unsigned long)countmiss;
    result->evictions = (unsigned long)eviction;
    result->dirty_evictions = (unsigned long)evict_dirty;
    result->dirty_bytes = (unsigned long)cache_dirty;
}

int main(int argc, char *argv[]) {
    /* looping over arguments */
    while (-1 != (opt = getopt(argc, argv, "s:E:b:t:"))) {
        /* determine which argument is processing */
        switch (opt) {
        case 's':
            s = atoi(optarg);
            S = 1 << s;
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            B = 1 << b;
            break;
        case 't':
            file_directory = optarg;
            break;
        default:
            printf("wrong argument\n");
            break;
        }
    }
    capacity = (int *)malloc(sizeof(int) * S);
    readfile(file_directory);
    printSummary(result);
    free(result);
    return 0;
}

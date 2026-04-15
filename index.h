#ifndef INDEX_H
#define INDEX_H

#include "pes.h"

typedef struct {
    int mode;
    ObjectID hash;
    uint32_t size;
    char path[256];
} IndexEntry;

typedef struct {
    IndexEntry entries[1024];
    int count;
} Index;

int index_load(Index *idx);
int index_save(const Index *idx);
int index_add(Index *idx, const char *path);
int index_status(const Index *idx);
IndexEntry* index_find(Index *idx, const char *path);
int index_remove(Index *idx, const char *path);

#endif

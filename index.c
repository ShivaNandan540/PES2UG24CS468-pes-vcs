#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// find entry
IndexEntry* index_find(Index *idx, const char *path) {
    for (int i = 0; i < idx->count; i++) {
        if (strcmp(idx->entries[i].path, path) == 0)
            return &idx->entries[i];
    }
    return NULL;
}

// load index
int index_load(Index *idx) {
    FILE *f = fopen(".pes/index", "r");

    if (!f) {
        idx->count = 0;
        return 0;
    }

    idx->count = 0;

    char hex[65];
    char path[256];

    while (fscanf(f, "%o %s %u %s\n",
                  &idx->entries[idx->count].mode,
                  hex,
                  &idx->entries[idx->count].size,
                  path) == 4) {

        hex_to_hash(hex, &idx->entries[idx->count].hash);
        strcpy(idx->entries[idx->count].path, path);

        idx->count++;
    }

    fclose(f);
    return 0;
}

// save index
int index_save(const Index *idx) {
    FILE *f = fopen(".pes/index", "w");
    if (!f) return -1;

    for (int i = 0; i < idx->count; i++) {
        char hex[65];
        hash_to_hex(&idx->entries[i].hash, hex);

        fprintf(f, "%o %s %u %s\n",
                idx->entries[i].mode,
                hex,
                idx->entries[i].size,
                idx->entries[i].path);
    }

    fclose(f);
    return 0;
}

// add file
int index_add(Index *idx, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    void *data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    ObjectID id;
    if (object_write(OBJ_BLOB, data, size, &id) != 0) {
        free(data);
        return -1;
    }

    free(data);

    IndexEntry *e = index_find(idx, path);

    if (!e) {
        e = &idx->entries[idx->count++];
    }

    e->mode = 100644;
    e->hash = id;
    e->size = size;
    strcpy(e->path, path);

    return 0;
}

// status
int index_status(const Index *idx) {
    printf("Staged changes:\n");

    if (idx->count == 0) {
        printf("    (nothing to show)\n");
    } else {
        for (int i = 0; i < idx->count; i++) {
            printf("    %s\n", idx->entries[i].path);
        }
    }

    return 0;
}

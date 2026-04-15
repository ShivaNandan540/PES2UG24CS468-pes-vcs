// commit.c — Commit creation and history traversal
//
// Commit object format (stored as text, one field per line):
//
//   tree <64-char-hex-hash>
//   parent <64-char-hex-hash>        ← omitted for the first commit
//   author <name> <unix-timestamp>
//   committer <name> <unix-timestamp>
//
//   <commit message>
//
// Note: there is a blank line between the headers and the message.
//
// PROVIDED functions: commit_parse, commit_serialize, commit_walk, head_read, head_update
// TODO functions:     commit_create

#include "commit.h"
#include "index.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

// Forward declarations (implemented in object.c)
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);

// ─── PROVIDED ────────────────────────────────────────────────────────────────

// Parse raw commit data into a Commit struct.
int commit_parse(const void *data, size_t len, Commit *commit_out) {
    (void)len;
    const char *p = (const char *)data;
    char hex[HASH_HEX_SIZE + 1];

    // "tree <hex>\n"
    if (sscanf(p, "tree %64s\n", hex) != 1) return -1;
    if (hex_to_hash(hex, &commit_out->tree) != 0) return -1;
    p = strchr(p, '\n') + 1;

    // optional "parent <hex>\n"
    if (strncmp(p, "parent ", 7) == 0) {
        if (sscanf(p, "parent %64s\n", hex) != 1) return -1;
        if (hex_to_hash(hex, &commit_out->parent) != 0) return -1;
        commit_out->has_parent = 1;
        p = strchr(p, '\n') + 1;
    } else {
        commit_out->has_parent = 0;
    }

    // "author <name> <timestamp>\n"
    char author_buf[256];
    uint64_t ts;
    if (sscanf(p, "author %255[^\n]\n", author_buf) != 1) return -1;
    // split off trailing timestamp
    char *last_space = strrchr(author_buf, ' ');
    if (!last_space) return -1;
    ts = (uint64_t)strtoull(last_space + 1, NULL, 10);
    *last_space = '\0';
    strncpy(commit_out->author, author_buf, sizeof(commit_out->author) - 1);
    commit_out->timestamp = ts;
    p = strchr(p, '\n') + 1;  // skip author line
    p = strchr(p, '\n') + 1;  // skip committer line
    p = strchr(p, '\n') + 1;  // skip blank line

    strncpy(commit_out->message, p, sizeof(commit_out->message) - 1);
    return 0;
}

// Serialize a Commit struct to the text format.
// Caller must free(*data_out).
int commit_serialize(const Commit *commit, void **data_out, size_t *len_out) {
    char tree_hex[HASH_HEX_SIZE + 1];
    char parent_hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit->tree, tree_hex);

    char buf[8192];
    int n = 0;
    n += snprintf(buf + n, sizeof(buf) - n, "tree %s\n", tree_hex);
    if (commit->has_parent) {
        hash_to_hex(&commit->parent, parent_hex);
        n += snprintf(buf + n, sizeof(buf) - n, "parent %s\n", parent_hex);
    }
    n += snprintf(buf + n, sizeof(buf) - n,
                  "author %s %" PRIu64 "\n"
                  "committer %s %" PRIu64 "\n"
                  "\n"
                  "%s",
                  commit->author, commit->timestamp,
                  commit->author, commit->timestamp,
                  commit->message);

    *data_out = malloc(n + 1);
    if (!*data_out) return -1;
    memcpy(*data_out, buf, n + 1);
    *len_out = (size_t)n;
    return 0;
}

// Walk commit history from HEAD to the root.
int commit_walk(commit_walk_fn callback, void *ctx) {
    ObjectID id;
    if (head_read(&id) != 0) return -1;

    while (1) {
        ObjectType type;
        void *raw;
        size_t raw_len;
        if (object_read(&id, &type, &raw, &raw_len) != 0) return -1;

        Commit c;
        int rc = commit_parse(raw, raw_len, &c);
        free(raw);
        if (rc != 0) return -1;

        callback(&id, &c, ctx);

        if (!c.has_parent) break;
        id = c.parent;
    }
    return 0;
}

// Read the current HEAD commit hash.
int head_read(ObjectID *id_out) {
    FILE *f = fopen(".pes/HEAD", "r");
    if (!f) return -1;

    char ref[256];
    if (!fgets(ref, sizeof(ref), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    ref[strcspn(ref, "\n")] = 0;

    if (strncmp(ref, "ref: ", 5) == 0) {
        char path[512];
        snprintf(path, sizeof(path), ".pes/%s", ref + 5);

        f = fopen(path, "r");
        if (!f) return -1;

        char hash[65];
        if (!fgets(hash, sizeof(hash), f)) {
            fclose(f);
            return -1;
        }
        fclose(f);

        hash[strcspn(hash, "\n")] = 0;

        hex_to_hash(hash, id_out);
        return 0;
    }

    return -1;
}

// Update the current branch ref to point to a new commit atomically.
int head_update(const ObjectID *id) {
    // Step 1: read HEAD
    FILE *f = fopen(".pes/HEAD", "r");
    if (!f) return -1;

    char ref[256];
    if (!fgets(ref, sizeof(ref), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    // remove newline
    ref[strcspn(ref, "\n")] = 0;

    // Step 2: get branch path
    if (strncmp(ref, "ref: ", 5) != 0) return -1;

    char path[512];
    snprintf(path, sizeof(path), ".pes/%s", ref + 5);

    // Step 3: convert hash to hex
    char hex[65];
    hash_to_hex(id, hex);

    // Step 4: write to branch file
    f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "%s\n", hex);
    fclose(f);

    return 0;
}

// ─── TODO: Implement these ───────────────────────────────────────────────────

// Create a new commit from the current staging area.
//
// HINTS - Useful functions to call:
//   - tree_from_index   : writes the directory tree and gets the root hash
//   - head_read         : gets the parent commit hash (if any)
//   - pes_author        : retrieves the author name string (from pes.h)
//   - time(NULL)        : gets the current unix timestamp
//   - commit_serialize  : converts the filled Commit struct to a text buffer
//   - object_write      : saves the serialized text as OBJ_COMMIT
//   - head_update       : moves the branch pointer to your new commit
//
// Returns 0 on success, -1 on error.
int commit_create(const char *message, ObjectID *id_out) {
    // Step 1: create tree
    ObjectID tree_id;
    if (tree_from_index(&tree_id) != 0) return -1;

    // Step 2: build commit content
    char buffer[4096];
    char tree_hex[65];
    hash_to_hex(&tree_id, tree_hex);

    const char *author = pes_author();
    long timestamp = time(NULL);

    int len = 0;

    len += sprintf(buffer + len, "tree %s\n", tree_hex);
    len += sprintf(buffer + len, "author %s %ld\n", author, timestamp);
    len += sprintf(buffer + len, "committer %s %ld\n\n", author, timestamp);
    len += sprintf(buffer + len, "%s\n", message);

    // Step 3: store commit
    if (object_write(OBJ_COMMIT, buffer, len, id_out) != 0)
        return -1;

    // Step 4: update HEAD
    if (head_update(id_out) != 0)
        return -1;

    return 0;
}

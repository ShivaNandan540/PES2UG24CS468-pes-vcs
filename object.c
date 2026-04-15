// object.c — Content-addressable object store
//
// Every piece of data (file contents, directory listings, commits) is stored
// as an "object" named by its SHA-256 hash. Objects are stored under
// .pes/objects/XX/YYYYYY... where XX is the first two hex characters of the
// hash (directory sharding).
//
// PROVIDED functions: compute_hash, object_path, object_exists, hash_to_hex, hex_to_hash
// TODO functions:     object_write, object_read

#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/sha.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(id_out->hash, &ctx);
}

// Get the filesystem path where an object should be stored.
// Format: .pes/objects/XX/YYYYYYYY...
// The first 2 hex chars form the shard directory; the rest is the filename.
void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ─── TODO: Implement these ──────────────────────────────────────────────────

// Write an object to the store.
//
// Object format on disk:
//   "<type> <size>\0<data>"
//   where <type> is "blob", "tree", or "commit"
//   and <size> is the decimal string of the data length
//
// Steps:
//   1. Build the full object: header ("blob 16\0") + data
//   2. Compute SHA-256 hash of the FULL object (header + data)
//   3. Check if object already exists (deduplication) — if so, just return success
//   4. Create shard directory (.pes/objects/XX/) if it doesn't exist
//   5. Write to a temporary file in the same shard directory
//   6. fsync() the temporary file to ensure data reaches disk
//   7. rename() the temp file to the final path (atomic on POSIX)
//   8. Open and fsync() the shard directory to persist the rename
//   9. Store the computed hash in *id_out

// HINTS - Useful syscalls and functions for this phase:
//   - sprintf / snprintf : formatting the header string
//   - compute_hash       : hashing the combined header + data
//   - object_exists      : checking for deduplication
//   - mkdir              : creating the shard directory (use mode 0755)
//   - open, write, close : creating and writing to the temp file
//                          (Use O_CREAT | O_WRONLY | O_TRUNC, mode 0644)
//   - fsync              : flushing the file descriptor to disk
//   - rename             : atomically moving the temp file to the final path
//
//
// Returns 0 on success, -1 on error.
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    // Step 1: convert type to string
    const char *type_str;
    if (type == OBJ_BLOB) type_str = "blob";
    else if (type == OBJ_TREE) type_str = "tree";
    else type_str = "commit";

    // Step 2: create header
    char header[64];
    int header_len = sprintf(header, "%s %zu", type_str, len) + 1;

    // Step 3: combine header + data
    size_t total_len = header_len + len;
    unsigned char *buffer = malloc(total_len);
    if (!buffer) return -1;

    memcpy(buffer, header, header_len);
    memcpy(buffer + header_len, data, len);

    // Step 4: compute hash
    compute_hash(buffer, total_len, id_out);

    // Step 5: convert hash to hex
    char hex[65];
    hash_to_hex(id_out, hex);

    // Step 6: create directory + path
    char dir[3];
    strncpy(dir, hex, 2);
    dir[2] = '\0';

    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), ".pes/objects/%s", dir);

    char path[512];
    snprintf(path, sizeof(path), ".pes/objects/%s/%s", dir, hex + 2);

    // Step 7: create directories
    mkdir(".pes", 0755);
    mkdir(".pes/objects", 0755);
    mkdir(dir_path, 0755);

    // Step 8: write file
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(buffer);
        return -1;
    }

    fwrite(buffer, 1, total_len, f);
    fclose(f);

    free(buffer);

    return 0;
}

// Read an object from the store.
//
// Steps:
//   1. Build the file path from the hash using object_path()
//   2. Open and read the entire file
//   3. Parse the header to extract the type string and size
//   4. Verify integrity: recompute the SHA-256 of the file contents
//      and compare to the expected hash (from *id). Return -1 if mismatch.
//   5. Set *type_out to the parsed ObjectType
//   6. Allocate a buffer, copy the data portion (after the \0), set *data_out and *len_out
//
// HINTS - Useful syscalls and functions for this phase:
//   - object_path        : getting the target file path
//   - fopen, fread, fseek: reading the file into memory
//   - memchr             : safely finding the '\0' separating header and data
//   - strncmp            : parsing the type string ("blob", "tree", "- compute_hash       : re-hashing the read data for integrity verification
//   - memcmp             : comparing the computed hash against the requested hash
//   - malloc, memcpy     : allocating and returning the extracted data
//
// The caller is responsible for calling free(*data_out).
//
// The caller is responsible for calling free(*data_out).
// Returns 0 on success, -1 on error (file not found, corrupt, etc.).
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    // Step 1: convert hash to hex
    char hex[65];
    hash_to_hex(id, hex);

    // Step 2: construct path
    char path[512];
    snprintf(path, sizeof(path), ".pes/objects/%c%c/%s", hex[0], hex[1], hex + 2);

    // Step 3: open file
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    // Step 4: read file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    unsigned char *buffer = malloc(size);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    fread(buffer, 1, size, f);
    fclose(f);

    // ✅ Step 5: VERIFY INTEGRITY (IMPORTANT FIX)
    ObjectID check_id;
    compute_hash(buffer, size, &check_id);

    if (memcmp(check_id.hash, id->hash, 32) != 0) {
        free(buffer);
        return -1;
    }

    // Step 6: parse header
    char *space = strchr((char *)buffer, ' ');
    char *null_byte = strchr((char *)buffer, '\0');

    if (!space || !null_byte) {
        free(buffer);
        return -1;
    }

    *space = '\0';
    char *type_str = (char *)buffer;

    size_t data_len = strtoul(space + 1, NULL, 10);

    if (data_len > (size_t)(size - (null_byte - (char *)buffer) - 1)) {
        free(buffer);
        return -1;
    }

    // Step 7: determine type
    if (strcmp(type_str, "blob") == 0) *type_out = OBJ_BLOB;
    else if (strcmp(type_str, "tree") == 0) *type_out = OBJ_TREE;
    else *type_out = OBJ_COMMIT;

    // Step 8: extract data
    unsigned char *data_start = (unsigned char *)(null_byte + 1);

    *data_out = malloc(data_len);
    if (!*data_out) {
        free(buffer);
        return -1;
    }

    memcpy(*data_out, data_start, data_len);
    *len_out = data_len;

    free(buffer);
    return 0;
}

#include "pes.h"
#include "index.h"
#include "commit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// ---------------- INIT ----------------
void cmd_init() {
    mkdir(".pes", 0777);
    mkdir(".pes/objects", 0777);
    mkdir(".pes/refs", 0777);
    mkdir(".pes/refs/heads", 0777);

    FILE *f = fopen(".pes/HEAD", "w");
    if (f) {
        fprintf(f, "ref: refs/heads/main\n");
        fclose(f);
    }

    f = fopen(".pes/refs/heads/main", "w");
    if (f) fclose(f);

    printf("Initialized empty PES repository\n");
}

// ---------------- ADD ----------------
void cmd_add(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: pes add <file>...\n");
        return;
    }

    Index idx;
    index_load(&idx);

    for (int i = 2; i < argc; i++) {
        index_add(&idx, argv[i]);
    }

    index_save(&idx);
}

// ---------------- STATUS ----------------
void cmd_status() {
    Index idx;
    index_load(&idx);
    index_status(&idx);
}

// ---------------- COMMIT ----------------
int cmd_commit(int argc, char *argv[]) {
    if (argc < 4 || strcmp(argv[2], "-m") != 0) {
        printf("error: commit requires -m \"message\"\n");
        return -1;
    }

    const char *message = argv[3];

    ObjectID id;
    if (commit_create(message, &id) != 0) return -1;

    char hex[65];
    hash_to_hex(&id, hex);

    printf("Committed: %.12s... %s\n", hex, message);
    return 0;
}

// ---------------- LOG ----------------
void cmd_log() {
    printf("Log not implemented yet\n");
}

// ---------------- BRANCH (stub) ----------------
void cmd_branch(int argc, char *argv[]) {
    printf("Branch feature not implemented\n");
}

// ---------------- CHECKOUT (stub) ----------------
void cmd_checkout(int argc, char *argv[]) {
    printf("Checkout feature not implemented\n");
}

// ---------------- MAIN ----------------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: pes <command>\n");
        return 1;
    }

    const char *cmd = argv[1];

    if      (strcmp(cmd, "init") == 0)     cmd_init();
    else if (strcmp(cmd, "add") == 0)      cmd_add(argc, argv);
    else if (strcmp(cmd, "status") == 0)   cmd_status();
    else if (strcmp(cmd, "commit") == 0)   cmd_commit(argc, argv);
    else if (strcmp(cmd, "log") == 0)      cmd_log();
    else if (strcmp(cmd, "branch") == 0)   cmd_branch(argc, argv);
    else if (strcmp(cmd, "checkout") == 0) cmd_checkout(argc, argv);
    else {
        printf("Unknown command\n");
        return 1;
    }

    return 0;
}

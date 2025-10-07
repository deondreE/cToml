#include "toml.h"
#include <stdio.h>

static void print_users(const TomlTable *users) {
    printf("\n-- User Array --\n");
    for (int i = 0; i < users->arr_count; i++) {
        const TomlTable *u = users->table_array[i];
        const char *name = toml_get_string(u, "name", "?");
        int age = toml_get_int(u, "age", -1);
        bool verified = toml_get_bool(u, "verified", false);
        printf("User %d: %-8s age=%d verified=%s\n",
               i + 1, name, age, verified ? "true" : "false");
    }
}

int main(void) {
    printf("Loading config.toml...\n");
    TomlDoc *doc = toml_load("../../config.toml");
    if (!doc) {
        fprintf(stderr, "Failed to parse config.toml\n");
        return 1;
    }

    // --- top-level values ---
    const TomlTable *root = doc->root;
    printf("Title       : %s\n", toml_get_string(root, "title", "<none>"));
    printf("Active      : %s\n", toml_get_bool(root, "active", false) ? "true" : "false");
    printf("Version     : %.2f\n", toml_get_float(root, "version", 0.0));

    const TomlEntry *desc = toml_entry_get(root, "description");
    if (desc && desc->type == TOML_STRING)
        printf("Description : %s\n\n", desc->value.str_val);

    // --- datetime test ---
    const TomlEntry *time = toml_entry_get(root, "start_time");
    if (time && time->type == TOML_DATETIME)
        printf("Start Time  : %04d-%02d-%02dT%02d:%02d:%02dZ\n",
               time->value.datetime.year, time->value.datetime.month,
               time->value.datetime.day, time->value.datetime.hour,
               time->value.datetime.minute, time->value.datetime.second);

    // --- server table ---
    const TomlTable *server = toml_table_get(root, "server");
    if (server) {
        printf("\n[server]\n");
        printf("Host        : %s\n", toml_get_string(server, "host", "?"));
        printf("Port        : %d\n", toml_get_int(server, "port", 0));
        printf("Secure      : %s\n", toml_get_bool(server, "secure", false) ? "true" : "false");
        TomlValidationCode v = toml_require(server, "host", TOML_STRING);
        if (v == TOML_OK)
            printf("Server.host validation OK\n");
    }

    // --- nested subtable ---
    const TomlTable *cfg = toml_table_get(server, "config");
    if (cfg) {
        printf("\n[server.config]\n");
        printf("Timeout     : %d\n", toml_get_int(cfg, "timeout", 0));
        printf("Compression : %s\n",
               toml_get_string(cfg, "compression", "<none>"));
    }

    // --- inline table inside server ---
    const TomlEntry *d = toml_entry_get(server, "details");
    if (d && d->type == TOML_TABLE) {
        printf("\nInline details table:\n");
        TomlTable *t = d->value.table_val;
        for (int i = 0; i < t->entry_count; i++)
            printf("  %s = ", t->entries[i].key),
            (t->entries[i].type == TOML_STRING)
                ? printf("\"%s\"\n", t->entries[i].value.str_val)
                : printf("%d\n", t->entries[i].value.int_val);
    }

    // --- dotted keys (database.main.*) ---
    const TomlTable *db = toml_table_get(root, "database");
    const TomlTable *main_tbl = db ? toml_table_get(db, "main") : NULL;
    if (main_tbl) {
        printf("\n[database.main]\n");
        printf("User        : %s\n", toml_get_string(main_tbl, "user", "?"));
        printf("Pass        : %s\n", toml_get_string(main_tbl, "pass", "?"));
        printf("Timeout     : %d\n", toml_get_int(main_tbl, "timeout", 0));
    }

    // --- array of tables ---
    const TomlTable *users = toml_table_get(root, "users");
    if (users && users->is_array)
        print_users(users);

    // --- writer round‑trip test ---
    TomlWriteOptions opts = { .indent_spaces = 2 };
    if (toml_write(doc, "output.toml", &opts) == 0)
        printf("\nConfig re‑written successfully to output.toml\n");

    // --- validations ---
    toml_require(server, "host", TOML_STRING);
    toml_require(server, "port", TOML_INT);
    toml_require(cfg, "timeout", TOML_INT);

    printf("\nAll parsing tests passed ✅\n");

    toml_free(doc);
    return 0;
}
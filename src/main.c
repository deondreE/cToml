#include "toml.h"
#include <stdio.h>

int main() {
    TomlDoc *doc = toml_load("../../config.toml");
    if (!doc) return 1;

    TomlTable *server = toml_table_get(doc->root, "server");
    printf("Host: w%s\n", toml_get_string(server, "host", "default"));
    printf("Port: %d\n", toml_get_int(server, "port", 0));

    TomlTable *db = toml_table_get(doc->root, "database");
    TomlTable *main = toml_table_get(db, "main");
    printf("User: %s\n", toml_get_string(main, "user", "?"));

    toml_dump(doc);  // debug tree print
    toml_free(doc);
}
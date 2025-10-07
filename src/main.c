#include "toml.h"
#include <stdio.h>

int main(void) {
    TomlDoc *doc = toml_load("../../config.toml");
    if (!doc) {
        fprintf(stderr, "Failed to load config.toml\n");
        return 1;
    }

    const TomlEntry *name = toml_get(doc, "name");
    if (name && name->type == TOML_STRING) {
        printf("name: %s\n", name->value.str_val);
    }

    const TomlEntry *age = toml_get(doc, "age");
    if (age && age->type == TOML_INT) {
        printf("age: %d\n", age->value.int_val);
    }

    toml_free(doc);
    return 0;
}
#ifndef TOML_H
#define TOML_H

#include <stdbool.h>

#define MAX_KEY_LEN 128
#define MAX_VAL_LEN 256

typedef enum {
    TOML_STRING,
    TOML_INT
} TomlValueType;

typedef struct {
    char key[MAX_KEY_LEN];
    TomlValueType type;
    union {
        char str_val[MAX_VAL_LEN];
        int int_val;
    } value;
} TomlEntry;

typedef struct {
    TomlEntry *entries;
    int count;
    int capacity;
} TomlDoc;

TomlDoc *toml_load(const char *filename);
void toml_free(TomlDoc *doc);
const TomlEntry *toml_get(const TomlDoc *doc, const char *key);

#endif
#ifndef TOML_H
#define TOML_H

#include <stdbool.h>

#define MAX_KEY_LEN 128
#define MAX_VAL_LEN 256
#define MAX_ARRAY_ITEMS 64

typedef enum {
    TOML_STRING,
    TOML_INT,
    TOML_FLOAT,
    TOML_BOOL,
    TOML_ARRAY_INT,
    TOML_ARRAY_FLOAT,
    TOML_ARRAY_STRING,
    TOML_DATETIME
} TomlValueType;

typedef struct {
    int year, month, day, hour, minute, second;
    int tx_offset;
    bool has_time;
} TomlDatetime;

typedef struct TomlEntry {
    char key[MAX_KEY_LEN];
    TomlValueType type;
    union {
        char str_val[MAX_VAL_LEN];
        int int_val;
        double float_val;
        bool bool_val;
        TomlDatetime datetime;

        struct {
            int int_values[MAX_ARRAY_ITEMS];
            double float_values[MAX_ARRAY_ITEMS];
            char string_values[MAX_ARRAY_ITEMS][MAX_VAL_LEN];
            int length;
        } array;
    } value;
    int line_num;
} TomlEntry;

struct TomlTable;

typedef struct TomlTable {
    char name[MAX_KEY_LEN];

    TomlEntry *entries;
    int entry_count;
    int entry_capacity;

    struct TomlTable **subtables;
    int sub_count;
    int sub_capacity;

    struct TomlTable **table_array;
    int arr_count, arr_cap;
    bool is_array;
} TomlTable;

typedef struct {
    int line;
    char message[128];
} TomlError;

typedef struct {
    TomlError *errors;
    int count, cap;
} TomlErrorList;

typedef struct {
    TomlTable *root;
    TomlErrorList errs;
} TomlDoc;

// API
TomlDoc *toml_load(const char *filename);
void toml_free(TomlDoc *doc);

TomlTable *toml_table_get(TomlTable *parent, const char *name);
const TomlEntry *toml_entry_get(const TomlTable *table, const char *key);

// Convenient typed accessors
int toml_get_int(const TomlTable *table, const char *key, int def);
const char *toml_get_string(const TomlTable *table, const char *key,
                            const char *def);
bool toml_get_bool(const TomlTable *table, const char *key, bool def);
double toml_get_float(const TomlTable *table, const char *key, double def);

void toml_dump(const TomlDoc *doc);

#endif
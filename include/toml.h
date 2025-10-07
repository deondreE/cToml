#ifndef TOML_H
#define TOML_H

#include <stdbool.h>

#define MAX_KEY_LEN       128
#define MAX_VAL_LEN       256
#define MAX_ARRAY_ITEMS   64

typedef enum {
    TOML_STRING,
    TOML_INT,
    TOML_FLOAT,
    TOML_BOOL,
    TOML_DATETIME,
    TOML_ARRAY_INT,
    TOML_ARRAY_FLOAT,
    TOML_ARRAY_STRING,
    TOML_TABLE, // inline table
} TomlValueType;

typedef struct {
    int year, month, day;
    int hour, minute, second;
    int tz_offset; // minutes east of UTC, 0 = Z
    bool has_time;
} TomlDatetime;

typedef struct TomlEntry TomlEntry;
typedef struct TomlTable TomlTable;

// ---------- Entries and Tables ----------
struct TomlEntry {
    char key[MAX_KEY_LEN];
    TomlValueType type;
    char comment[128];
    union {
        char str_val[MAX_VAL_LEN];
        int int_val;
        double float_val;
        bool bool_val;
        TomlDatetime datetime;
        struct {
            int ints[MAX_ARRAY_ITEMS];
            double floats[MAX_ARRAY_ITEMS];
            char strings[MAX_ARRAY_ITEMS][MAX_VAL_LEN];
            int length;
        } array;
        TomlTable *table_val; // for inline tables
    } value;
    int line_num;
};

struct TomlTable {
    char name[MAX_KEY_LEN];
    TomlEntry *entries;
    int entry_count, entry_cap;

    TomlTable **subtables;
    int sub_count, sub_cap;

    TomlTable **table_array;
    int arr_count, arr_cap;

    bool is_array;
    char comment[128];
};

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

// ---------- Access API ----------
TomlDoc *toml_load(const char *filename);
void toml_free(TomlDoc *doc);

// Table & Entry access
TomlTable *toml_table_get(TomlTable *parent, const char *name);
const TomlEntry *toml_entry_get(const TomlTable *tbl, const char *key);

// Typed accessors
int toml_get_int(const TomlTable *t, const char *key, int def);
double toml_get_float(const TomlTable *t, const char *key, double def);
bool toml_get_bool(const TomlTable *t, const char *key, bool def);
const char *toml_get_string(const TomlTable *t, const char *key, const char *def);

// Dump/Debug
void toml_dump(const TomlDoc *doc);

// ---------- Writer API ----------
typedef struct {
    int indent_spaces;  // default 0
} TomlWriteOptions;

int toml_write(const TomlDoc *doc, const char *filename,
               const TomlWriteOptions *opts);

// ---------- Validation API ----------
typedef enum {
    TOML_OK,
    TOML_ERR_MISSING_KEY,
    TOML_ERR_TYPE_MISMATCH
} TomlValidationCode;

TomlValidationCode toml_expect_type(const TomlTable *t, const char *key,
                                    TomlValueType type);
TomlValidationCode toml_require(const TomlTable *t, const char *key,
                                TomlValueType type);

#endif
#define _CRT_SECURE_NO_WARNINGS
#include "toml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ----------------------------------------------------------
// Helpers
// ----------------------------------------------------------

static void trim(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

static void err_add(TomlErrorList *elist, int line, char *msg) {
    if (elist->count >= elist->cap) {
        elist->cap = elist->cap ? elist->cap * 2 : 8;
        elist->errors = realloc(elist->errors, elist->cap * sizeof(TomlError));
    }
    elist->errors[elist->count].line = line;
    strncpy(elist->errors[elist->count].message, msg,
            sizeof(elist->errors[elist->count].message) - 1);
    elist->count++;
}

static TomlEntry *toml_entry_add(TomlTable *t, const char *key) {
    if (t->entry_count >= t->entry_capacity) {
        t->entry_capacity = t->entry_capacity > 0 ? t->entry_capacity * 2 : 8;
        t->entries =
            realloc(t->entries, t->entry_capacity * sizeof(TomlEntry));
    }
    TomlEntry *e = &t->entries[t->entry_count++];
    memset(e, 0, sizeof(TomlEntry));
    strncpy(e->key, key, MAX_KEY_LEN - 1);
    return e;
}

static TomlTable *toml_table_add_sub(TomlTable *parent, const char *name) {
    for (int i = 0; i < parent->sub_count; i++) {
        if (strcmp(parent->subtables[i]->name, name) == 0)
            return parent->subtables[i];
    }
    if (parent->sub_count >= parent->sub_capacity) {
        parent->sub_capacity = parent->sub_capacity > 0 ? parent->sub_capacity * 2 : 4;
        parent->subtables =
            realloc(parent->subtables, parent->sub_capacity * sizeof(TomlTable *));
    }
    TomlTable *child = calloc(1, sizeof(TomlTable));
    strncpy(child->name, name, MAX_KEY_LEN - 1);
    parent->subtables[parent->sub_count++] = child;
    return child;
}

static TomlTable *toml_table_ensure_path(TomlTable *root, const char *path) {
    if (!path || *path == '\0') return root;
    char buf[256];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim(buf);

    TomlTable *cur = root;
    char *tok = strtok(buf, ".");
    while (tok) {
        cur = toml_table_add_sub(cur, tok);
        tok = strtok(NULL, ".");
    }
    return cur;
}

// ----------------------------------------------------------
// Value Parsing
// ----------------------------------------------------------

static TomlValueType parse_array(const char *val_text, TomlEntry *entry_out) {
    char buf[512];
    strncpy(buf, val_text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim(buf);

    entry_out->value.array.length = 0;
    if (strlen(buf) == 0)
        return TOML_ARRAY_INT; // empty array default type

    char *token = strtok(buf, ",");
    int int_count = 0, float_count = 0, str_count = 0;

    while (token && entry_out->value.array.length < MAX_ARRAY_ITEMS) {
        trim(token);
        if (*token == '"' && token[strlen(token) - 1] == '"') {
            token[strlen(token) - 1] = '\0';
            token++;
            strncpy(entry_out->value.array.string_values[str_count++],
                    token, MAX_VAL_LEN - 1);
        } else if (strchr(token, '.')) {
            entry_out->value.array.float_values[float_count++] = atof(token);
        } else if (*token != '\0') {
            entry_out->value.array.int_values[int_count++] = atoi(token);
        }
        entry_out->value.array.length++;
        token = strtok(NULL, ",");
    }

    if (str_count > 0)
        return TOML_ARRAY_STRING;
    else if (float_count > 0)
        return TOML_ARRAY_FLOAT;
    else
        return TOML_ARRAY_INT;
}

// ------------------------------------------------------------
// Multiline String and Datetime Detection
// ------------------------------------------------------------

static bool starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool looks_like_datetime(const char *s) {
    return (strchr(s, 'T') && strchr(s, '-') && strchr(s, ':'));
}

// ----------------------------------------------------------
// Loading / Parsing
// ----------------------------------------------------------

TomlDoc *toml_load(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", filename); return NULL; }

    TomlDoc *d = calloc(1, sizeof(TomlDoc));
    d->root = calloc(1, sizeof(TomlTable));
    strncpy(d->root->name, "root", 4);

    char line[1024];
    int lineno = 0;
    char current_path[128] = "";
    TomlTable *current = d->root;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        trim(line);
        if (!*line || line[0] == '#') continue;

        // Arrays of tables [[table]]
        if (starts_with(line, "[[")) {
            char buf[128];
            strncpy(buf, line + 2, strlen(line) - 4);
            buf[strlen(line) - 4] = '\0';
            trim(buf);
            current = ensure_path(d->root, buf);
            current->is_array = true;
            continue;
        }

        // Table [table]
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            strncpy(current_path, line+1, strlen(line)-2);
            current_path[strlen(line)-2] = '\0';
            trim(current_path);
            current = ensure_path(d->root, current_path);
            continue;
        }

        // Key‑value
        char *eq = strchr(line, '=');
        if (!eq) { err_add(&d->errs, lineno, "missing '='"); continue; }
        *eq = '\0';
        char keybuf[128]; strncpy(keybuf, line, sizeof(keybuf)-1); trim(keybuf);
        char *val = eq + 1; trim(val);

        // implicit dotted key path
        char key_path[128]; strncpy(key_path, keybuf, sizeof(key_path)-1);
        char *final_key = strrchr(key_path, '.');
        TomlTable *target =
            final_key ? ensure_path(d->root, key_path) : current;
        if (final_key) {
            *final_key = '\0'; final_key++;
        } else { final_key = keybuf; }

        TomlEntry *e = entry_add(target, final_key);
        e->line_num = lineno;

        // Multiline string
        if (starts_with(val, "\"\"\"")) {
            char buf[2048] = {0};
            while (fgets(line, sizeof(line), f)) {
                lineno++;
                if (strstr(line, "\"\"\"")) break;
                strcat(buf, line);
            }
            e->type = TOML_STRING;
            strncpy(e->value.str_val, buf, MAX_VAL_LEN-1);
            continue;
        }

        // Array
        if (val[0] == '[' && val[strlen(val)-1] == ']') {
            char inner[512];
            strncpy(inner, val+1, strlen(val)-2);
            inner[strlen(val)-2] = '\0';
            e->type = parse_array(inner, e);
            continue;
        }

        // String
        if (val[0] == '"' && val[strlen(val)-1] == '"') {
            val[strlen(val)-1] = '\0'; val++;
            e->type = TOML_STRING;
            strncpy(e->value.str_val, val, MAX_VAL_LEN-1);
            continue;
        }

        // Bool
        if (!strcmp(val, "true") || !strcmp(val, "false")) {
            e->type = TOML_BOOL;
            e->value.bool_val = !strcmp(val, "true");
            continue;
        }

        // Datetime
        if (looks_like_datetime(val)) {
            e->type = TOML_DATETIME;
            strncpy(e->value.str_val, val, MAX_VAL_LEN-1); // stored raw
            continue;
        }

        // Numeric
        if (strchr(val, '.')) {
            e->type = TOML_FLOAT; e->value.float_val = atof(val);
        } else {
            e->type = TOML_INT; e->value.int_val = atoi(val);
        }
    }

    fclose(f);
    return d;
}

// ----------------------------------------------------------
// Accessors
// ----------------------------------------------------------

TomlTable *toml_table_get(TomlTable *parent, const char *name) {
    if (!parent || !name) return NULL;
    for (int i = 0; i < parent->sub_count; i++) {
        if (strcmp(parent->subtables[i]->name, name) == 0)
            return parent->subtables[i];
    }
    return NULL;
}

const TomlEntry *toml_entry_get(const TomlTable *t, const char *key) {
    if (!t) return NULL;
    for (int i = 0; i < t->entry_count; i++) {
        if (strcmp(t->entries[i].key, key) == 0)
            return &t->entries[i];
    }
    return NULL;
}

int toml_get_int(const TomlTable *t, const char *key, int def) {
    const TomlEntry *e = toml_entry_get(t, key);
    return (e && e->type == TOML_INT) ? e->value.int_val : def;
}
const char *toml_get_string(const TomlTable *t, const char *key,
                            const char *def) {
    const TomlEntry *e = toml_entry_get(t, key);
    return (e && e->type == TOML_STRING) ? e->value.str_val : def;
}
bool toml_get_bool(const TomlTable *t, const char *key, bool def) {
    const TomlEntry *e = toml_entry_get(t, key);
    return (e && e->type == TOML_BOOL) ? e->value.bool_val : def;
}
double toml_get_float(const TomlTable *t, const char *k, double def) {
    const TomlEntry *e = toml_entry_get(t, k);
    return (e && e->type == TOML_FLOAT) ? e->value.float_val : def;
}

// ----------------------------------------------------------
// Cleanup
// ----------------------------------------------------------

static void dump_table(const TomlTable *t, int depth) {
    for (int i = 0; i < t->entry_count; i++) {
        for (int d = 0; d < depth; d++) printf("  ");
        const TomlEntry *e = &t->entries[i];
        printf("%s = ", e->key);
        switch (e->type) {
            case TOML_INT: printf("%d", e->value.int_val); break;
            case TOML_FLOAT: printf("%f", e->value.float_val); break;
            case TOML_BOOL: printf("%s", e->value.bool_val?"true":"false"); break;
            case TOML_STRING: printf("\"%s\"", e->value.str_val); break;
            case TOML_ARRAY_INT:
                printf("[ "); for (int j=0;j<e->value.array.length;j++)
                    printf("%d ", e->value.array.int_values[j]); printf("]");
                break;
            case TOML_DATETIME: printf("%s", e->value.str_val); break;
            default: printf("..."); break;
        }
        printf("\n");
    }
    for (int i=0;i<t->sub_count;i++) {
        for (int d=0; d<depth; d++) printf("  ");
        printf("[%s]\n", t->subtables[i]->name);
        dump_table(t->subtables[i], depth+1);
    }
}

void toml_dump(const TomlDoc *d) {
    dump_table(d->root, 0);
}

static void free_table(TomlTable *t) {
    if (!t) return;
    for (int i=0;i<t->sub_count;i++) free_table(t->subtables[i]);
    for (int i=0;i<t->arr_count;i++) free_table(t->table_array[i]);
    free(t->entries);
    free(t->subtables);
    free(t->table_array);
    free(t);
}

void toml_free(TomlDoc *d) {
    if (!d) return;
    free_table(d->root);
    free(d->errs.errors);
    free(d);
}
#include "toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void trim(char *str) {
  char *start = str;
  while (isspace((unsigned char)*start)) ++start;
  if (start != str) memmove(str, start, strlen(start) + 1);

  char *end = str + strlen(str) - 1; 
  while (end >= str && isspace((unsigned char)*end)) --end;
  *(end + 1) = '\0';
}

static void toml_add_entry(TomlDoc *doc, const char *key,
                           TomlValueType type, const void *val) {
    if (doc->count >= doc->capacity) {
        doc->capacity *= 2;
        doc->entries = realloc(doc->entries, doc->capacity * sizeof(TomlEntry));
        if (!doc->entries) {
            fprintf(stderr, "Memory allocation failed.\n");
            exit(EXIT_FAILURE);
        }
    }

    TomlEntry *entry = &doc->entries[doc->count++];
    strncpy(entry->key, key, MAX_KEY_LEN - 1);
    entry->key[MAX_KEY_LEN - 1] = '\0';
    entry->type = type;

    if (type == TOML_STRING) {
        strncpy(entry->value.str_val, (const char *)val, MAX_VAL_LEN - 1);
        entry->value.str_val[MAX_VAL_LEN - 1] = '\0';
    } else if (type == TOML_INT) {
        entry->value.int_val = *(const int *)val;
    }
}

TomlDoc *toml_load(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return NULL;
    }

    TomlDoc *doc = malloc(sizeof(TomlDoc));
    if (!doc) {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(file);
        return NULL;
    }

    doc->capacity = 8;
    doc->count = 0;
    doc->entries = malloc(doc->capacity * sizeof(TomlEntry));

    if (!doc->entries) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(doc);
        fclose(file);
        return NULL;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || strlen(line) < 3)
            continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        trim(key);
        trim(val);

        if (*val == '"' && val[strlen(val) - 1] == '"') {
            val[strlen(val) - 1] = '\0';
            val++;
            toml_add_entry(doc, key, TOML_STRING, val);
        } else {
            int int_val = atoi(val);
            toml_add_entry(doc, key, TOML_INT, &int_val);
        }
    }

    fclose(file);
    return doc;
}

void toml_free(TomlDoc *doc) {
    if (!doc) return;
    free(doc->entries);
    free(doc);
}

const TomlEntry *toml_get(const TomlDoc *doc, const char *key) {
    for (int i = 0; i < doc->count; i++) {
        if (strcmp(doc->entries[i].key, key) == 0)
            return &doc->entries[i];
    }
    return NULL;
}
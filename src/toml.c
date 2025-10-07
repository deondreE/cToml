#define _CRT_SECURE_NO_WARNINGS
#include "toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ------------------------------------------------------------
// Utility helpers
// ------------------------------------------------------------
static void trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

static bool starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void err_add(TomlErrorList *elist, int line, const char *msg) {
    if (elist->count >= elist->cap) {
        elist->cap = elist->cap ? elist->cap * 2 : 8;
        elist->errors = realloc(elist->errors, elist->cap * sizeof(TomlError));
    }
    elist->errors[elist->count].line = line;
    strncpy(elist->errors[elist->count].message, msg,
            sizeof(elist->errors[elist->count].message) - 1);
    elist->count++;
}

// ------------------------------------------------------------
// Table management
// ------------------------------------------------------------
static TomlEntry *entry_add(TomlTable *t, const char *key) {
    if (t->entry_count >= t->entry_cap) {
        t->entry_cap = t->entry_cap ? t->entry_cap * 2 : 8;
        t->entries = realloc(t->entries, t->entry_cap * sizeof(TomlEntry));
    }
    TomlEntry *e = &t->entries[t->entry_count++];
    memset(e, 0, sizeof(TomlEntry));
    strncpy(e->key, key, MAX_KEY_LEN - 1);
    return e;
}

static TomlTable *subtable_add(TomlTable *p, const char *name) {
    for (int i = 0; i < p->sub_count; i++)
        if (!strcmp(p->subtables[i]->name, name))
            return p->subtables[i];
    if (p->sub_count >= p->sub_cap) {
        p->sub_cap = p->sub_cap ? p->sub_cap * 2 : 4;
        p->subtables = realloc(p->subtables, p->sub_cap * sizeof(TomlTable *));
    }
    TomlTable *t = calloc(1, sizeof(TomlTable));
    strncpy(t->name, name, MAX_KEY_LEN - 1);
    p->subtables[p->sub_count++] = t;
    return t;
}

static TomlTable *ensure_path(TomlTable *root, const char *path) {
    if (!path || !*path) return root;
    char buf[256];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tok = strtok(buf, ".");
    TomlTable *cur = root;
    while (tok) {
        cur = subtable_add(cur, tok);
        tok = strtok(NULL, ".");
    }
    return cur;
}

// ------------------------------------------------------------
// Datetime Parsing
// ------------------------------------------------------------
static bool parse_datetime(const char *src, TomlDatetime *out) {
    int y, m, d, H = 0, M = 0, S = 0, tz_h = 0, tz_m = 0;
    char tz[8] = "";
    if (sscanf(src, "%d-%d-%dT%d:%d:%d%7s", &y, &m, &d, &H, &M, &S, tz) >= 3) {
        out->year = y; out->month = m; out->day = d;
        out->hour = H; out->minute = M; out->second = S;
        out->has_time = strchr(src, 'T') || strchr(src, ' ');
        if (tz[0] == 'Z') out->tz_offset = 0;
        else if (tz[0] == '+' || tz[0] == '-') {
            int sign = (tz[0] == '-') ? -1 : 1;
            sscanf(tz + 1, "%d:%d", &tz_h, &tz_m);
            out->tz_offset = sign * (tz_h * 60 + tz_m);
        } else out->tz_offset = 0;
        return true;
    }
    return false;
}

// ------------------------------------------------------------
// Inline Table Parsing
// ------------------------------------------------------------
static TomlTable *parse_inline_table(const char *src) {
    TomlTable *tbl = calloc(1, sizeof(TomlTable));
    if (!tbl) return NULL;

    // Make a local editable copy of the inline table block
    char buf[512];
    strncpy(buf, src, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim(buf);

    // Tokenize by comma
    char *token = strtok(buf, ",");
    while (token) {
        char *eq = strchr(token, '=');
        if (!eq) {
            token = strtok(NULL, ",");
            continue;
        }

        *eq = '\0';
        char key[MAX_KEY_LEN];
        strncpy(key, token, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';
        trim(key);

        // writable buffer for the value portion
        char val_buf[MAX_VAL_LEN];
        strncpy(val_buf, eq + 1, sizeof(val_buf) - 1);
        val_buf[sizeof(val_buf) - 1] = '\0';
        trim(val_buf);

        // pointer version so we can increment safely
        char *val = val_buf;

        TomlEntry *e = entry_add(tbl, key);

        // String value
        if (*val == '"' && val[strlen(val) - 1] == '"') {
            val[strlen(val) - 1] = '\0';
            val++;
            e->type = TOML_STRING;
            strncpy(e->value.str_val, val, MAX_VAL_LEN - 1);
            e->value.str_val[MAX_VAL_LEN - 1] = '\0';

        // Boolean
        } else if (!strcmp(val, "true") || !strcmp(val, "false")) {
            e->type = TOML_BOOL;
            e->value.bool_val = !strcmp(val, "true");

        // Float
        } else if (strchr(val, '.')) {
            e->type = TOML_FLOAT;
            e->value.float_val = atof(val);

        // Integer (fallback)
        } else {
            e->type = TOML_INT;
            e->value.int_val = atoi(val);
        }

        token = strtok(NULL, ",");
    }

    return tbl;
}

// ------------------------------------------------------------
// Array Parsing
// ------------------------------------------------------------
static TomlValueType parse_array(const char *src, TomlEntry *e) {
    char buf[512];
    strncpy(buf, src, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim(buf);
    e->value.array.length = 0;
    if (!strlen(buf)) return TOML_ARRAY_INT;
    char *tok = strtok(buf, ",");
    int ints = 0, floats = 0, strs = 0;
    while (tok && e->value.array.length < MAX_ARRAY_ITEMS) {
        trim(tok);
        if (*tok == '"' && tok[strlen(tok) - 1] == '"') {
            tok[strlen(tok)-1] = '\0'; tok++;
            strncpy(e->value.array.strings[strs++], tok, MAX_VAL_LEN - 1);
        } else if (strchr(tok, '.')) {
            e->value.array.floats[floats++] = atof(tok);
        } else {
            e->value.array.ints[ints++] = atoi(tok);
        }
        e->value.array.length++;
        tok = strtok(NULL, ",");
    }
    if (strs) return TOML_ARRAY_STRING;
    if (floats) return TOML_ARRAY_FLOAT;
    return TOML_ARRAY_INT;
}

// ------------------------------------------------------------
// Parser Core
// ------------------------------------------------------------
TomlDoc *toml_load(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", filename); return NULL; }

    TomlDoc *doc = calloc(1, sizeof(TomlDoc));
    doc->root = calloc(1, sizeof(TomlTable));
    strncpy(doc->root->name, "root", sizeof(doc->root->name)-1);

    char line[1024], current_path[128] = "";
    TomlTable *current = doc->root;
    int line_no = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        trim(line);
        if (!*line) continue;

        // Standalone comment
        if (line[0] == '#') {
            strncpy(current->comment, line + 1, sizeof(current->comment) - 1);
            continue;
        }

        // Table headers
        if (starts_with(line, "[[")) { // array of tables
            char name[128];
            strncpy(name, line + 2, strlen(line) - 4);
            name[strlen(line)-4] = '\0';
            trim(name);
            current = ensure_path(doc->root, name);
            current->is_array = true;
            continue;
        } else if (line[0] == '[' && line[strlen(line)-1] == ']') {
            strncpy(current_path, line+1, strlen(line)-2);
            current_path[strlen(line)-2] = '\0';
            trim(current_path);
            current = ensure_path(doc->root, current_path);
            continue;
        }

        // key/value with potential comment
        char *hash = strchr(line, '#');
        char comment[128] = "";
        if (hash) {
            strncpy(comment, hash + 1, sizeof(comment) - 1);
            *hash = '\0';
        }

        char *eq = strchr(line, '=');
        if (!eq) { err_add(&doc->errs, line_no, "missing '='"); continue; }
        *eq = '\0';
        char keybuf[128]; strncpy(keybuf, line, sizeof(keybuf)-1); trim(keybuf);
        char *val = eq + 1; trim(val);

        // dotted keys
        char keycopy[128]; strncpy(keycopy, keybuf, sizeof(keycopy)-1);
        char *final_key = strrchr(keycopy, '.');
        TomlTable *target = final_key ? ensure_path(doc->root, keycopy) : current;
        if (final_key) {
            *final_key = '\0'; final_key++;
        } else final_key = keybuf;

        TomlEntry *e = entry_add(target, final_key);
        e->line_num = line_no;
        strncpy(e->comment, comment, sizeof(e->comment)-1);

        // Detect multiline string
        if (starts_with(val, "\"\"\"")) {
            char buf[2048] = "";
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "\"\"\"")) break;
                strcat(buf, line);
            }
            e->type = TOML_STRING;
            strncpy(e->value.str_val, buf, MAX_VAL_LEN - 1);
            continue;
        }

        // Inline table
        if (val[0] == '{' && val[strlen(val)-1] == '}') {
            char inner[512];
            strncpy(inner, val+1, strlen(val)-2);
            inner[strlen(val)-2] = '\0';
            e->type = TOML_TABLE;
            e->value.table_val = parse_inline_table(inner);
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
            val[strlen(val)-1]='\0'; val++;
            e->type = TOML_STRING;
            strncpy(e->value.str_val, val, MAX_VAL_LEN-1);
            continue;
        }

        // Bool
        if (!strcmp(val,"true")||!strcmp(val,"false")) {
            e->type = TOML_BOOL; e->value.bool_val=!strcmp(val,"true");
            continue;
        }

        // Datetime
        TomlDatetime dt;
        if (parse_datetime(val, &dt)) {
            e->type = TOML_DATETIME; e->value.datetime = dt;
            continue;
        }

        // Number fallback
        if (strchr(val, '.')) {
            e->type = TOML_FLOAT; e->value.float_val = atof(val);
        } else {
            e->type = TOML_INT; e->value.int_val = atoi(val);
        }
    }

    fclose(f);
    return doc;
}

// ------------------------------------------------------------
// Accessors
// ------------------------------------------------------------
TomlTable *toml_table_get(TomlTable *p, const char *name) {
    for (int i=0;i<p->sub_count;i++)
        if (!strcmp(p->subtables[i]->name,name))
            return p->subtables[i];
    return NULL;
}

const TomlEntry *toml_entry_get(const TomlTable *t,const char *key){
    for (int i=0;i<t->entry_count;i++)
        if (!strcmp(t->entries[i].key,key))
            return &t->entries[i];
    return NULL;
}

int toml_get_int(const TomlTable *t,const char *k,int def){
    const TomlEntry *e=toml_entry_get(t,k);
    return (e&&e->type==TOML_INT)?e->value.int_val:def;
}
double toml_get_float(const TomlTable *t,const char *k,double def){
    const TomlEntry *e=toml_entry_get(t,k);
    return (e&&e->type==TOML_FLOAT)?e->value.float_val:def;
}
bool toml_get_bool(const TomlTable *t,const char *k,bool def){
    const TomlEntry *e=toml_entry_get(t,k);
    return (e&&e->type==TOML_BOOL)?e->value.bool_val:def;
}
const char* toml_get_string(const TomlTable *t,const char *k,const char *def){
    const TomlEntry *e=toml_entry_get(t,k);
    return (e&&e->type==TOML_STRING)?e->value.str_val:def;
}

// ------------------------------------------------------------
// Validation
// ------------------------------------------------------------
TomlValidationCode toml_expect_type(const TomlTable *t, const char *key,
                                    TomlValueType type) {
    const TomlEntry *e = toml_entry_get(t, key);
    if (!e) return TOML_ERR_MISSING_KEY;
    if (e->type != type) return TOML_ERR_TYPE_MISMATCH;
    return TOML_OK;
}

TomlValidationCode toml_require(const TomlTable *t, const char *key,
                                TomlValueType type) {
    TomlValidationCode v = toml_expect_type(t, key, type);
    if (v == TOML_ERR_MISSING_KEY)
        fprintf(stderr,"Missing key: %s in table %s\n",key,t->name);
    else if (v == TOML_ERR_TYPE_MISMATCH)
        fprintf(stderr,"Wrong type for key: %s in table %s\n",key,t->name);
    return v;
}

// ------------------------------------------------------------
// Writer
// ------------------------------------------------------------
static void write_indent(FILE *f,int lvl,int sp){for(int i=0;i<lvl*sp;i++)fputc(' ',f);}

static void write_escaped_string(FILE *f,const char *s){
    fputc('"',f);for(;*s;s++){switch(*s){
        case '\\':fputs("\\\\",f);break;
        case '"':fputs("\\\"",f);break;
        case '\n':fputs("\\n",f);break;
        case '\r':fputs("\\r",f);break;
        case '\t':fputs("\\t",f);break;
        default:fputc(*s,f);
    }}fputc('"',f);
}

static void write_array(FILE *f,const TomlEntry *e){
    fprintf(f,"[");
    for(int i=0;i<e->value.array.length;i++){
        if(i>0)fprintf(f,", ");
        if(e->type==TOML_ARRAY_INT)fprintf(f,"%d",e->value.array.ints[i]);
        else if(e->type==TOML_ARRAY_FLOAT)fprintf(f,"%g",e->value.array.floats[i]);
        else if(e->type==TOML_ARRAY_STRING)write_escaped_string(f,e->value.array.strings[i]);
    }
    fprintf(f,"]");
}

static void write_table(FILE *f,const TomlTable *t,int depth,int indent){
    if(strlen(t->comment)>0)fprintf(f,"#%s\n",t->comment);
    for(int i=0;i<t->entry_count;i++){
        const TomlEntry *e=&t->entries[i];
        write_indent(f,depth,indent);
        fprintf(f,"%s = ",e->key);
        switch(e->type){
            case TOML_INT:fprintf(f,"%d",e->value.int_val);break;
            case TOML_FLOAT:fprintf(f,"%g",e->value.float_val);break;
            case TOML_BOOL:fprintf(f,e->value.bool_val?"true":"false");break;
            case TOML_STRING:write_escaped_string(f,e->value.str_val);break;
            case TOML_ARRAY_INT:
            case TOML_ARRAY_FLOAT:
            case TOML_ARRAY_STRING:write_array(f,e);break;
            case TOML_DATETIME:
                fprintf(f,"%04d-%02d-%02dT%02d:%02d:%02dZ",
                        e->value.datetime.year,e->value.datetime.month,
                        e->value.datetime.day,e->value.datetime.hour,
                        e->value.datetime.minute,e->value.datetime.second);
                break;
            case TOML_TABLE:
                fprintf(f,"{"); for(int j=0;j<e->value.table_val->entry_count;j++){
                    const TomlEntry *ie=&e->value.table_val->entries[j];
                    if(j>0)fprintf(f,", ");
                    fprintf(f,"%s = ",ie->key);
                    if(ie->type==TOML_STRING)write_escaped_string(f,ie->value.str_val);
                    else if(ie->type==TOML_INT)fprintf(f,"%d",ie->value.int_val);
                    else if(ie->type==TOML_BOOL)fprintf(f,ie->value.bool_val?"true":"false");
                    else if(ie->type==TOML_FLOAT)fprintf(f,"%g",ie->value.float_val);
                } fprintf(f,"}");
                break;
        }
        if(strlen(e->comment)>0)fprintf(f,"  # %s",e->comment);
        fprintf(f,"\n");
    }
    for(int i=0;i<t->sub_count;i++){
        fprintf(f,"\n");
        const TomlTable *st=t->subtables[i];
        write_indent(f,depth,indent);fprintf(f,"[%s]\n",st->name);
        write_table(f,st,depth+1,indent);
    }
}

int toml_write(const TomlDoc *doc,const char *file,const TomlWriteOptions *opts){
    FILE *f=fopen(file,"w"); if(!f)return -1;
    int ind=opts?opts->indent_spaces:0;
    write_table(f,doc->root,0,ind);
    fclose(f); return 0;
}

// ------------------------------------------------------------
// Dump / Free
// ------------------------------------------------------------
static void dump_table(const TomlTable *t,int d){
    for(int i=0;i<t->entry_count;i++){
        for(int j=0;j<d;j++)printf("  ");
        printf("%s\n",t->entries[i].key);
    }
    for(int i=0;i<t->sub_count;i++){
        for(int j=0;j<d;j++)printf("  ");
        printf("[%s]\n",t->subtables[i]->name);
        dump_table(t->subtables[i],d+1);
    }
}
void toml_dump(const TomlDoc *doc){dump_table(doc->root,0);}

static void free_table(TomlTable *t){
    if(!t)return;
    for(int i=0;i<t->sub_count;i++)free_table(t->subtables[i]);
    for(int i=0;i<t->arr_count;i++)free_table(t->table_array[i]);
    free(t->entries); free(t->subtables); free(t->table_array); free(t);
}
void toml_free(TomlDoc *d){
    if(!d)return;
    free_table(d->root);
    free(d->errs.errors);
    free(d);
}
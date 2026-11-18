#ifndef ZOD_YAML_H
#define ZOD_YAML_H

/* zod-yaml: dependency-free YAML subset reader. C99, usable from C++11.
 *
 * NOT a full YAML parser. Unsupported syntax is rejected, never guessed.
 * FOR PERSONAL PURPOSE - not production ready (yet)
 *
 * Single header. In exactly one source file, before including it:
 *     #define ZOD_YAML_IMPLEMENTATION
 * C++ users can include zodyaml.hpp (namespace zod::yaml) instead.
 *
 * Supported, with examples:
 *
 *   # comments, full line or after a value
 *   version: v1                      # plain scalar
 *   title: "Quoted: text, # kept"    # double-quoted, escapes \" and \\
 *   url: https://app.example         # colon not followed by a space is text
 *
 *   server:                          # block map, any nesting
 *     port: 8080
 *     origins:                       # block list
 *       - https://app.example
 *       - "https://other.example"
 *
 *   operations:
 *     - id: create_match             # list item that starts a map
 *       tags: [Matches, Upload]      # flow list
 *     - id: list_matches
 *       limit: { key: ip, max: 10 }  # flow map
 *
 *   security: [{ JwtAuth: [] }]      # flow collections nest
 *   empty_map: {}
 *   empty_list: []
 *
 * Rejected with ZOD_YAML_ERR_UNSUPPORTED, with the line number:
 *
 *   script: |                        # block scalars ( | and > )
 *   base: &anchor                    # anchors, aliases, merge keys
 *   value: !!str 5                   # tags
 *   <tab>key: value                  # tab indentation
 *   ---                              # multiple documents
 *
 * Scalars are text. Callers convert. Maps keep insertion order.
 * Everything returned points into the document and lives until zod_yaml_free().
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZOD_YAML_ERR_MSG_CAP 128

typedef enum zod_yaml_status {
    ZOD_YAML_OK = 0,
    ZOD_YAML_ERR_SYNTAX,
    ZOD_YAML_ERR_UNSUPPORTED,
    ZOD_YAML_ERR_DUPLICATE_KEY,
    ZOD_YAML_ERR_IO,
    ZOD_YAML_ERR_NOMEM,
    ZOD_YAML_ERR_DEPTH
} zod_yaml_status;

typedef enum zod_yaml_kind {
    ZOD_YAML_NONE = 0,
    ZOD_YAML_MAP,
    ZOD_YAML_LIST,
    ZOD_YAML_SCALAR
} zod_yaml_kind;

typedef enum zod_yaml_presence { ZOD_YAML_MISSING = 0, ZOD_YAML_FOUND } zod_yaml_presence;

typedef struct zod_yaml_error {
    zod_yaml_status status;
    int             line;   /* 1-based, 0 when unknown */
    int             column; /* 1-based, 0 when unknown */
    char            message[ZOD_YAML_ERR_MSG_CAP];
} zod_yaml_error;

typedef struct zod_yaml_doc  zod_yaml_doc;
typedef struct zod_yaml_node zod_yaml_node;

zod_yaml_status zod_yaml_parse(const char *text, size_t len, zod_yaml_doc **doc,
                               zod_yaml_error *err);
zod_yaml_status zod_yaml_load(const char *path, zod_yaml_doc **doc, zod_yaml_error *err);
void            zod_yaml_free(zod_yaml_doc *doc);

const zod_yaml_node *zod_yaml_root(const zod_yaml_doc *doc);
zod_yaml_kind        zod_yaml_kind_of(const zod_yaml_node *node);
const char          *zod_yaml_str(const zod_yaml_node *node);

const zod_yaml_node *zod_yaml_get(const zod_yaml_node *map, const char *key);
zod_yaml_presence    zod_yaml_has(const zod_yaml_node *map, const char *key);

const zod_yaml_node *zod_yaml_path(const zod_yaml_node *from, ...);

size_t               zod_yaml_len(const zod_yaml_node *node);
const zod_yaml_node *zod_yaml_at(const zod_yaml_node *list, size_t index);
const char          *zod_yaml_key_at(const zod_yaml_node *map, size_t index);
const zod_yaml_node *zod_yaml_val_at(const zod_yaml_node *map, size_t index);

#ifdef __cplusplus
}
#endif

#endif

#ifdef ZOD_YAML_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct zod_yaml_internal_string_view {
    const char *data;
    size_t      len;
} zod_yaml_internal_string_view;

static inline zod_yaml_internal_string_view zod_yaml_internal_sv(const char *data, size_t len) {
    zod_yaml_internal_string_view view = { data, len };
    return view;
}

static inline int zod_yaml_internal_sv_equals(zod_yaml_internal_string_view view,
                                              const char                   *text) {
    size_t text_len = strlen(text);

    return view.len == text_len && memcmp(view.data, text, text_len) == 0;
}

typedef enum zod_yaml_scalar_style { ZOD_YAML_PLAIN = 0, ZOD_YAML_QUOTED } zod_yaml_scalar_style;

typedef struct zod_yaml_node_scalar {
    const char           *text;
    zod_yaml_scalar_style style;
} zod_yaml_node_scalar;

typedef struct zod_yaml_node_list {
    size_t                      count;
    const zod_yaml_node *const *items;
} zod_yaml_node_list;

typedef struct zod_yaml_node_entry {
    const char          *key;
    const zod_yaml_node *value;
} zod_yaml_node_entry;

typedef struct zod_yaml_node_map {
    size_t                     count;
    const zod_yaml_node_entry *entries; /* insertion order */
} zod_yaml_node_map;

struct zod_yaml_node {
    zod_yaml_kind kind;
    int           line; /* 1-based source line */
    union {
        zod_yaml_node_scalar scalar;
        zod_yaml_node_list   list;
        zod_yaml_node_map    map;
    } as; /* valid member is picked by kind */
};

struct zod_yaml_doc {
    zod_yaml_node root;
};

static const zod_yaml_node zod_yaml_none = {
    .kind      = ZOD_YAML_NONE,
    .line      = 0,
    .as.scalar = { .text = NULL, .style = ZOD_YAML_PLAIN },
};

static void zod_yaml_set_error(zod_yaml_error *err, zod_yaml_status status, const char *message) {
    if (err == NULL) {
        return;
    }

    err->status = status;
    err->line   = 0;
    err->column = 0;
    snprintf(err->message, ZOD_YAML_ERR_MSG_CAP, "%s", message);
}

zod_yaml_status zod_yaml_parse(const char *text, size_t len, zod_yaml_doc **doc,
                               zod_yaml_error *err) {
    *doc = NULL;
    if (text == NULL && len != 0) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_SYNTAX, "text is NULL but len is not 0");
        return ZOD_YAML_ERR_SYNTAX;
    }
    zod_yaml_doc *new_doc = calloc(1, sizeof(*new_doc));
    if (new_doc == NULL) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_NOMEM, "out of memory");
        return ZOD_YAML_ERR_NOMEM;
    }

    new_doc->root.kind = ZOD_YAML_NONE;
    *doc               = new_doc;

    return ZOD_YAML_OK;
}

zod_yaml_status zod_yaml_load(const char *path, zod_yaml_doc **doc, zod_yaml_error *err) {
    (void)path;

    *doc = NULL;
    zod_yaml_set_error(err, ZOD_YAML_ERR_UNSUPPORTED, "not implemented");
    return ZOD_YAML_ERR_UNSUPPORTED;
}

void zod_yaml_free(zod_yaml_doc *doc) { free(doc); }

const zod_yaml_node *zod_yaml_root(const zod_yaml_doc *doc) {
    return doc == NULL ? &zod_yaml_none : &doc->root;
}

zod_yaml_kind zod_yaml_kind_of(const zod_yaml_node *node) { return node->kind; }

const char *zod_yaml_str(const zod_yaml_node *node) {
    (void)node;
    return NULL;
}

const zod_yaml_node *zod_yaml_get(const zod_yaml_node *map, const char *key) {
    (void)map;
    (void)key;
    return &zod_yaml_none;
}

zod_yaml_presence zod_yaml_has(const zod_yaml_node *map, const char *key) {
    (void)map;
    (void)key;
    return ZOD_YAML_MISSING;
}

const zod_yaml_node *zod_yaml_path(const zod_yaml_node *from, ...) {
    va_list              keys;
    const char          *key;
    const zod_yaml_node *node = from;

    va_start(keys, from);
    while (node != &zod_yaml_none && (key = va_arg(keys, const char *)) != NULL) {
        node = zod_yaml_get(node, key);
    }
    va_end(keys);

    return node;
}

size_t zod_yaml_len(const zod_yaml_node *node) {
    (void)node;
    return 0;
}

const zod_yaml_node *zod_yaml_at(const zod_yaml_node *list, size_t index) {
    (void)list;
    (void)index;
    return &zod_yaml_none;
}

const char *zod_yaml_key_at(const zod_yaml_node *map, size_t index) {
    (void)map;
    (void)index;
    return NULL;
}

const zod_yaml_node *zod_yaml_val_at(const zod_yaml_node *map, size_t index) {
    (void)map;
    (void)index;
    return &zod_yaml_none;
}

#endif

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

#define ZOD_YAML_ERR_MSG_CAP     128
#define ZOD_YAML_MAX_INPUT_BYTES ((size_t)5 * 1024 * 1024)  // 5 MB

typedef enum zod_yaml_status {
    ZOD_YAML_OK = 0,
    ZOD_YAML_ERR_SYNTAX,
    ZOD_YAML_ERR_UNSUPPORTED,
    ZOD_YAML_ERR_DUPLICATE_KEY,
    ZOD_YAML_ERR_IO,
    ZOD_YAML_ERR_NOMEM,
    ZOD_YAML_ERR_DEPTH,
    ZOD_YAML_ERR_TOO_LARGE
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZOD_YAML_AREA_BLOCK_CAP 4096
#define ZOD_YAML_KEY_SEP        ':'
#define ZOD_YAML_MSG_NOMEM      "out of memory"
#define ZOD_YAML_MSG_INDENT     "unexpected indentation"
#define ZOD_YAML_MAX_ENTRIES    1024
#define ZOD_YAML_MAX_DEPTH      64
#define ZOD_YAML_DOC_MARKER     "---"
#define ZOD_YAML_DOC_MARKER_LEN (sizeof(ZOD_YAML_DOC_MARKER) - 1)

typedef struct zod_yaml_internal_block {
    struct zod_yaml_internal_block *next;

    size_t        used;
    size_t        cap;
    unsigned char bytes[];  // data

} zod_yaml_internal_block;

typedef struct zod_yaml_internal_area {
    zod_yaml_internal_block *head;
} zod_yaml_internal_area;

static void *zod_yaml_internal_area_bump(zod_yaml_internal_block *block, size_t size,
                                         size_t align) {
    uintptr_t base   = (uintptr_t)block->bytes;
    uintptr_t start  = (base + block->used + align - 1) & ~((uintptr_t)(align - 1));
    size_t    offset = (size_t)(start - base);

    if (offset > block->cap || size > block->cap - offset) {
        return NULL;
    }
    block->used = offset + size;
    return (void *)start;
}

static void *zod_yaml_internal_area_alloc(zod_yaml_internal_area *area, size_t size, size_t align) {
    void *ptr = NULL;
    if (area->head != NULL) {
        ptr = zod_yaml_internal_area_bump(area->head, size, align);
    }
    if (ptr != NULL) {
        return ptr;
    }
    if (size > SIZE_MAX - align - sizeof(zod_yaml_internal_block)) {
        return NULL;
    }
    size_t need = size + align;
    size_t cap  = need > ZOD_YAML_AREA_BLOCK_CAP ? need : ZOD_YAML_AREA_BLOCK_CAP;

    zod_yaml_internal_block *block = malloc(sizeof(*block) + cap);
    if (block == NULL) {
        return NULL;
    }

    block->used = 0;
    block->cap  = cap;

    if (need > ZOD_YAML_AREA_BLOCK_CAP && area->head != NULL) {
        block->next      = area->head->next;
        area->head->next = block;
    } else {
        block->next = area->head;
        area->head  = block;
    }

    return zod_yaml_internal_area_bump(block, size, align);
}

static void zod_yaml_internal_area_free(zod_yaml_internal_area *area) {
    zod_yaml_internal_block *block = area->head;
    while (block != NULL) {
        zod_yaml_internal_block *next = block->next;
        free(block);
        block = next;
    }
    area->head = NULL;
}

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

static inline const char *zod_yaml_internal_area_copy(zod_yaml_internal_area       *area,
                                                      zod_yaml_internal_string_view view) {
    char *copy = zod_yaml_internal_area_alloc(area, view.len + 1, 1);
    if (copy == NULL) {
        return NULL;
    }
    if (view.len > 0) {
        memcpy(copy, view.data, view.len);
    }
    copy[view.len] = '\0';
    return copy;
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
    const zod_yaml_node_entry *entries;
} zod_yaml_node_map;

struct zod_yaml_node {
    zod_yaml_kind kind;
    int           line; /* 1-based source line */
    union {
        zod_yaml_node_scalar scalar;
        zod_yaml_node_list   list;
        zod_yaml_node_map    map;
    } as;
};

struct zod_yaml_doc {
    zod_yaml_node          root;
    zod_yaml_internal_area area;
};

static const zod_yaml_node zod_yaml_none = {
    .kind      = ZOD_YAML_NONE,
    .line      = 0,
    .as.scalar = { .text = NULL, .style = ZOD_YAML_PLAIN },
};

static void zod_yaml_set_error(zod_yaml_error *err, zod_yaml_status status, int line, int column,
                               const char *message) {
    if (err == NULL) {
        return;
    }

    err->status = status;
    err->line   = line;
    err->column = column;
    snprintf(err->message, ZOD_YAML_ERR_MSG_CAP, "%s", message);
}

typedef struct zod_yaml_internal_line {
    int                           indent;
    zod_yaml_internal_string_view text;
    int                           number;
} zod_yaml_internal_line;

typedef struct zod_yaml_internal_parser {
    const char     *text;
    size_t          len;
    size_t          pos;
    int             line_number;
    zod_yaml_error *err;
} zod_yaml_internal_parser;

typedef enum zod_yaml_internal_next {
    ZOD_YAML_NEXT_LINE = 0,
    ZOD_YAML_NEXT_END,
    ZOD_YAML_NEXT_ERROR
} zod_yaml_internal_next;

static void zod_yaml_internal_parser_init(zod_yaml_internal_parser *parser, const char *text,
                                          size_t len, zod_yaml_error *err) {
    parser->text        = text;
    parser->len         = len;
    parser->pos         = 0;
    parser->line_number = 0;
    parser->err         = err;
}

static size_t zod_yaml_internal_strip_len(const char *data, size_t len) {
    int in_quotes = 0;

    for (size_t i = 0; i < len; i++) {
        char ch = data[i];

        if (in_quotes && ch == '\\') {
            i++;
            continue;
        }

        if (ch == '"') {
            in_quotes = !in_quotes;
            continue;
        }

        int starts_comment = ch == '#' && !in_quotes && (i == 0 || data[i - 1] == ' ');
        if (starts_comment) {
            len = i;
            break;
        }
    }

    while (len > 0 && (data[len - 1] == ' ' || data[len - 1] == '\r')) {
        len--;
    }
    return len;
}

static int zod_yaml_internal_doc_marker(zod_yaml_internal_string_view text, int indent) {
    if (indent != 0 || text.len < ZOD_YAML_DOC_MARKER_LEN ||
        memcmp(text.data, ZOD_YAML_DOC_MARKER, ZOD_YAML_DOC_MARKER_LEN) != 0) {
        return 0;
    }
    return text.len == ZOD_YAML_DOC_MARKER_LEN || text.data[ZOD_YAML_DOC_MARKER_LEN] == ' ';
}

static zod_yaml_internal_next zod_yaml_internal_next_line(zod_yaml_internal_parser *parser,
                                                          zod_yaml_internal_line   *line) {
    while (parser->pos < parser->len) {
        const char *start = parser->text + parser->pos;
        size_t      avail = parser->len - parser->pos;
        const char *eol   = memchr(start, '\n', avail);
        size_t      span  = eol != NULL ? (size_t)(eol - start) : avail;

        parser->pos += eol != NULL ? span + 1 : span;
        parser->line_number++;

        size_t indent = 0;
        while (indent < span && start[indent] == ' ') {
            indent++;
        }

        if (indent < span && start[indent] == '\t') {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, parser->line_number,
                               (int)indent + 1, "tab in indentation");
            return ZOD_YAML_NEXT_ERROR;
        }

        size_t text_len = zod_yaml_internal_strip_len(start + indent, span - indent);
        if (text_len == 0) {
            continue;
        }

        line->indent = (int)indent;
        line->text   = zod_yaml_internal_sv(start + indent, text_len);
        line->number = parser->line_number;

        if (zod_yaml_internal_doc_marker(line->text, line->indent)) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, parser->line_number, 1,
                               "document markers are not supported");
            return ZOD_YAML_NEXT_ERROR;
        }
        return ZOD_YAML_NEXT_LINE;
    }
    return ZOD_YAML_NEXT_END;
}

static int zod_yaml_internal_split(zod_yaml_internal_string_view  text,
                                   zod_yaml_internal_string_view *key,
                                   zod_yaml_internal_string_view *value) {
    for (size_t i = 0; i < text.len; i++) {
        int ends_key =
            text.data[i] == ZOD_YAML_KEY_SEP && (i + 1 == text.len || text.data[i + 1] == ' ');
        if (!ends_key) {
            continue;
        }

        size_t key_len = i;
        while (key_len > 0 && text.data[key_len - 1] == ' ') {
            key_len--;
        }

        size_t value_pos = i + 1;
        while (value_pos < text.len && text.data[value_pos] == ' ') {
            value_pos++;
        }

        *key   = zod_yaml_internal_sv(text.data, key_len);
        *value = zod_yaml_internal_sv(text.data + value_pos, text.len - value_pos);
        return key_len > 0;
    }
    return 0;
}

static zod_yaml_scalar_style zod_yaml_internal_unquote(zod_yaml_internal_string_view *value) {
    int quoted = value->len >= 2 && value->data[0] == '"' && value->data[value->len - 1] == '"';
    if (!quoted) {
        return ZOD_YAML_PLAIN;
    }

    *value = zod_yaml_internal_sv(value->data + 1, value->len - 2);
    return ZOD_YAML_QUOTED;
}

static zod_yaml_internal_next zod_yaml_internal_peek(const zod_yaml_internal_parser *parser,
                                                     zod_yaml_internal_line         *line) {
    zod_yaml_internal_parser ahead = *parser;

    return zod_yaml_internal_next_line(&ahead, line);
}

static size_t zod_yaml_internal_count(zod_yaml_internal_parser parser, int indent,
                                      zod_yaml_status *status) {
    zod_yaml_internal_line line;
    zod_yaml_internal_next next;
    size_t                 count = 0;

    while ((next = zod_yaml_internal_next_line(&parser, &line)) == ZOD_YAML_NEXT_LINE &&
           line.indent >= indent) {
        count += line.indent == indent;
    }

    *status = next == ZOD_YAML_NEXT_ERROR ? ZOD_YAML_ERR_SYNTAX : ZOD_YAML_OK;
    return count;
}

static int zod_yaml_internal_has_key(const zod_yaml_node_entry *entries, size_t count,
                                     const char *key) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].key, key) == 0) {
            return 1;
        }
    }
    return 0;
}

static zod_yaml_status zod_yaml_internal_parse_map(zod_yaml_internal_parser *parser,
                                                   zod_yaml_internal_area *area, int indent,
                                                   int depth, zod_yaml_node *out);

static zod_yaml_status zod_yaml_internal_value(zod_yaml_internal_parser     *parser,
                                               zod_yaml_internal_area       *area,
                                               const zod_yaml_internal_line *line,
                                               zod_yaml_internal_string_view value, int depth,
                                               zod_yaml_node *node) {
    node->line = line->number;

    if (value.len > 0) {
        zod_yaml_scalar_style style = zod_yaml_internal_unquote(&value);
        const char           *text  = zod_yaml_internal_area_copy(area, value);
        if (text == NULL) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
            return ZOD_YAML_ERR_NOMEM;
        }

        node->kind      = ZOD_YAML_SCALAR;
        node->as.scalar = (zod_yaml_node_scalar){
            .text  = text,
            .style = style,
        };
        return ZOD_YAML_OK;
    }

    zod_yaml_internal_line child;
    zod_yaml_internal_next next = zod_yaml_internal_peek(parser, &child);
    if (next == ZOD_YAML_NEXT_ERROR) {
        return ZOD_YAML_ERR_SYNTAX;
    }
    if (next == ZOD_YAML_NEXT_END || child.indent <= line->indent) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_UNSUPPORTED, line->number, line->indent + 1,
                           "empty values are not supported");
        return ZOD_YAML_ERR_UNSUPPORTED;
    }

    return zod_yaml_internal_parse_map(parser, area, child.indent, depth + 1, node);
}

static zod_yaml_status zod_yaml_internal_parse_map(zod_yaml_internal_parser *parser,
                                                   zod_yaml_internal_area *area, int indent,
                                                   int depth, zod_yaml_node *out) {
    zod_yaml_internal_line line;
    zod_yaml_status        status;

    if (depth > ZOD_YAML_MAX_DEPTH) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_DEPTH, parser->line_number, 0,
                           "nesting too deep");
        return ZOD_YAML_ERR_DEPTH;
    }

    size_t count = zod_yaml_internal_count(*parser, indent, &status);
    if (status != ZOD_YAML_OK) {
        return status;
    }
    if (count > ZOD_YAML_MAX_ENTRIES) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_TOO_LARGE, parser->line_number, 0,
                           "too many entries in map");
        return ZOD_YAML_ERR_TOO_LARGE;
    }

    zod_yaml_node_entry *entries =
        zod_yaml_internal_area_alloc(area, count * sizeof(*entries), sizeof(void *));
    zod_yaml_node *nodes =
        zod_yaml_internal_area_alloc(area, count * sizeof(*nodes), sizeof(void *));
    if (entries == NULL || nodes == NULL) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
        return ZOD_YAML_ERR_NOMEM;
    }

    for (size_t i = 0; i < count; i++) {
        zod_yaml_internal_string_view key, value;

        if (zod_yaml_internal_next_line(parser, &line) != ZOD_YAML_NEXT_LINE) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, parser->line_number, 0,
                               "unexpected end of input");
            return ZOD_YAML_ERR_SYNTAX;
        }
        if (line.indent != indent) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line.number, line.indent + 1,
                               ZOD_YAML_MSG_INDENT);
            return ZOD_YAML_ERR_SYNTAX;
        }
        if (!zod_yaml_internal_split(line.text, &key, &value)) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line.number, line.indent + 1,
                               "expected 'key: value'");
            return ZOD_YAML_ERR_SYNTAX;
        }

        const char *key_text = zod_yaml_internal_area_copy(area, key);
        if (key_text == NULL) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
            return ZOD_YAML_ERR_NOMEM;
        }
        if (zod_yaml_internal_has_key(entries, i, key_text)) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_DUPLICATE_KEY, line.number,
                               line.indent + 1, "duplicate key");
            return ZOD_YAML_ERR_DUPLICATE_KEY;
        }

        zod_yaml_node_entry entry = {
            .key   = key_text,
            .value = &nodes[i],
        };
        memcpy(&entries[i], &entry, sizeof(entry));

        status = zod_yaml_internal_value(parser, area, &line, value, depth, &nodes[i]);
        if (status != ZOD_YAML_OK) {
            return status;
        }
    }

    out->kind           = ZOD_YAML_MAP;
    out->line           = nodes[0].line;
    out->as.map.count   = count;
    out->as.map.entries = entries;
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_finish(zod_yaml_internal_parser *parser) {
    zod_yaml_internal_line line;
    zod_yaml_internal_next next = zod_yaml_internal_next_line(parser, &line);

    if (next == ZOD_YAML_NEXT_END) {
        return ZOD_YAML_OK;
    }
    if (next == ZOD_YAML_NEXT_LINE) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line.number, line.indent + 1,
                           ZOD_YAML_MSG_INDENT);
    }
    return ZOD_YAML_ERR_SYNTAX;
}

zod_yaml_status zod_yaml_parse(const char *text, size_t len, zod_yaml_doc **doc,
                               zod_yaml_error *err) {
    *doc = NULL;

    if (len > ZOD_YAML_MAX_INPUT_BYTES) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_TOO_LARGE, 0, 0, "input exceeds 5 MB");
        return ZOD_YAML_ERR_TOO_LARGE;
    }

    if (text == NULL && len != 0) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_SYNTAX, 0, 0, "text is NULL but len is not 0");
        return ZOD_YAML_ERR_SYNTAX;
    }

    zod_yaml_internal_parser parser;
    zod_yaml_internal_line   first;

    zod_yaml_internal_parser_init(&parser, text, len, err);

    zod_yaml_internal_next first_next = zod_yaml_internal_peek(&parser, &first);
    if (first_next == ZOD_YAML_NEXT_ERROR) {
        return ZOD_YAML_ERR_SYNTAX;
    }

    zod_yaml_doc *new_doc = calloc(1, sizeof(*new_doc));
    if (new_doc == NULL) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
        return ZOD_YAML_ERR_NOMEM;
    }

    new_doc->root.kind     = ZOD_YAML_NONE;
    zod_yaml_status status = ZOD_YAML_OK;
    if (first_next == ZOD_YAML_NEXT_LINE) {
        status =
            zod_yaml_internal_parse_map(&parser, &new_doc->area, first.indent, 1, &new_doc->root);
        if (status == ZOD_YAML_OK) {
            status = zod_yaml_internal_finish(&parser);
        }
    }
    if (status != ZOD_YAML_OK) {
        zod_yaml_free(new_doc);
        return status;
    }

    *doc = new_doc;
    return ZOD_YAML_OK;
}

zod_yaml_status zod_yaml_load(const char *path, zod_yaml_doc **doc, zod_yaml_error *err) {
    (void)path;

    *doc = NULL;
    zod_yaml_set_error(err, ZOD_YAML_ERR_UNSUPPORTED, 0, 0, "not implemented");
    return ZOD_YAML_ERR_UNSUPPORTED;
}

void zod_yaml_free(zod_yaml_doc *doc) {
    if (doc == NULL) {
        return;
    }
    zod_yaml_internal_area_free(&doc->area);
    free(doc);
}

const zod_yaml_node *zod_yaml_root(const zod_yaml_doc *doc) {
    return doc == NULL ? &zod_yaml_none : &doc->root;
}

zod_yaml_kind zod_yaml_kind_of(const zod_yaml_node *node) { return node->kind; }

const char *zod_yaml_str(const zod_yaml_node *node) {
    return node->kind == ZOD_YAML_SCALAR ? node->as.scalar.text : NULL;
}

static const zod_yaml_node *zod_yaml_internal_find(const zod_yaml_node *map, const char *key) {
    if (map->kind != ZOD_YAML_MAP) {
        return NULL;
    }

    for (size_t i = 0; i < map->as.map.count; i++) {
        if (strcmp(map->as.map.entries[i].key, key) == 0) {
            return map->as.map.entries[i].value;
        }
    }
    return NULL;
}

const zod_yaml_node *zod_yaml_get(const zod_yaml_node *map, const char *key) {
    const zod_yaml_node *found = zod_yaml_internal_find(map, key);

    return found != NULL ? found : &zod_yaml_none;
}

zod_yaml_presence zod_yaml_has(const zod_yaml_node *map, const char *key) {
    return zod_yaml_internal_find(map, key) != NULL ? ZOD_YAML_FOUND : ZOD_YAML_MISSING;
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
    if (node->kind == ZOD_YAML_MAP) {
        return node->as.map.count;
    }
    return node->kind == ZOD_YAML_LIST ? node->as.list.count : 0;
}

const zod_yaml_node *zod_yaml_at(const zod_yaml_node *list, size_t index) {
    if (list->kind != ZOD_YAML_LIST || index >= list->as.list.count) {
        return &zod_yaml_none;
    }
    return list->as.list.items[index];
}

const char *zod_yaml_key_at(const zod_yaml_node *map, size_t index) {
    if (map->kind != ZOD_YAML_MAP || index >= map->as.map.count) {
        return NULL;
    }
    return map->as.map.entries[index].key;
}

const zod_yaml_node *zod_yaml_val_at(const zod_yaml_node *map, size_t index) {
    if (map->kind != ZOD_YAML_MAP || index >= map->as.map.count) {
        return &zod_yaml_none;
    }
    return map->as.map.entries[index].value;
}

#endif

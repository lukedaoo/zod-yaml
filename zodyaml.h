#ifndef ZOD_YAML_H
#define ZOD_YAML_H

// Author: LD - Nov, 2026
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
 *   # comments, full line or after a value (a # needs a space before it)
 *   version: v1                      # plain scalar
 *   title: "Quoted: text, # kept"    # double-quoted, escapes \" and \\
 *   "201": ok                        # quoted key, stored without the quotes
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
 *   tags: [                          # flow collections may span lines
 *     No-Auth,                       # comments and blank lines inside are skipped
 *     Auth,                          # a trailing comma is allowed
 *   ]
 *   reaches:
 *     [Auth, Users]                  # a flow value may start on the next line
 *
 * Rejected with ZOD_YAML_ERR_UNSUPPORTED, with line and column:
 *
 *   script: |                        # block scalars ( | and > )
 *   base: &anchor                    # anchors, aliases, merge keys
 *   value: !!str 5                   # tags
 *   key:                             # empty value (null) with nothing nested
 *   key:
 *   - item                           # list at the same indent as its key
 *   - - nested                       # nested block lists
 *   [a, b]                           # flow collection as the whole document
 *   tags: [a: b]                     # single-pair map inside a flow list
 *   text: "tab\n"                    # escapes other than \" and \\
 *
 * Rejected with ZOD_YAML_ERR_SYNTAX, with line and column:
 *
 *   <tab>key: value                  # tab indentation
 *   ---                              # multiple documents
 *   title: "a" b                     # text after a closing quote
 *   tags: [a, b                      # unterminated bracket
 *   tags: "x                         # unterminated quote
 *   tags: [a,, b]                    # empty flow item
 *
 * Other errors: DUPLICATE_KEY (same key twice in one map), DEPTH (nesting over
 * ZOD_YAML_MAX_DEPTH, default 64), TOO_LARGE (input over ZOD_YAML_MAX_INPUT_BYTES,
 * default 5 MB, or over ZOD_YAML_MAX_ENTRIES, default 1024, in one block or
 * flow collection), IO (zod_yaml_load cannot read the file), NOMEM.
 * Each limit can be overridden with #define before including the header.
 *
 * Not special: single quotes. 'x' is plain text and keeps its quotes.
 *
 * Skipping keys: zod_yaml_parse_opts() and zod_yaml_load_opts() take a list of
 * key names. In block maps, at any depth, those keys are dropped: the value is
 * not stored and not validated, so prose, block scalars (| and >), anchors and
 * tags under them are fine. Deeper-indented lines and unclosed flow collections
 * belong to the skipped value. Notes:
 *   - Flow maps ({ description: x }) are skipped too, but the skipped value is
 *     still parsed, so { description: *x } is an error.
 *   - Skipped entries still count toward ZOD_YAML_MAX_ENTRIES.
 *   - Duplicate skipped keys are not reported.
 *   - Content under a skipped key must still be tab-free and must not contain a
 *     line that is just ---.
 *
 * zod_yaml_str() on a map or list returns NULL and prints a warning to stderr.
 * A missing key (none) stays silent. Override ZOD_YAML_WARN to redirect it.
 *
 * Scalars are text. Callers convert. Maps keep insertion order.
 * Everything returned points into the document and lives until zod_yaml_free().
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZOD_YAML_ERR_MSG_CAP 128

#ifndef ZOD_YAML_MAX_INPUT_BYTES
    #define ZOD_YAML_MAX_INPUT_BYTES ((size_t)5 * 1024 * 1024)
#endif

#ifndef ZOD_YAML_MAX_ENTRIES
    #define ZOD_YAML_MAX_ENTRIES 1024
#endif

#ifndef ZOD_YAML_MAX_DEPTH
    #define ZOD_YAML_MAX_DEPTH 64
#endif

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

typedef struct zod_yaml_options {
    const char *const *skip_keys;
    size_t             skip_count;
} zod_yaml_options;

zod_yaml_status zod_yaml_parse(const char *text, size_t len, zod_yaml_doc **doc,
                               zod_yaml_error *err);
zod_yaml_status zod_yaml_parse_opts(const char *text, size_t len, zod_yaml_doc **doc,
                                    zod_yaml_error *err, const zod_yaml_options *options);
zod_yaml_status zod_yaml_load(const char *path, zod_yaml_doc **doc, zod_yaml_error *err);
zod_yaml_status zod_yaml_load_opts(const char *path, zod_yaml_doc **doc, zod_yaml_error *err,
                                   const zod_yaml_options *options);
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

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZOD_YAML_WARN
    #define ZOD_YAML_WARN(...) fprintf(stderr, __VA_ARGS__)
#endif

#define ZOD_YAML_AREA_BLOCK_CAP 4096
#define ZOD_YAML_READ_CHUNK     4096
#define ZOD_YAML_KEY_SEP        ':'
#define ZOD_YAML_ITEM_MARK      '-'
#define ZOD_YAML_QUOTE          '"'
#define ZOD_YAML_ESCAPE         '\\'
#define ZOD_YAML_RESERVED_LEAD  "|>&*!"
#define ZOD_YAML_MSG_RESERVED   "block scalar, anchor, alias and tag syntax is not supported"
#define ZOD_YAML_LIST_OPEN      '['
#define ZOD_YAML_LIST_CLOSE     ']'
#define ZOD_YAML_MAP_OPEN       '{'
#define ZOD_YAML_MAP_CLOSE      '}'
#define ZOD_YAML_FLOW_SEP       ','
#define ZOD_YAML_MSG_NOMEM      "out of memory"
#define ZOD_YAML_MSG_TOO_LARGE  "input too large"
#define ZOD_YAML_MSG_INDENT     "unexpected indentation"
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

    const zod_yaml_options *options;

    zod_yaml_internal_line pending;
    int                    has_pending;
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
    parser->options     = NULL;
    parser->has_pending = 0;
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
    if (parser->has_pending) {
        *line               = parser->pending;
        parser->has_pending = 0;
        return ZOD_YAML_NEXT_LINE;
    }

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

static size_t zod_yaml_internal_quote_end(zod_yaml_internal_string_view text) {
    for (size_t i = 1; i < text.len; i++) {
        if (text.data[i] == ZOD_YAML_ESCAPE) {
            i++;
            continue;
        }
        if (text.data[i] == ZOD_YAML_QUOTE) {
            return i + 1;
        }
    }
    return 0;
}

static int zod_yaml_internal_split(zod_yaml_internal_string_view  text,
                                   zod_yaml_internal_string_view *key,
                                   zod_yaml_internal_string_view *value) {
    size_t start = 0;

    if (text.len > 0 && text.data[0] == ZOD_YAML_QUOTE) {
        start = zod_yaml_internal_quote_end(text);
        if (start == 0) {
            return 0;
        }
    }

    for (size_t i = start; i < text.len; i++) {
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

static zod_yaml_status zod_yaml_internal_text(zod_yaml_internal_parser     *parser,
                                              zod_yaml_internal_area       *area,
                                              const zod_yaml_internal_line *line,
                                              zod_yaml_internal_string_view raw, const char **out,
                                              zod_yaml_scalar_style *style) {
    int column = line->indent + 1 + (int)(raw.data - line->text.data);

    if (style != NULL) {
        *style = ZOD_YAML_PLAIN;
    }

    if (raw.len == 0 || raw.data[0] != ZOD_YAML_QUOTE) {
        *out = zod_yaml_internal_area_copy(area, raw);
        if (*out == NULL) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
            return ZOD_YAML_ERR_NOMEM;
        }
        return ZOD_YAML_OK;
    }

    char *copy = zod_yaml_internal_area_alloc(area, raw.len, 1);
    if (copy == NULL) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
        return ZOD_YAML_ERR_NOMEM;
    }

    size_t used = 0;
    for (size_t i = 1; i < raw.len; i++) {
        char ch = raw.data[i];

        if (ch == ZOD_YAML_QUOTE) {
            if (i + 1 != raw.len) {
                zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line->number,
                                   column + (int)i + 1, "text after closing quote");
                return ZOD_YAML_ERR_SYNTAX;
            }

            copy[used] = '\0';
            *out       = copy;
            if (style != NULL) {
                *style = ZOD_YAML_QUOTED;
            }
            return ZOD_YAML_OK;
        }

        if (ch == ZOD_YAML_ESCAPE) {
            i++;
            if (i >= raw.len) {
                break;
            }

            ch = raw.data[i];
            if (ch != ZOD_YAML_QUOTE && ch != ZOD_YAML_ESCAPE) {
                zod_yaml_set_error(parser->err, ZOD_YAML_ERR_UNSUPPORTED, line->number,
                                   column + (int)i - 1, "unsupported escape sequence");
                return ZOD_YAML_ERR_UNSUPPORTED;
            }
        }
        copy[used++] = ch;
    }

    zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line->number, column,
                       "unterminated quoted string");
    return ZOD_YAML_ERR_SYNTAX;
}

static zod_yaml_internal_next zod_yaml_internal_peek(const zod_yaml_internal_parser *parser,
                                                     zod_yaml_internal_line         *line) {
    zod_yaml_internal_parser ahead = *parser;

    return zod_yaml_internal_next_line(&ahead, line);
}

typedef enum zod_yaml_internal_flow_state {
    ZOD_YAML_FLOW_CLOSED = 0,
    ZOD_YAML_FLOW_OPEN,
    ZOD_YAML_FLOW_BAD_QUOTE
} zod_yaml_internal_flow_state;

static int zod_yaml_internal_is_item(zod_yaml_internal_string_view text);
static int zod_yaml_internal_item_text(zod_yaml_internal_string_view  text,
                                       zod_yaml_internal_string_view *item);
static int zod_yaml_internal_flow_start(zod_yaml_internal_string_view text);

static zod_yaml_internal_flow_state zod_yaml_internal_flow_track(zod_yaml_internal_string_view text,
                                                                 size_t *nesting) {
    for (size_t i = 0; i < text.len; i++) {
        char ch = text.data[i];

        if (ch == ZOD_YAML_QUOTE) {
            size_t end =
                zod_yaml_internal_quote_end(zod_yaml_internal_sv(text.data + i, text.len - i));
            if (end == 0) {
                *nesting = 0;
                return ZOD_YAML_FLOW_BAD_QUOTE;
            }

            i += end - 1;
            continue;
        }

        if (ch == ZOD_YAML_LIST_OPEN || ch == ZOD_YAML_MAP_OPEN) {
            (*nesting)++;
            continue;
        }

        int closes = ch == ZOD_YAML_LIST_CLOSE || ch == ZOD_YAML_MAP_CLOSE;
        if (closes && *nesting > 0 && --(*nesting) == 0) {
            return ZOD_YAML_FLOW_CLOSED;
        }
    }
    return *nesting > 0 ? ZOD_YAML_FLOW_OPEN : ZOD_YAML_FLOW_CLOSED;
}

static zod_yaml_internal_string_view zod_yaml_internal_flow_opener(
    zod_yaml_internal_string_view text) {
    zod_yaml_internal_string_view key, value;

    while (zod_yaml_internal_is_item(text)) {
        zod_yaml_internal_item_text(text, &text);
    }
    if (zod_yaml_internal_flow_start(text)) {
        return text;
    }
    if (zod_yaml_internal_split(text, &key, &value) && zod_yaml_internal_flow_start(value)) {
        return value;
    }
    return zod_yaml_internal_sv(text.data + text.len, 0);
}

static int zod_yaml_internal_entry_skipped(const zod_yaml_internal_parser *parser,
                                           zod_yaml_internal_string_view   text) {
    const zod_yaml_options       *options = parser->options;
    zod_yaml_internal_string_view key, value;

    if (options == NULL || options->skip_keys == NULL ||
        !zod_yaml_internal_split(text, &key, &value)) {
        return 0;
    }
    if (key.len >= 2 && key.data[0] == ZOD_YAML_QUOTE && key.data[key.len - 1] == ZOD_YAML_QUOTE) {
        key = zod_yaml_internal_sv(key.data + 1, key.len - 2);
    }

    for (size_t i = 0; i < options->skip_count; i++) {
        const char *name = options->skip_keys[i];

        if (strlen(name) == key.len && strncmp(name, key.data, key.len) == 0) {
            return 1;
        }
    }
    return 0;
}

static size_t zod_yaml_internal_count(zod_yaml_internal_parser parser, int indent,
                                      zod_yaml_status *status) {
    zod_yaml_internal_line line;
    zod_yaml_internal_next next;
    size_t                 count    = 0;
    size_t                 nesting  = 0;
    int                    skipping = 0;

    while ((next = zod_yaml_internal_next_line(&parser, &line)) == ZOD_YAML_NEXT_LINE) {
        if (nesting > 0) {
            zod_yaml_internal_flow_track(line.text, &nesting);
            continue;
        }
        if (line.indent < indent) {
            break;
        }

        if (line.indent == indent) {
            count++;
            skipping = zod_yaml_internal_entry_skipped(&parser, line.text);
        }
        if (line.indent > indent && skipping) {
            continue;
        }

        zod_yaml_internal_flow_track(zod_yaml_internal_flow_opener(line.text), &nesting);
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

static int zod_yaml_internal_reserved(zod_yaml_internal_string_view text) {
    return text.len > 0 && strchr(ZOD_YAML_RESERVED_LEAD, text.data[0]) != NULL;
}

static int zod_yaml_internal_is_item(zod_yaml_internal_string_view text) {
    return text.len > 0 && text.data[0] == ZOD_YAML_ITEM_MARK &&
           (text.len == 1 || text.data[1] == ' ');
}

static int zod_yaml_internal_item_text(zod_yaml_internal_string_view  text,
                                       zod_yaml_internal_string_view *item) {
    if (!zod_yaml_internal_is_item(text)) {
        return 0;
    }

    size_t pos = 1;
    while (pos < text.len && text.data[pos] == ' ') {
        pos++;
    }

    *item = zod_yaml_internal_sv(text.data + pos, text.len - pos);
    return 1;
}

static zod_yaml_status zod_yaml_internal_next_entry(zod_yaml_internal_parser *parser, int indent,
                                                    zod_yaml_internal_line *line) {
    if (zod_yaml_internal_next_line(parser, line) != ZOD_YAML_NEXT_LINE) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, parser->line_number, 0,
                           "unexpected end of input");
        return ZOD_YAML_ERR_SYNTAX;
    }
    if (line->indent != indent) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line->number, line->indent + 1,
                           ZOD_YAML_MSG_INDENT);
        return ZOD_YAML_ERR_SYNTAX;
    }
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_open(zod_yaml_internal_parser *parser, int indent,
                                              int depth, size_t *count) {
    zod_yaml_status status;

    if (depth > ZOD_YAML_MAX_DEPTH) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_DEPTH, parser->line_number, 0,
                           "nesting too deep");
        return ZOD_YAML_ERR_DEPTH;
    }

    *count = zod_yaml_internal_count(*parser, indent, &status);
    if (status != ZOD_YAML_OK) {
        return status;
    }
    if (*count > ZOD_YAML_MAX_ENTRIES) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_TOO_LARGE, parser->line_number, 0,
                           "too many entries in block");
        return ZOD_YAML_ERR_TOO_LARGE;
    }
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_parse_block(zod_yaml_internal_parser     *parser,
                                                     zod_yaml_internal_area       *area,
                                                     const zod_yaml_internal_line *first, int depth,
                                                     zod_yaml_node *out);

typedef enum zod_yaml_internal_flow_mode {
    ZOD_YAML_FLOW_VALUE = 0,
    ZOD_YAML_FLOW_KEY
} zod_yaml_internal_flow_mode;

typedef struct zod_yaml_internal_segment {
    size_t start;
    int    line;
    int    column;
} zod_yaml_internal_segment;

typedef struct zod_yaml_internal_flow {
    zod_yaml_internal_parser        *parser;
    zod_yaml_internal_area          *area;
    const zod_yaml_internal_segment *segs;
    size_t                           seg_count;
    zod_yaml_internal_string_view    raw;
    size_t                           pos;
} zod_yaml_internal_flow;

static void zod_yaml_internal_flow_where(const zod_yaml_internal_flow *flow, size_t pos, int *line,
                                         int *column) {
    size_t i = flow->seg_count - 1;

    while (i > 0 && flow->segs[i].start > pos) {
        i--;
    }

    *line   = flow->segs[i].line;
    *column = flow->segs[i].column + (int)(pos - flow->segs[i].start);
}

static int zod_yaml_internal_flow_line(const zod_yaml_internal_flow *flow, size_t pos) {
    int line, column;

    zod_yaml_internal_flow_where(flow, pos, &line, &column);
    return line;
}

static zod_yaml_status zod_yaml_internal_flow_text(const zod_yaml_internal_flow *flow,
                                                   zod_yaml_internal_string_view slice,
                                                   const char **out, zod_yaml_scalar_style *style) {
    zod_yaml_internal_line at;
    int                    column;

    zod_yaml_internal_flow_where(flow, (size_t)(slice.data - flow->raw.data), &at.number, &column);
    at.indent = column - 1;
    at.text   = slice;

    return zod_yaml_internal_text(flow->parser, flow->area, &at, slice, out, style);
}

static int zod_yaml_internal_flow_start(zod_yaml_internal_string_view text) {
    return text.len > 0 &&
           (text.data[0] == ZOD_YAML_LIST_OPEN || text.data[0] == ZOD_YAML_MAP_OPEN);
}

static void zod_yaml_internal_flow_error(const zod_yaml_internal_flow *flow, zod_yaml_status status,
                                         const char *message) {
    int line, column;

    zod_yaml_internal_flow_where(flow, flow->pos, &line, &column);
    zod_yaml_set_error(flow->parser->err, status, line, column, message);
}

static void zod_yaml_internal_flow_skip(zod_yaml_internal_flow *flow) {
    while (flow->pos < flow->raw.len && flow->raw.data[flow->pos] == ' ') {
        flow->pos++;
    }
}

static int zod_yaml_internal_flow_at(const zod_yaml_internal_flow *flow, char ch) {
    return flow->pos < flow->raw.len && flow->raw.data[flow->pos] == ch;
}

static zod_yaml_status zod_yaml_internal_flow_count(zod_yaml_internal_flow *flow, size_t *count) {
    size_t nesting   = 0;
    size_t commas    = 0;
    int    any       = 0;
    int    after_sep = 0;

    for (size_t i = flow->pos; i < flow->raw.len; i++) {
        char ch = flow->raw.data[i];

        if (ch == ZOD_YAML_QUOTE) {
            size_t end = zod_yaml_internal_quote_end(
                zod_yaml_internal_sv(flow->raw.data + i, flow->raw.len - i));
            if (end == 0) {
                flow->pos = i;
                zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX,
                                             "unterminated quoted string");
                return ZOD_YAML_ERR_SYNTAX;
            }

            i += end - 1;
            any       = 1;
            after_sep = 0;
            continue;
        }

        if (ch == ZOD_YAML_LIST_OPEN || ch == ZOD_YAML_MAP_OPEN) {
            nesting++;
            any       = 1;
            after_sep = 0;
            continue;
        }

        if (ch == ZOD_YAML_LIST_CLOSE || ch == ZOD_YAML_MAP_CLOSE) {
            if (nesting == 0) {
                *count = any ? commas + 1 - (size_t)after_sep : 0;
                return ZOD_YAML_OK;
            }
            nesting--;
            after_sep = 0;
            continue;
        }

        if (ch == ZOD_YAML_FLOW_SEP && nesting == 0) {
            commas++;
            after_sep = 1;
            continue;
        }

        if (ch != ' ') {
            any       = 1;
            after_sep = 0;
        }
    }

    flow->pos = flow->raw.len;
    zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX, "unterminated flow collection");
    return ZOD_YAML_ERR_SYNTAX;
}

static zod_yaml_status zod_yaml_internal_flow_scalar(zod_yaml_internal_flow        *flow,
                                                     zod_yaml_internal_flow_mode    mode,
                                                     zod_yaml_internal_string_view *slice) {
    const char *data = flow->raw.data;
    size_t      len  = flow->raw.len;

    zod_yaml_internal_flow_skip(flow);
    size_t start = flow->pos;

    if (zod_yaml_internal_flow_at(flow, ZOD_YAML_QUOTE)) {
        size_t end = zod_yaml_internal_quote_end(zod_yaml_internal_sv(data + start, len - start));
        if (end == 0) {
            zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX, "unterminated quoted string");
            return ZOD_YAML_ERR_SYNTAX;
        }

        flow->pos += end;
        *slice = zod_yaml_internal_sv(data + start, end);
        return ZOD_YAML_OK;
    }

    while (flow->pos < len) {
        char ch       = data[flow->pos];
        char next     = flow->pos + 1 < len ? data[flow->pos + 1] : ' ';
        int  ends_key = mode == ZOD_YAML_FLOW_KEY && ch == ZOD_YAML_KEY_SEP &&
                        (next == ' ' || next == ZOD_YAML_FLOW_SEP || next == ZOD_YAML_MAP_CLOSE);

        if (ch == ZOD_YAML_FLOW_SEP || ch == ZOD_YAML_LIST_CLOSE || ch == ZOD_YAML_MAP_CLOSE ||
            ends_key) {
            break;
        }
        flow->pos++;
    }

    size_t end = flow->pos;
    while (end > start && data[end - 1] == ' ') {
        end--;
    }

    *slice = zod_yaml_internal_sv(data + start, end - start);
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_flow_parse(zod_yaml_internal_flow *flow, int depth,
                                                    zod_yaml_node *out);

static zod_yaml_status zod_yaml_internal_flow_item(zod_yaml_internal_flow *flow, int depth,
                                                   zod_yaml_node *node) {
    zod_yaml_internal_string_view slice, key, value;
    zod_yaml_scalar_style         style;
    const char                   *text;

    zod_yaml_internal_flow_skip(flow);
    node->line = zod_yaml_internal_flow_line(flow, flow->pos);

    zod_yaml_internal_string_view rest =
        zod_yaml_internal_sv(flow->raw.data + flow->pos, flow->raw.len - flow->pos);
    if (zod_yaml_internal_flow_start(rest)) {
        return zod_yaml_internal_flow_parse(flow, depth + 1, node);
    }

    zod_yaml_status status = zod_yaml_internal_flow_scalar(flow, ZOD_YAML_FLOW_VALUE, &slice);
    if (status != ZOD_YAML_OK) {
        return status;
    }
    if (slice.len == 0) {
        zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX, "empty flow item");
        return ZOD_YAML_ERR_SYNTAX;
    }
    if (zod_yaml_internal_reserved(slice)) {
        flow->pos = (size_t)(slice.data - flow->raw.data);
        zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_UNSUPPORTED, ZOD_YAML_MSG_RESERVED);
        return ZOD_YAML_ERR_UNSUPPORTED;
    }
    if (zod_yaml_internal_split(slice, &key, &value)) {
        flow->pos = (size_t)(slice.data - flow->raw.data);
        zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_UNSUPPORTED,
                                     "single-pair maps in flow collections are not supported");
        return ZOD_YAML_ERR_UNSUPPORTED;
    }

    status = zod_yaml_internal_flow_text(flow, slice, &text, &style);
    if (status != ZOD_YAML_OK) {
        return status;
    }

    node->kind      = ZOD_YAML_SCALAR;
    node->as.scalar = (zod_yaml_node_scalar){
        .text  = text,
        .style = style,
    };
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_flow_sep(zod_yaml_internal_flow *flow) {
    zod_yaml_internal_flow_skip(flow);
    if (!zod_yaml_internal_flow_at(flow, ZOD_YAML_FLOW_SEP)) {
        zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX, "expected ',' between items");
        return ZOD_YAML_ERR_SYNTAX;
    }

    flow->pos++;
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_flow_list(zod_yaml_internal_flow *flow, int depth,
                                                   size_t count, zod_yaml_node *out) {
    const zod_yaml_node **items = NULL;
    zod_yaml_node        *nodes = NULL;

    if (count > 0) {
        items = zod_yaml_internal_area_alloc(flow->area, count * sizeof(*items), sizeof(void *));
        nodes = zod_yaml_internal_area_alloc(flow->area, count * sizeof(*nodes), sizeof(void *));
        if (items == NULL || nodes == NULL) {
            zod_yaml_set_error(flow->parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
            return ZOD_YAML_ERR_NOMEM;
        }
    }

    for (size_t i = 0; i < count; i++) {
        zod_yaml_status status = zod_yaml_internal_flow_item(flow, depth, &nodes[i]);
        if (status != ZOD_YAML_OK) {
            return status;
        }
        items[i] = &nodes[i];

        if (i + 1 < count && (status = zod_yaml_internal_flow_sep(flow)) != ZOD_YAML_OK) {
            return status;
        }
    }

    out->kind          = ZOD_YAML_LIST;
    out->as.list.count = count;
    out->as.list.items = items;
    return ZOD_YAML_OK;
}

static int zod_yaml_internal_skipped(const zod_yaml_internal_parser *parser, const char *key) {
    const zod_yaml_options *options = parser->options;

    if (options == NULL || options->skip_keys == NULL) {
        return 0;
    }

    for (size_t i = 0; i < options->skip_count; i++) {
        if (strcmp(options->skip_keys[i], key) == 0) {
            return 1;
        }
    }
    return 0;
}

static zod_yaml_status zod_yaml_internal_flow_map(zod_yaml_internal_flow *flow, int depth,
                                                  size_t count, zod_yaml_node *out) {
    zod_yaml_node_entry *entries = NULL;
    zod_yaml_node       *nodes   = NULL;
    size_t               stored  = 0;

    if (count > 0) {
        entries =
            zod_yaml_internal_area_alloc(flow->area, count * sizeof(*entries), sizeof(void *));
        nodes = zod_yaml_internal_area_alloc(flow->area, count * sizeof(*nodes), sizeof(void *));
        if (entries == NULL || nodes == NULL) {
            zod_yaml_set_error(flow->parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
            return ZOD_YAML_ERR_NOMEM;
        }
    }

    for (size_t i = 0; i < count; i++) {
        zod_yaml_internal_string_view key;
        const char                   *key_text;

        zod_yaml_status status = zod_yaml_internal_flow_scalar(flow, ZOD_YAML_FLOW_KEY, &key);
        if (status != ZOD_YAML_OK) {
            return status;
        }
        if (key.len == 0 || zod_yaml_internal_reserved(key)) {
            zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX, "invalid flow map key");
            return ZOD_YAML_ERR_SYNTAX;
        }

        status = zod_yaml_internal_flow_text(flow, key, &key_text, NULL);
        if (status != ZOD_YAML_OK) {
            return status;
        }
        if (zod_yaml_internal_has_key(entries, stored, key_text)) {
            zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_DUPLICATE_KEY, "duplicate key");
            return ZOD_YAML_ERR_DUPLICATE_KEY;
        }

        zod_yaml_internal_flow_skip(flow);
        if (!zod_yaml_internal_flow_at(flow, ZOD_YAML_KEY_SEP)) {
            zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX, "expected ':' after key");
            return ZOD_YAML_ERR_SYNTAX;
        }
        flow->pos++;

        zod_yaml_node  scratch = { 0 };
        zod_yaml_node *target  = &scratch;

        if (!zod_yaml_internal_skipped(flow->parser, key_text)) {
            zod_yaml_node_entry entry = {
                .key   = key_text,
                .value = &nodes[stored],
            };
            memcpy(&entries[stored], &entry, sizeof(entry));
            target = &nodes[stored++];
        }

        status = zod_yaml_internal_flow_item(flow, depth, target);
        if (status != ZOD_YAML_OK) {
            return status;
        }

        if (i + 1 < count && (status = zod_yaml_internal_flow_sep(flow)) != ZOD_YAML_OK) {
            return status;
        }
    }

    out->kind           = ZOD_YAML_MAP;
    out->as.map.count   = stored;
    out->as.map.entries = entries;
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_flow_parse(zod_yaml_internal_flow *flow, int depth,
                                                    zod_yaml_node *out) {
    int    is_list = flow->raw.data[flow->pos] == ZOD_YAML_LIST_OPEN;
    char   close   = is_list ? ZOD_YAML_LIST_CLOSE : ZOD_YAML_MAP_CLOSE;
    size_t count;

    if (depth > ZOD_YAML_MAX_DEPTH) {
        zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_DEPTH, "nesting too deep");
        return ZOD_YAML_ERR_DEPTH;
    }

    out->line = zod_yaml_internal_flow_line(flow, flow->pos);
    flow->pos++;

    zod_yaml_status status = zod_yaml_internal_flow_count(flow, &count);
    if (status != ZOD_YAML_OK) {
        return status;
    }
    if (count > ZOD_YAML_MAX_ENTRIES) {
        zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_TOO_LARGE, "too many entries in block");
        return ZOD_YAML_ERR_TOO_LARGE;
    }

    status = is_list ? zod_yaml_internal_flow_list(flow, depth, count, out)
                     : zod_yaml_internal_flow_map(flow, depth, count, out);
    if (status != ZOD_YAML_OK) {
        return status;
    }

    zod_yaml_internal_flow_skip(flow);
    if (count > 0 && zod_yaml_internal_flow_at(flow, ZOD_YAML_FLOW_SEP)) {
        flow->pos++;
        zod_yaml_internal_flow_skip(flow);
    }
    if (!zod_yaml_internal_flow_at(flow, close)) {
        zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX, "expected closing bracket");
        return ZOD_YAML_ERR_SYNTAX;
    }

    flow->pos++;
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_flow_gather(zod_yaml_internal_flow *flow, size_t nesting) {
    zod_yaml_internal_parser ahead = *flow->parser;
    zod_yaml_internal_line   more;
    size_t                   total = flow->raw.len;
    size_t                   lines = 1;

    do {
        zod_yaml_internal_next next = zod_yaml_internal_next_line(&ahead, &more);

        if (next == ZOD_YAML_NEXT_ERROR) {
            return ZOD_YAML_ERR_SYNTAX;
        }
        if (next == ZOD_YAML_NEXT_END) {
            flow->pos = 0;
            zod_yaml_internal_flow_error(flow, ZOD_YAML_ERR_SYNTAX, "unterminated flow collection");
            return ZOD_YAML_ERR_SYNTAX;
        }

        total += 1 + more.text.len;
        lines++;
    } while (zod_yaml_internal_flow_track(more.text, &nesting) == ZOD_YAML_FLOW_OPEN);

    char                      *joined = zod_yaml_internal_area_alloc(flow->area, total, 1);
    zod_yaml_internal_segment *segs =
        zod_yaml_internal_area_alloc(flow->area, lines * sizeof(*segs), sizeof(void *));
    if (joined == NULL || segs == NULL) {
        zod_yaml_set_error(flow->parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
        return ZOD_YAML_ERR_NOMEM;
    }

    memcpy(joined, flow->raw.data, flow->raw.len);
    segs[0]     = flow->segs[0];
    size_t used = flow->raw.len;

    for (size_t i = 1; i < lines; i++) {
        zod_yaml_internal_next_line(flow->parser, &more);

        joined[used++] = ' ';
        segs[i]        = (zod_yaml_internal_segment){
            .start  = used,
            .line   = more.number,
            .column = more.indent + 1,
        };
        memcpy(joined + used, more.text.data, more.text.len);
        used += more.text.len;
    }

    flow->raw       = zod_yaml_internal_sv(joined, used);
    flow->segs      = segs;
    flow->seg_count = lines;
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_flow_value(zod_yaml_internal_parser     *parser,
                                                    zod_yaml_internal_area       *area,
                                                    const zod_yaml_internal_line *line,
                                                    zod_yaml_internal_string_view raw, int depth,
                                                    zod_yaml_node *node) {
    zod_yaml_internal_segment first = {
        .start  = 0,
        .line   = line->number,
        .column = line->indent + 1 + (int)(raw.data - line->text.data),
    };
    zod_yaml_internal_flow flow = {
        .parser    = parser,
        .area      = area,
        .segs      = &first,
        .seg_count = 1,
        .raw       = raw,
        .pos       = 0,
    };
    size_t nesting         = 0;

    node->line             = line->number;

    zod_yaml_status status = ZOD_YAML_OK;
    if (zod_yaml_internal_flow_track(raw, &nesting) == ZOD_YAML_FLOW_OPEN) {
        status = zod_yaml_internal_flow_gather(&flow, nesting);
    }
    if (status == ZOD_YAML_OK) {
        status = zod_yaml_internal_flow_parse(&flow, depth + 1, node);
    }
    if (status != ZOD_YAML_OK) {
        return status;
    }

    zod_yaml_internal_flow_skip(&flow);
    if (flow.pos != flow.raw.len) {
        zod_yaml_internal_flow_error(&flow, ZOD_YAML_ERR_SYNTAX, "text after flow collection");
        return ZOD_YAML_ERR_SYNTAX;
    }
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_value(zod_yaml_internal_parser     *parser,
                                               zod_yaml_internal_area       *area,
                                               const zod_yaml_internal_line *line,
                                               zod_yaml_internal_string_view value, int depth,
                                               zod_yaml_node *node) {
    node->line = line->number;

    if (value.len > 0) {
        if (zod_yaml_internal_flow_start(value)) {
            return zod_yaml_internal_flow_value(parser, area, line, value, depth, node);
        }
        if (zod_yaml_internal_reserved(value)) {
            int column = line->indent + 1 + (int)(value.data - line->text.data);

            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_UNSUPPORTED, line->number, column,
                               ZOD_YAML_MSG_RESERVED);
            return ZOD_YAML_ERR_UNSUPPORTED;
        }

        zod_yaml_scalar_style style;
        const char           *text;

        zod_yaml_status status = zod_yaml_internal_text(parser, area, line, value, &text, &style);
        if (status != ZOD_YAML_OK) {
            return status;
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

    if (zod_yaml_internal_flow_start(child.text)) {
        zod_yaml_internal_next_line(parser, &child);
        return zod_yaml_internal_flow_value(parser, area, &child, child.text, depth, node);
    }

    return zod_yaml_internal_parse_block(parser, area, &child, depth + 1, node);
}

static zod_yaml_status zod_yaml_internal_skip_flow(zod_yaml_internal_parser     *parser,
                                                   const zod_yaml_internal_line *line,
                                                   zod_yaml_internal_string_view value) {
    size_t                 nesting = 0;
    zod_yaml_internal_line more;

    if (zod_yaml_internal_flow_track(value, &nesting) != ZOD_YAML_FLOW_OPEN) {
        return ZOD_YAML_OK;
    }

    do {
        zod_yaml_internal_next next = zod_yaml_internal_next_line(parser, &more);

        if (next == ZOD_YAML_NEXT_ERROR) {
            return ZOD_YAML_ERR_SYNTAX;
        }
        if (next == ZOD_YAML_NEXT_END) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line->number,
                               line->indent + 1 + (int)(value.data - line->text.data),
                               "unterminated flow collection");
            return ZOD_YAML_ERR_SYNTAX;
        }
    } while (zod_yaml_internal_flow_track(more.text, &nesting) == ZOD_YAML_FLOW_OPEN);

    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_skip_value(zod_yaml_internal_parser     *parser,
                                                    const zod_yaml_internal_line *line,
                                                    zod_yaml_internal_string_view value) {
    zod_yaml_internal_line child;
    zod_yaml_internal_next next;

    zod_yaml_status status = zod_yaml_internal_skip_flow(parser, line, value);
    if (status != ZOD_YAML_OK) {
        return status;
    }

    while ((next = zod_yaml_internal_peek(parser, &child)) == ZOD_YAML_NEXT_LINE &&
           child.indent > line->indent) {
        zod_yaml_internal_next_line(parser, &child);
    }
    return next == ZOD_YAML_NEXT_ERROR ? ZOD_YAML_ERR_SYNTAX : ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_parse_map(zod_yaml_internal_parser *parser,
                                                   zod_yaml_internal_area *area, int indent,
                                                   int depth, zod_yaml_node *out) {
    zod_yaml_internal_line line;
    size_t                 count;
    size_t                 stored     = 0;
    int                    first_line = 0;

    zod_yaml_status status            = zod_yaml_internal_open(parser, indent, depth, &count);
    if (status != ZOD_YAML_OK) {
        return status;
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

        status = zod_yaml_internal_next_entry(parser, indent, &line);
        if (status != ZOD_YAML_OK) {
            return status;
        }
        if (zod_yaml_internal_flow_start(line.text) || zod_yaml_internal_reserved(line.text)) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_UNSUPPORTED, line.number, line.indent + 1,
                               "key starts with a reserved character");
            return ZOD_YAML_ERR_UNSUPPORTED;
        }
        if (zod_yaml_internal_is_item(line.text)) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line.number, line.indent + 1,
                               "list item inside a map");
            return ZOD_YAML_ERR_SYNTAX;
        }
        if (!zod_yaml_internal_split(line.text, &key, &value)) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line.number, line.indent + 1,
                               "expected 'key: value'");
            return ZOD_YAML_ERR_SYNTAX;
        }

        const char *key_text;

        status = zod_yaml_internal_text(parser, area, &line, key, &key_text, NULL);
        if (status != ZOD_YAML_OK) {
            return status;
        }
        if (i == 0) {
            first_line = line.number;
        }
        if (zod_yaml_internal_skipped(parser, key_text)) {
            status = zod_yaml_internal_skip_value(parser, &line, value);
            if (status != ZOD_YAML_OK) {
                return status;
            }
            continue;
        }
        if (zod_yaml_internal_has_key(entries, stored, key_text)) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_DUPLICATE_KEY, line.number,
                               line.indent + 1, "duplicate key");
            return ZOD_YAML_ERR_DUPLICATE_KEY;
        }

        zod_yaml_node_entry entry = {
            .key   = key_text,
            .value = &nodes[stored],
        };
        memcpy(&entries[stored], &entry, sizeof(entry));

        status = zod_yaml_internal_value(parser, area, &line, value, depth, &nodes[stored]);
        if (status != ZOD_YAML_OK) {
            return status;
        }
        stored++;
    }

    out->kind           = ZOD_YAML_MAP;
    out->line           = stored > 0 ? nodes[0].line : first_line;
    out->as.map.count   = stored;
    out->as.map.entries = entries;
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_item(zod_yaml_internal_parser     *parser,
                                              zod_yaml_internal_area       *area,
                                              const zod_yaml_internal_line *line,
                                              zod_yaml_internal_string_view item, int depth,
                                              zod_yaml_node *node) {
    zod_yaml_internal_string_view key, value;

    if (item.len == 0) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_UNSUPPORTED, line->number, line->indent + 1,
                           "empty items are not supported");
        return ZOD_YAML_ERR_UNSUPPORTED;
    }

    if (zod_yaml_internal_is_item(item)) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_UNSUPPORTED, line->number, line->indent + 1,
                           "nested lists are not supported");
        return ZOD_YAML_ERR_UNSUPPORTED;
    }

    int is_entry = !zod_yaml_internal_flow_start(item) && !zod_yaml_internal_reserved(item) &&
                   zod_yaml_internal_split(item, &key, &value);
    if (!is_entry) {
        return zod_yaml_internal_value(parser, area, line, item, depth, node);
    }

    int content_indent = line->indent + (int)(item.data - line->text.data);

    parser->pending    = (zod_yaml_internal_line){
        .indent = content_indent,
        .text   = item,
        .number = line->number,
    };
    parser->has_pending = 1;
    return zod_yaml_internal_parse_map(parser, area, content_indent, depth + 1, node);
}

static zod_yaml_status zod_yaml_internal_parse_list(zod_yaml_internal_parser *parser,
                                                    zod_yaml_internal_area *area, int indent,
                                                    int depth, zod_yaml_node *out) {
    zod_yaml_internal_line line;
    size_t                 count;

    zod_yaml_status status = zod_yaml_internal_open(parser, indent, depth, &count);
    if (status != ZOD_YAML_OK) {
        return status;
    }

    const zod_yaml_node **items =
        zod_yaml_internal_area_alloc(area, count * sizeof(*items), sizeof(void *));
    zod_yaml_node *nodes =
        zod_yaml_internal_area_alloc(area, count * sizeof(*nodes), sizeof(void *));
    if (items == NULL || nodes == NULL) {
        zod_yaml_set_error(parser->err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
        return ZOD_YAML_ERR_NOMEM;
    }

    for (size_t i = 0; i < count; i++) {
        zod_yaml_internal_string_view item;

        status = zod_yaml_internal_next_entry(parser, indent, &line);
        if (status != ZOD_YAML_OK) {
            return status;
        }
        if (!zod_yaml_internal_item_text(line.text, &item)) {
            zod_yaml_set_error(parser->err, ZOD_YAML_ERR_SYNTAX, line.number, line.indent + 1,
                               "expected '- item'");
            return ZOD_YAML_ERR_SYNTAX;
        }

        status = zod_yaml_internal_item(parser, area, &line, item, depth, &nodes[i]);
        if (status != ZOD_YAML_OK) {
            return status;
        }
        items[i] = &nodes[i];
    }

    out->kind          = ZOD_YAML_LIST;
    out->line          = nodes[0].line;
    out->as.list.count = count;
    out->as.list.items = items;
    return ZOD_YAML_OK;
}

static zod_yaml_status zod_yaml_internal_parse_block(zod_yaml_internal_parser     *parser,
                                                     zod_yaml_internal_area       *area,
                                                     const zod_yaml_internal_line *first, int depth,
                                                     zod_yaml_node *out) {
    if (zod_yaml_internal_is_item(first->text)) {
        return zod_yaml_internal_parse_list(parser, area, first->indent, depth, out);
    }
    return zod_yaml_internal_parse_map(parser, area, first->indent, depth, out);
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
    return zod_yaml_parse_opts(text, len, doc, err, NULL);
}

zod_yaml_status zod_yaml_parse_opts(const char *text, size_t len, zod_yaml_doc **doc,
                                    zod_yaml_error *err, const zod_yaml_options *options) {
    *doc = NULL;

    if (len > ZOD_YAML_MAX_INPUT_BYTES) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_TOO_LARGE, 0, 0, ZOD_YAML_MSG_TOO_LARGE);
        return ZOD_YAML_ERR_TOO_LARGE;
    }

    if (text == NULL && len != 0) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_SYNTAX, 0, 0, "text is NULL but len is not 0");
        return ZOD_YAML_ERR_SYNTAX;
    }

    zod_yaml_internal_parser parser;
    zod_yaml_internal_line   first;

    zod_yaml_internal_parser_init(&parser, text, len, err);
    parser.options                    = options;

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
        status = zod_yaml_internal_parse_block(&parser, &new_doc->area, &first, 1, &new_doc->root);
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

static zod_yaml_status zod_yaml_internal_slurp(FILE *file, char **out, size_t *len,
                                               zod_yaml_error *err) {
    size_t cap  = ZOD_YAML_READ_CHUNK;
    size_t used = 0;
    char  *buf  = malloc(cap);

    while (buf != NULL) {
        if (used == cap) {
            cap *= 2;
            char *grown = realloc(buf, cap);
            if (grown == NULL) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = grown;
        }

        size_t got = fread(buf + used, 1, cap - used, file);
        used += got;

        if (used > ZOD_YAML_MAX_INPUT_BYTES) {
            free(buf);
            zod_yaml_set_error(err, ZOD_YAML_ERR_TOO_LARGE, 0, 0, ZOD_YAML_MSG_TOO_LARGE);
            return ZOD_YAML_ERR_TOO_LARGE;
        }
        if (got == 0) {
            break;
        }
    }

    if (buf == NULL) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_NOMEM, 0, 0, ZOD_YAML_MSG_NOMEM);
        return ZOD_YAML_ERR_NOMEM;
    }
    if (ferror(file)) {
        char message[ZOD_YAML_ERR_MSG_CAP];

        free(buf);
        snprintf(message, sizeof(message), "read failed: %s", strerror(errno));
        zod_yaml_set_error(err, ZOD_YAML_ERR_IO, 0, 0, message);
        return ZOD_YAML_ERR_IO;
    }

    *out = buf;
    *len = used;
    return ZOD_YAML_OK;
}

zod_yaml_status zod_yaml_load(const char *path, zod_yaml_doc **doc, zod_yaml_error *err) {
    return zod_yaml_load_opts(path, doc, err, NULL);
}

zod_yaml_status zod_yaml_load_opts(const char *path, zod_yaml_doc **doc, zod_yaml_error *err,
                                   const zod_yaml_options *options) {
    *doc = NULL;

    if (path == NULL) {
        zod_yaml_set_error(err, ZOD_YAML_ERR_IO, 0, 0, "path is NULL");
        return ZOD_YAML_ERR_IO;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        char message[ZOD_YAML_ERR_MSG_CAP];

        snprintf(message, sizeof(message), "cannot open %s: %s", path, strerror(errno));
        zod_yaml_set_error(err, ZOD_YAML_ERR_IO, 0, 0, message);
        return ZOD_YAML_ERR_IO;
    }

    char  *text            = NULL;
    size_t len             = 0;

    zod_yaml_status status = zod_yaml_internal_slurp(file, &text, &len, err);
    fclose(file);
    if (status != ZOD_YAML_OK) {
        return status;
    }

    status = zod_yaml_parse_opts(text, len, doc, err, options);
    free(text);
    return status;
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
    if (node->kind == ZOD_YAML_MAP || node->kind == ZOD_YAML_LIST) {
        ZOD_YAML_WARN("zod-yaml: zod_yaml_str needs a scalar, node is a %s\n",
                      node->kind == ZOD_YAML_MAP ? "map" : "list");
    }
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

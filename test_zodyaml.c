#include <stdio.h>
#include <string.h>

#define KEY_LINE_CAP   24
#define DEPTH_TEXT_CAP 8192

#define ZOD_YAML_IMPLEMENTATION
#include "zodyaml.h"

static int checks_run;
static int checks_failed;

#define CHECK(condition)                                                \
    do {                                                                \
        checks_run++;                                                   \
        if (!(condition)) {                                             \
            checks_failed++;                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        }                                                               \
    } while (0)

static void test_parse_flat_map(void) {
    const char    *text = "a: 1\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    zod_yaml_status status = zod_yaml_parse(text, strlen(text), &doc, &err);

    CHECK(status == ZOD_YAML_OK);
    CHECK(doc != NULL);

    const zod_yaml_node *root = zod_yaml_root(doc);
    CHECK(zod_yaml_kind_of(root) == ZOD_YAML_MAP);
    CHECK(zod_yaml_len(root) == 1);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "a")), "1") == 0);
    CHECK(zod_yaml_has(root, "a") == ZOD_YAML_FOUND);
    CHECK(zod_yaml_has(root, "b") == ZOD_YAML_MISSING);
    CHECK(zod_yaml_kind_of(zod_yaml_get(root, "b")) == ZOD_YAML_NONE);
    CHECK(strcmp(zod_yaml_key_at(root, 0), "a") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_val_at(root, 0)), "1") == 0);
    zod_yaml_free(doc);
}

static void test_parse_flat_map_many(void) {
    const char    *text = "name: zod\n# c\n\nver:   2 # x\nq: \"a: b\"\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    CHECK(zod_yaml_len(root) == 3);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "name")), "zod") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "ver")), "2") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "q")), "a: b") == 0);
    zod_yaml_free(doc);
}

static void expect_parse_error(const char *text, zod_yaml_status want, int line) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == want);
    CHECK(doc == NULL);
    CHECK(err.status == want);
    CHECK(err.line == line);
}

static void test_parse_errors(void) {
    expect_parse_error("a: 1\nb\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a: 1\na: 2\n", ZOD_YAML_ERR_DUPLICATE_KEY, 2);
    expect_parse_error("a: 1\n  b: 2\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a:\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a:\nb: 1\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: 1\n\tb: 2\n", ZOD_YAML_ERR_SYNTAX, 2);
}

static void test_parse_nested_map(void) {
    const char    *text = "a:\n  b: 1\n  c:\n    d: 2\ne: 3\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    CHECK(zod_yaml_len(root) == 2);
    CHECK(zod_yaml_kind_of(zod_yaml_get(root, "a")) == ZOD_YAML_MAP);
    CHECK(zod_yaml_len(zod_yaml_get(root, "a")) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "a", "b", NULL)), "1") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "a", "c", "d", NULL)), "2") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "e")), "3") == 0);
    CHECK(zod_yaml_kind_of(zod_yaml_path(root, "a", "x", NULL)) == ZOD_YAML_NONE);
    zod_yaml_free(doc);
}

static void test_parse_sibling_maps(void) {
    const char    *text = "a:\n  x: 1\nb:\n  y: 2\n  z: 3\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    CHECK(zod_yaml_len(zod_yaml_get(root, "a")) == 1);
    CHECK(zod_yaml_len(zod_yaml_get(root, "b")) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "b", "z", NULL)), "3") == 0);
    zod_yaml_free(doc);
}

static void test_parse_nested_errors(void) {
    expect_parse_error("a:\n    b: 1\n  c: 2\n", ZOD_YAML_ERR_SYNTAX, 3);
    expect_parse_error("a:\n  b: 1\n   c: 2\n", ZOD_YAML_ERR_SYNTAX, 3);
    expect_parse_error("  a: 1\nb: 2\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a:\n  b: 1\n  b: 2\n", ZOD_YAML_ERR_DUPLICATE_KEY, 3);
    expect_parse_error("a:\n  b:\nc: 1\n", ZOD_YAML_ERR_UNSUPPORTED, 2);
    expect_parse_error("a:\n  b: 1\n\tc: 2\n", ZOD_YAML_ERR_SYNTAX, 3);
}

static void expect_depth(int levels, zod_yaml_status want) {
    char   text[DEPTH_TEXT_CAP];
    size_t len = 0;

    for (int i = 0; i < levels; i++) {
        len += (size_t)snprintf(text + len, sizeof(text) - len, "%*sk:\n", i, "");
    }
    len += (size_t)snprintf(text + len, sizeof(text) - len, "%*sv: 1\n", levels, "");

    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, len, &doc, &err) == want);
    CHECK((doc != NULL) == (want == ZOD_YAML_OK));
    zod_yaml_free(doc);
}

static void test_parse_depth_limit(void) {
    expect_depth(ZOD_YAML_MAX_DEPTH - 1, ZOD_YAML_OK);
    expect_depth(ZOD_YAML_MAX_DEPTH, ZOD_YAML_ERR_DEPTH);
}

static void expect_key_count(size_t count, zod_yaml_status want) {
    char *text = malloc(count * KEY_LINE_CAP + 1);
    CHECK(text != NULL);
    if (text == NULL) {
        return;
    }

    size_t len = 0;
    for (size_t i = 0; i < count; i++) {
        len += (size_t)snprintf(text + len, KEY_LINE_CAP, "k%zu: 1\n", i);
    }

    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, len, &doc, &err) == want);
    CHECK((doc != NULL) == (want == ZOD_YAML_OK));
    zod_yaml_free(doc);
    free(text);
}

static void test_parse_entry_cap(void) {
    expect_key_count(ZOD_YAML_MAX_ENTRIES, ZOD_YAML_OK);
    expect_key_count(ZOD_YAML_MAX_ENTRIES + 1, ZOD_YAML_ERR_TOO_LARGE);
}

static void test_parse_empty(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    zod_yaml_status status = zod_yaml_parse("", 0, &doc, &err);

    CHECK(status == ZOD_YAML_OK);
    CHECK(doc != NULL);
    CHECK(zod_yaml_kind_of(zod_yaml_root(doc)) == ZOD_YAML_NONE);
    zod_yaml_free(doc);
}

static void test_free_null(void) {
    zod_yaml_free(NULL);
    CHECK(1);
}

static void test_area_align_and_copy(void) {
    zod_yaml_internal_area area = { NULL };

    void *odd                   = zod_yaml_internal_area_alloc(&area, 1, 1);
    void *node = zod_yaml_internal_area_alloc(&area, sizeof(zod_yaml_node), sizeof(void *));

    CHECK(odd != NULL);
    CHECK(node != NULL);
    CHECK(((uintptr_t)node % sizeof(void *)) == 0);

    const char *copy = zod_yaml_internal_area_copy(&area, zod_yaml_internal_sv("hello", 5));

    CHECK(copy != NULL);
    CHECK(strcmp(copy, "hello") == 0);

    void *big = zod_yaml_internal_area_alloc(&area, ZOD_YAML_AREA_BLOCK_CAP * 2, 1);

    CHECK(big != NULL);
    CHECK(strcmp(copy, "hello") == 0);

    zod_yaml_internal_area_free(&area);
    CHECK(area.head == NULL);
}

static void test_area_fills_blocks(void) {
    zod_yaml_internal_area area   = { NULL };
    const size_t           allocs = ZOD_YAML_AREA_BLOCK_CAP;

    for (size_t i = 0; i < allocs; i++) {
        CHECK(zod_yaml_internal_area_alloc(&area, 8, 8) != NULL);
    }
    CHECK(area.head != NULL && area.head->next != NULL);

    zod_yaml_internal_area_free(&area);
}

static void test_area_copy_empty(void) {
    zod_yaml_internal_area area = { NULL };

    const char *copy            = zod_yaml_internal_area_copy(&area, zod_yaml_internal_sv(NULL, 0));

    CHECK(copy != NULL);
    CHECK(copy[0] == '\0');

    zod_yaml_internal_area_free(&area);
}

static void test_parse_too_large(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    zod_yaml_status status = zod_yaml_parse("a: 1\n", ZOD_YAML_MAX_INPUT_BYTES + 1, &doc, &err);

    CHECK(status == ZOD_YAML_ERR_TOO_LARGE);
    CHECK(err.status == ZOD_YAML_ERR_TOO_LARGE);
    CHECK(doc == NULL);
}

static void test_parse_max_size_ok(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;
    char          *text = malloc(ZOD_YAML_MAX_INPUT_BYTES);

    CHECK(text != NULL);
    if (text == NULL) {
        return;
    }
    memset(text, '\n', ZOD_YAML_MAX_INPUT_BYTES);

    zod_yaml_status status = zod_yaml_parse(text, ZOD_YAML_MAX_INPUT_BYTES, &doc, &err);

    CHECK(status == ZOD_YAML_OK);
    zod_yaml_free(doc);
    free(text);
}

static void test_set_error_sets_position(void) {
    zod_yaml_error err;

    zod_yaml_set_error(&err, ZOD_YAML_ERR_SYNTAX, 7, 3, "bad");

    CHECK(err.status == ZOD_YAML_ERR_SYNTAX);
    CHECK(err.line == 7);
    CHECK(err.column == 3);
    CHECK(strcmp(err.message, "bad") == 0);
}

static zod_yaml_internal_next scan_first(const char *text, zod_yaml_internal_line *line,
                                         zod_yaml_error *err) {
    zod_yaml_internal_parser parser;

    zod_yaml_internal_parser_init(&parser, text, strlen(text), err);
    return zod_yaml_internal_next_line(&parser, line);
}

static void expect_line(const char *text, int indent, const char *content, int number) {
    zod_yaml_internal_line line = { 0 };
    zod_yaml_error         err;

    CHECK(scan_first(text, &line, &err) == ZOD_YAML_NEXT_LINE);
    CHECK(line.indent == indent);
    CHECK(zod_yaml_internal_sv_equals(line.text, content));
    CHECK(line.number == number);
}

static void expect_end(const char *text) {
    zod_yaml_internal_line line = { 0 };
    zod_yaml_error         err;

    CHECK(scan_first(text, &line, &err) == ZOD_YAML_NEXT_END);
}

static void expect_error(const char *text, int line_number) {
    zod_yaml_internal_parser parser;
    zod_yaml_internal_line   line = { 0 };
    zod_yaml_error           err;
    zod_yaml_internal_next   next;

    zod_yaml_internal_parser_init(&parser, text, strlen(text), &err);
    do {
        next = zod_yaml_internal_next_line(&parser, &line);
    } while (next == ZOD_YAML_NEXT_LINE);

    CHECK(next == ZOD_YAML_NEXT_ERROR);
    CHECK(err.line == line_number);
}

static void test_scan_empty_input(void) {
    expect_end("");
    expect_end("\n\n");
    expect_end("   \n");
}

static void test_scan_plain_line(void) {
    expect_line("a: 1", 0, "a: 1", 1);
    expect_line("a: 1\n", 0, "a: 1", 1);
}

static void test_scan_indent(void) { expect_line("  b: 2\n", 2, "b: 2", 1); }

static void test_scan_skips_blank_lines(void) { expect_line("\n\n  a: 1\n", 2, "a: 1", 3); }

static void test_scan_second_line(void) {
    zod_yaml_internal_parser parser;
    zod_yaml_internal_line   line = { 0 };
    zod_yaml_error           err;
    const char              *text = "a: 1\nb: 2\n";

    zod_yaml_internal_parser_init(&parser, text, strlen(text), &err);

    CHECK(zod_yaml_internal_next_line(&parser, &line) == ZOD_YAML_NEXT_LINE);
    CHECK(zod_yaml_internal_next_line(&parser, &line) == ZOD_YAML_NEXT_LINE);
    CHECK(zod_yaml_internal_sv_equals(line.text, "b: 2"));
    CHECK(line.number == 2);
    CHECK(zod_yaml_internal_next_line(&parser, &line) == ZOD_YAML_NEXT_END);
}

static void test_scan_comments(void) {
    expect_end("# only\n");
    expect_end("   # indented only\n");
    expect_line("a: 1 # note\n", 0, "a: 1", 1);
    expect_line("a: 1   \n", 0, "a: 1", 1);
}

static void test_scan_hash_needs_space(void) {
    expect_line("a: b#c\n", 0, "a: b#c", 1);
    expect_line("url: https://x/#frag\n", 0, "url: https://x/#frag", 1);
    expect_line("url: https://x/#frag  #comment\n", 0, "url: https://x/#frag", 1);
}

static void test_scan_hash_in_quotes(void) {
    expect_line("a: \"x # y\"\n", 0, "a: \"x # y\"", 1);
    expect_line("a: \"x # y\" # real\n", 0, "a: \"x # y\"", 1);
    expect_line("a: \"q \\\" # y\"\n", 0, "a: \"q \\\" # y\"", 1);
}

static void test_scan_rejects_tab_indent(void) {
    expect_error("\tkey: v\n", 1);
    expect_error("a: 1\n  \tb: 2\n", 2);
}

static void test_scan_rejects_doc_marker(void) {
    expect_error("---\n", 1);
    expect_error("a: 1\n---\n", 2);
}

int main(void) {
    test_set_error_sets_position();
    test_scan_empty_input();
    test_scan_plain_line();
    test_scan_indent();
    test_scan_skips_blank_lines();
    test_scan_second_line();
    test_scan_comments();
    test_scan_hash_needs_space();
    test_scan_hash_in_quotes();
    test_scan_rejects_tab_indent();
    test_scan_rejects_doc_marker();
    test_parse_too_large();
    test_parse_max_size_ok();
    test_area_align_and_copy();
    test_area_fills_blocks();
    test_area_copy_empty();
    test_parse_flat_map();
    test_parse_flat_map_many();
    test_parse_errors();
    test_parse_entry_cap();
    test_parse_nested_map();
    test_parse_sibling_maps();
    test_parse_nested_errors();
    test_parse_depth_limit();
    test_parse_empty();
    test_free_null();

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}

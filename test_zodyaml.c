#include <stdio.h>
#include <string.h>

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
    zod_yaml_free(doc);
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
    test_parse_empty();
    test_free_null();

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}

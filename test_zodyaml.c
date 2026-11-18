#include <stdio.h>
#include <string.h>

#define LOAD_TEST_PATH "build/load_test.yaml"
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
    zod_yaml_free(doc);
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

static void test_parse_list_in_map(void) {
    const char    *text = "a:\n  - x\n  - \"p: q\"   # c\n  - 3\nb: 1\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    const zod_yaml_node *list = zod_yaml_get(root, "a");

    CHECK(zod_yaml_kind_of(list) == ZOD_YAML_LIST);
    CHECK(zod_yaml_len(list) == 3);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(list, 0)), "x") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(list, 1)), "p: q") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(list, 2)), "3") == 0);
    CHECK(zod_yaml_kind_of(zod_yaml_at(list, 3)) == ZOD_YAML_NONE);
    CHECK(zod_yaml_kind_of(zod_yaml_get(list, "x")) == ZOD_YAML_NONE);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "b")), "1") == 0);
    zod_yaml_free(doc);
}

static void test_parse_root_list(void) {
    const char    *text = "- a\n- b\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    CHECK(zod_yaml_kind_of(root) == ZOD_YAML_LIST);
    CHECK(zod_yaml_len(root) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(root, 1)), "b") == 0);
    zod_yaml_free(doc);
}

static void test_parse_map_items(void) {
    const char *text =
        "tags:\n"
        "  - name: A\n"
        "    description: d1\n"
        "  - name: B\n"
        "    description: d2\n"
        "  - plain\n"
        "  -   wide: 1\n"
        "      more: 2\n"
        "  - nest:\n"
        "      deep: 3\n"
        "    top: 4\n"
        "after: 5\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    const zod_yaml_node *tags = zod_yaml_get(root, "tags");

    CHECK(zod_yaml_kind_of(tags) == ZOD_YAML_LIST);
    CHECK(zod_yaml_len(tags) == 5);
    CHECK(zod_yaml_kind_of(zod_yaml_at(tags, 0)) == ZOD_YAML_MAP);
    CHECK(zod_yaml_len(zod_yaml_at(tags, 0)) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(tags, 0), "name")), "A") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(tags, 1), "description")), "d2") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(tags, 2)), "plain") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(tags, 3), "more")), "2") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(zod_yaml_at(tags, 4), "nest", "deep", NULL)), "3") ==
          0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(tags, 4), "top")), "4") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "after")), "5") == 0);
    zod_yaml_free(doc);
}

static void test_parse_root_map_items(void) {
    const char    *text = "- id: 1\n  k: v\n- id: 2\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    CHECK(zod_yaml_kind_of(root) == ZOD_YAML_LIST);
    CHECK(zod_yaml_len(root) == 2);
    CHECK(zod_yaml_len(zod_yaml_at(root, 0)) == 2);
    CHECK(zod_yaml_len(zod_yaml_at(root, 1)) == 1);
    zod_yaml_free(doc);
}

static void test_parse_item_nested_list(void) {
    const char    *text = "- a:\n    - x\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *item = zod_yaml_at(zod_yaml_root(doc), 0);
    const zod_yaml_node *list = zod_yaml_get(item, "a");

    CHECK(zod_yaml_kind_of(item) == ZOD_YAML_MAP);
    CHECK(zod_yaml_kind_of(list) == ZOD_YAML_LIST);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(list, 0)), "x") == 0);
    zod_yaml_free(doc);
}

static void test_parse_quoted_keys(void) {
    const char *text =
        "\"201\": ok\n"
        "\"a: b\": 1\n"
        "plain: \"say \\\"hi\\\" \\\\ done\"\n"
        "empty: \"\"\n"
        "list:\n"
        "  - \"k\": \"v\"\n"
        "    j: 2\n"
        "  - \"x: y\"\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    const zod_yaml_node *list = zod_yaml_get(root, "list");

    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "201")), "ok") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "a: b")), "1") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "plain")), "say \"hi\" \\ done") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "empty")), "") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(list, 0), "k")), "v") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(list, 0), "j")), "2") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(list, 1)), "x: y") == 0);
    zod_yaml_free(doc);
}

static void test_parse_quote_errors(void) {
    expect_parse_error("k: \"a\" and \"b\"\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("k: \"abc\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("k: \"abc\\\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("k: \"a\\nb\"\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("\"a: 1\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("\"a\": 1\na: 2\n", ZOD_YAML_ERR_DUPLICATE_KEY, 2);
    expect_parse_error("x: 1\n\"b\" c: 2\n", ZOD_YAML_ERR_SYNTAX, 2);
}

static void test_quote_error_column(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse("k: \"a\" b\n", 9, &doc, &err) == ZOD_YAML_ERR_SYNTAX);
    CHECK(err.column == 7);
    zod_yaml_free(doc);
}

static void test_parse_flow_list(void) {
    const char *text =
        "a: [pAccess.Read, pMatch.Ready]\n"
        "b: [No-Auth, Match Stats, \"x, y: z\"]\n"
        "c: [https://app.example]\n"
        "d: []\n"
        "e: [ \"q\\\"r\" ]   # note\n"
        "f: [[1, 2], [3]]\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    const zod_yaml_node *a    = zod_yaml_get(root, "a");
    const zod_yaml_node *b    = zod_yaml_get(root, "b");

    CHECK(zod_yaml_kind_of(a) == ZOD_YAML_LIST);
    CHECK(zod_yaml_len(a) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(a, 0)), "pAccess.Read") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(a, 1)), "pMatch.Ready") == 0);
    CHECK(zod_yaml_len(b) == 3);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(b, 1)), "Match Stats") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(b, 2)), "x, y: z") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(zod_yaml_get(root, "c"), 0)), "https://app.example") ==
          0);
    CHECK(zod_yaml_kind_of(zod_yaml_get(root, "d")) == ZOD_YAML_LIST);
    CHECK(zod_yaml_len(zod_yaml_get(root, "d")) == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(zod_yaml_get(root, "e"), 0)), "q\"r") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(zod_yaml_at(zod_yaml_get(root, "f"), 0), 1)), "2") == 0);
    zod_yaml_free(doc);
}

static void test_parse_flow_map(void) {
    const char *text =
        "a: { key: ip, max: 10 }\n"
        "b: {}\n"
        "c: { description: Accepted. Email sent if the account exists. }\n"
        "d: { $ref: \"#/components/x\" }   # c\n"
        "e: [{ JwtAuth: [] }]\n"
        "f: { in: { deep: [1, { k: v }] } }\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);

    CHECK(zod_yaml_kind_of(zod_yaml_get(root, "a")) == ZOD_YAML_MAP);
    CHECK(zod_yaml_len(zod_yaml_get(root, "a")) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "a", "max", NULL)), "10") == 0);
    CHECK(zod_yaml_kind_of(zod_yaml_get(root, "b")) == ZOD_YAML_MAP);
    CHECK(zod_yaml_len(zod_yaml_get(root, "b")) == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "c", "description", NULL)),
                 "Accepted. Email sent if the account exists.") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "d", "$ref", NULL)), "#/components/x") == 0);

    const zod_yaml_node *e0 = zod_yaml_at(zod_yaml_get(root, "e"), 0);
    CHECK(zod_yaml_kind_of(zod_yaml_get(e0, "JwtAuth")) == ZOD_YAML_LIST);
    CHECK(zod_yaml_len(zod_yaml_get(e0, "JwtAuth")) == 0);

    const zod_yaml_node *deep = zod_yaml_path(root, "f", "in", "deep", NULL);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(zod_yaml_at(deep, 1), "k", NULL)), "v") == 0);
    zod_yaml_free(doc);
}

static void test_parse_flow_items(void) {
    const char *text =
        "list:\n"
        "  - { $ref: \"#/x\" }\n"
        "  - [a, b]\n"
        "  - { code: \"forbidden\", status: 403, message: \"Missing, or no: access\" }\n"
        "  - pRateLimit: { key: ip, window: 1h }   \n"
        "    other: [1]\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *list = zod_yaml_get(zod_yaml_root(doc), "list");

    CHECK(zod_yaml_len(list) == 4);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(list, 0), "$ref")), "#/x") == 0);
    CHECK(zod_yaml_kind_of(zod_yaml_at(list, 1)) == ZOD_YAML_LIST);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(list, 2), "message")),
                 "Missing, or no: access") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(list, 2), "status")), "403") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(zod_yaml_at(list, 3), "pRateLimit", "window", NULL)),
                 "1h") == 0);
    CHECK(zod_yaml_len(zod_yaml_get(zod_yaml_at(list, 3), "other")) == 1);
    zod_yaml_free(doc);
}

static void test_parse_flow_errors(void) {
    expect_parse_error("a: [x, y\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: [x y] z\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: [x,, y]\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: [,]\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: [x,,]\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: [x, ,]\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: {,}\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: {k: 1,,}\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: [\n  x,,\n]\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a: [x}\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: {k: 1]\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: {k}\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: {k: }\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: {k: 1, k: 2}\n", ZOD_YAML_ERR_DUPLICATE_KEY, 1);
    expect_parse_error("a: [*x]\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: [x: y]\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: [\"x]\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("b: 1\nc:\n  d: [x\n", ZOD_YAML_ERR_SYNTAX, 3);
}

static void test_parse_flow_key_lead(void) {
    expect_parse_error("{ a: 1 }\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("[a, b]\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("x:\n  *a: 1\n", ZOD_YAML_ERR_UNSUPPORTED, 2);
    expect_parse_error("x:\n  { a: 1 }: 2\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("[a]: 1\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("*a: 1\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
}

static void test_parse_flow_next_line(void) {
    const char *text =
        "reaches:\n"
        "  [Auth, Users]\n"
        "obj:\n"
        "  { k: v }\n"
        "list:\n"
        "  - a:\n"
        "      [x, y]\n"
        "    b: 1\n"
        "after: 2\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    const zod_yaml_node *item = zod_yaml_at(zod_yaml_get(root, "list"), 0);

    CHECK(zod_yaml_kind_of(zod_yaml_get(root, "reaches")) == ZOD_YAML_LIST);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(zod_yaml_get(root, "reaches"), 1)), "Users") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "obj", "k", NULL)), "v") == 0);
    CHECK(zod_yaml_len(zod_yaml_get(item, "a")) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(item, "b")), "1") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "after")), "2") == 0);
    zod_yaml_free(doc);
}

static void test_flow_next_line_errors(void) {
    expect_parse_error("a:\n[x]\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a:\n  [x\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a:\n  [x] y\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a:\n  [x]\n  [y]\n", ZOD_YAML_ERR_SYNTAX, 3);
    expect_parse_error("a:\n  [x]\n  b: 1\n", ZOD_YAML_ERR_SYNTAX, 3);
}

static void test_flow_next_line_column(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse("a:\n  [x y] z\n", 13, &doc, &err) == ZOD_YAML_ERR_SYNTAX);
    CHECK(err.column == 9);
    zod_yaml_free(doc);
}

static void test_flow_trailing_comma(void) {
    const char *text =
        "a: [x,]\n"
        "b: [x, y, ]\n"
        "c: [[1, 2,], 3,]\n"
        "d: {k: 1,}\n"
        "e: {k: 1, j: [p,], }\n"
        "f: [\n"
        "  p,\n"
        "  q,\n"
        "]\n"
        "g: {\n"
        "  k: v,\n"
        "}\n"
        "h: [\"a,\",]\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);

    CHECK(zod_yaml_len(zod_yaml_get(root, "a")) == 1);
    CHECK(zod_yaml_len(zod_yaml_get(root, "b")) == 2);
    CHECK(zod_yaml_len(zod_yaml_get(root, "c")) == 2);
    CHECK(zod_yaml_len(zod_yaml_at(zod_yaml_get(root, "c"), 0)) == 2);
    CHECK(zod_yaml_len(zod_yaml_get(root, "d")) == 1);
    CHECK(zod_yaml_len(zod_yaml_get(root, "e")) == 2);
    CHECK(zod_yaml_len(zod_yaml_path(root, "e", "j", NULL)) == 1);
    CHECK(zod_yaml_len(zod_yaml_get(root, "f")) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(zod_yaml_get(root, "f"), 1)), "q") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "g", "k", NULL)), "v") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(zod_yaml_get(root, "h"), 0)), "a,") == 0);
    zod_yaml_free(doc);
}

static void test_flow_empty_value_error(void) {
    expect_parse_error("a: {k: }\n", ZOD_YAML_ERR_SYNTAX, 1);
}

static void test_parse_multiline_flow(void) {
    const char *text =
        "a: [\n"
        "  x,\n"
        "  y\n"
        "]\n"
        "tags:\n"
        "  [\n"
        "    No-Auth,\n"
        "    Match Stats\n"
        "  ]\n"
        "limit: {\n"
        "    key: ip,\n"
        "    in: { deep: [1, 2] }\n"
        "  }\n"
        "c: [\n"
        "  p, # first\n"
        "  # standalone\n"
        "\n"
        "  q\n"
        "]\n"
        "see: a [1 foo\n"
        "outer:\n"
        "  inner: [\n"
        "  z\n"
        "  ]\n"
        "  next: 1\n"
        "last: 2\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    const zod_yaml_node *a    = zod_yaml_get(root, "a");
    const zod_yaml_node *tags = zod_yaml_get(root, "tags");
    const zod_yaml_node *c    = zod_yaml_get(root, "c");

    CHECK(zod_yaml_len(root) == 7);
    CHECK(zod_yaml_len(a) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(a, 1)), "y") == 0);
    CHECK(a->line == 1);
    CHECK(zod_yaml_at(a, 1)->line == 3);
    CHECK(zod_yaml_len(tags) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(tags, 1)), "Match Stats") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "limit", "key", NULL)), "ip") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(zod_yaml_path(root, "limit", "in", "deep", NULL), 1)),
                 "2") == 0);
    CHECK(zod_yaml_len(c) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(c, 0)), "p") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(c, 1)), "q") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "see")), "a [1 foo") == 0);
    CHECK(zod_yaml_len(zod_yaml_path(root, "outer", "inner", NULL)) == 1);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "outer", "next", NULL)), "1") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "last")), "2") == 0);
    zod_yaml_free(doc);
}

static void test_parse_multiline_items(void) {
    const char *text =
        "x-errors:\n"
        "  - {\n"
        "      code: \"forbidden\",\n"
        "      message: \"Missing, or no: access\"\n"
        "    }\n"
        "  - { code: b }\n"
        "  - key: [\n"
        "      p,\n"
        "      q\n"
        "    ]\n"
        "    other: 1\n"
        "  - [\n"
        "      m, n\n"
        "    ]\n";
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *list = zod_yaml_get(zod_yaml_root(doc), "x-errors");

    CHECK(zod_yaml_len(list) == 4);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(list, 0), "message")),
                 "Missing, or no: access") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(list, 1), "code")), "b") == 0);
    CHECK(zod_yaml_len(zod_yaml_get(zod_yaml_at(list, 2), "key")) == 2);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(list, 2), "other")), "1") == 0);
    CHECK(zod_yaml_len(zod_yaml_at(list, 3)) == 2);
    zod_yaml_free(doc);
}

static void test_multiline_flow_errors(void) {
    expect_parse_error("a: [\n  x,\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_parse_error("a: [\n  x}\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a: [\n  x\n] y\n", ZOD_YAML_ERR_SYNTAX, 3);
    expect_parse_error("a: [\n\tx\n]\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a: [\n  x,, y\n]\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a: {\n  k: 1,\n  k: 2\n}\n", ZOD_YAML_ERR_DUPLICATE_KEY, 3);
    expect_parse_error("a: [\n  *x\n]\n", ZOD_YAML_ERR_UNSUPPORTED, 2);
    expect_parse_error("a: [\n  \"x\n]\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a: [\n  x\n]\nb\n", ZOD_YAML_ERR_SYNTAX, 4);
}

static void test_multiline_error_column(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse("a: [\n  x\n] q\n", 13, &doc, &err) == ZOD_YAML_ERR_SYNTAX);
    CHECK(err.line == 3);
    CHECK(err.column == 3);
    zod_yaml_free(doc);
}

static void test_flow_error_column(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse("a: [x y] z\n", 11, &doc, &err) == ZOD_YAML_ERR_SYNTAX);
    CHECK(err.column == 10);
    zod_yaml_free(doc);
}

static void expect_flow_depth(int levels, zod_yaml_status want) {
    char   text[DEPTH_TEXT_CAP];
    size_t len = (size_t)snprintf(text, sizeof(text), "k: ");

    for (int i = 0; i < levels; i++) {
        text[len++] = '[';
    }
    for (int i = 0; i < levels; i++) {
        text[len++] = ']';
    }
    text[len++]        = '\n';

    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, len, &doc, &err) == want);
    zod_yaml_free(doc);
}

static void test_flow_depth_and_cap(void) {
    expect_flow_depth(ZOD_YAML_MAX_DEPTH - 1, ZOD_YAML_OK);
    expect_flow_depth(ZOD_YAML_MAX_DEPTH, ZOD_YAML_ERR_DEPTH);

    for (size_t items = ZOD_YAML_MAX_ENTRIES; items <= ZOD_YAML_MAX_ENTRIES + 1; items++) {
        char  *text = malloc(items * 2 + 8);
        size_t len  = 0;

        CHECK(text != NULL);
        if (text == NULL) {
            return;
        }

        len += (size_t)sprintf(text, "k: [");
        for (size_t i = 0; i < items; i++) {
            text[len++] = 'x';
            text[len++] = i + 1 < items ? ',' : ']';
        }
        text[len++]         = '\n';

        zod_yaml_doc   *doc = NULL;
        zod_yaml_error  err;
        zod_yaml_status want = items > ZOD_YAML_MAX_ENTRIES ? ZOD_YAML_ERR_TOO_LARGE : ZOD_YAML_OK;

        CHECK(zod_yaml_parse(text, len, &doc, &err) == want);
        zod_yaml_free(doc);
        free(text);
    }
}

static void test_parse_list_errors(void) {
    expect_parse_error("- a\nb: 1\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a: 1\n- b\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("- a:\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("- a: 1\n b: 2\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("- a: 1\n    b: 2\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("- a: 1\n  a: 2\n", ZOD_YAML_ERR_DUPLICATE_KEY, 2);
    expect_parse_error("- a: 1\n  - b\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("-\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("- - x\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("- a\n  - b\n", ZOD_YAML_ERR_SYNTAX, 2);
    expect_parse_error("a:\n- x\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
}

static void test_parse_reserved_lead(void) {
    expect_parse_error("a: !t [x, y]\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: &anc {k: v}\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: |\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: >\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: &x 1\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: *x\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("a: !!str 5\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_parse_error("b: 1\nc:\n  d: !x\n", ZOD_YAML_ERR_UNSUPPORTED, 3);
    expect_parse_error("- ok\n- *x\n", ZOD_YAML_ERR_UNSUPPORTED, 2);
    expect_parse_error("- &a\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
}

static void test_reserved_lead_column(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse("ab:   *x\n", 9, &doc, &err) == ZOD_YAML_ERR_UNSUPPORTED);
    CHECK(err.column == 7);
    zod_yaml_free(doc);
}

static void test_parse_reserved_inside(void) {
    const char    *text = "a: \"[x]\"\nb: x[1]\nc: b|c\nd: a&b\ne: \"{k: v}\"\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "a")), "[x]") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "b")), "x[1]") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "c")), "b|c") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "d")), "a&b") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "e")), "{k: v}") == 0);
    zod_yaml_free(doc);
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

static void write_file(const char *content, size_t len) {
    FILE *file = fopen(LOAD_TEST_PATH, "wb");

    CHECK(file != NULL);
    if (file == NULL) {
        return;
    }

    fwrite(content, 1, len, file);
    fclose(file);
}

static void test_load_file(void) {
    const char    *text = "a: 1\nlist:\n  - x\n  - { k: v }\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    write_file(text, strlen(text));

    CHECK(zod_yaml_load(LOAD_TEST_PATH, &doc, &err) == ZOD_YAML_OK);
    CHECK(doc != NULL);

    const zod_yaml_node *root = zod_yaml_root(doc);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "a")), "1") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_at(zod_yaml_get(root, "list"), 0)), "x") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(zod_yaml_get(root, "list"), 1), "k")),
                 "v") == 0);
    zod_yaml_free(doc);
    remove(LOAD_TEST_PATH);
}

static void test_load_empty_file(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    write_file("", 0);

    CHECK(zod_yaml_load(LOAD_TEST_PATH, &doc, &err) == ZOD_YAML_OK);
    CHECK(zod_yaml_kind_of(zod_yaml_root(doc)) == ZOD_YAML_NONE);
    zod_yaml_free(doc);
    remove(LOAD_TEST_PATH);
}

static void test_load_io_errors(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_load("build/does_not_exist.yaml", &doc, &err) == ZOD_YAML_ERR_IO);
    CHECK(doc == NULL);
    CHECK(err.status == ZOD_YAML_ERR_IO);
    CHECK(strstr(err.message, "does_not_exist") != NULL);

    CHECK(zod_yaml_load(NULL, &doc, &err) == ZOD_YAML_ERR_IO);
    CHECK(doc == NULL);

    CHECK(zod_yaml_load("build", &doc, &err) == ZOD_YAML_ERR_IO);
    CHECK(doc == NULL);
}

static void test_load_parse_error(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;
    const char    *text = "a: 1\nb\n";

    write_file(text, strlen(text));

    CHECK(zod_yaml_load(LOAD_TEST_PATH, &doc, &err) == ZOD_YAML_ERR_SYNTAX);
    CHECK(doc == NULL);
    CHECK(err.line == 2);
    remove(LOAD_TEST_PATH);
}

static void test_load_size_limit(void) {
    size_t sizes[]    = { ZOD_YAML_MAX_INPUT_BYTES, ZOD_YAML_MAX_INPUT_BYTES + 1 };
    int    expected[] = { ZOD_YAML_OK, ZOD_YAML_ERR_TOO_LARGE };
    char  *text       = malloc(ZOD_YAML_MAX_INPUT_BYTES + 1);

    CHECK(text != NULL);
    if (text == NULL) {
        return;
    }
    memset(text, '\n', ZOD_YAML_MAX_INPUT_BYTES + 1);

    for (size_t i = 0; i < 2; i++) {
        zod_yaml_doc  *doc = NULL;
        zod_yaml_error err;

        write_file(text, sizes[i]);

        CHECK((int)zod_yaml_load(LOAD_TEST_PATH, &doc, &err) == expected[i]);
        CHECK((doc != NULL) == (expected[i] == ZOD_YAML_OK));
        zod_yaml_free(doc);
    }

    remove(LOAD_TEST_PATH);
    free(text);
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

static void test_flow_closer_dedent(void) {
    const char *text = "outer:\n"
                       "  inner: [\n"
                       "    z,\n"
                       "]\n"
                       "last: 1\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse(text, strlen(text), &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);

    CHECK(zod_yaml_len(root) == 2);
    CHECK(zod_yaml_len(zod_yaml_path(root, "outer", "inner", NULL)) == 1);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "last")), "1") == 0);
    zod_yaml_free(doc);
}

static const char *const SKIP_KEYS[] = { "description", "summary" };

static zod_yaml_status parse_skip(const char *text, zod_yaml_doc **doc, zod_yaml_error *err) {
    zod_yaml_options options = { SKIP_KEYS, sizeof SKIP_KEYS / sizeof SKIP_KEYS[0] };

    return zod_yaml_parse_opts(text, strlen(text), doc, err, &options);
}

static void expect_skip_error(const char *text, zod_yaml_status want, int line) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(parse_skip(text, &doc, &err) == want);
    CHECK(doc == NULL);
    CHECK(err.line == line);
    zod_yaml_free(doc);
}

static void test_skip_keys(void) {
    const char *text = "id: 1\n"
                       "description: plain text\n"
                       "summary: \"quoted: text\"\n"
                       "\"description\": quoted key\n"
                       "info:\n"
                       "  description:\n"
                       "    deep: map\n"
                       "    list:\n"
                       "      - a\n"
                       "      - b\n"
                       "  name: kept\n"
                       "flow:\n"
                       "  description: [\n"
                       "    a,\n"
                       "    b,\n"
                       "  ]\n"
                       "  summary: { k: v,\n"
                       "  }\n"
                       "  keep: 1\n"
                       "items:\n"
                       "  - description: first skipped\n"
                       "    id: 1\n"
                       "  - id: 2\n"
                       "    description: last skipped\n"
                       "  - description: only\n"
                       "  - id: 3\n"
                       "end: 2\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(parse_skip(text, &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root  = zod_yaml_root(doc);
    const zod_yaml_node *items = zod_yaml_get(root, "items");

    CHECK(zod_yaml_len(root) == 5);
    CHECK(zod_yaml_has(root, "description") == ZOD_YAML_MISSING);
    CHECK(zod_yaml_has(root, "summary") == ZOD_YAML_MISSING);
    CHECK(strcmp(zod_yaml_key_at(root, 1), "info") == 0);
    CHECK(zod_yaml_len(zod_yaml_get(root, "info")) == 1);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "info", "name", NULL)), "kept") == 0);
    CHECK(zod_yaml_len(zod_yaml_get(root, "flow")) == 1);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "flow", "keep", NULL)), "1") == 0);
    CHECK(zod_yaml_len(items) == 4);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(items, 0), "id")), "1") == 0);
    CHECK(zod_yaml_len(zod_yaml_at(items, 0)) == 1);
    CHECK(zod_yaml_len(zod_yaml_at(items, 1)) == 1);
    CHECK(zod_yaml_len(zod_yaml_at(items, 2)) == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(items, 3), "id")), "3") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "end")), "2") == 0);
    zod_yaml_free(doc);
}

static void test_skip_unsupported_values(void) {
    const char *text = "a: 1\n"
                       "description: |\n"
                       "  free text\n"
                       "  see: [ not a flow\n"
                       "  more # comment\n"
                       "\n"
                       "  after blank\n"
                       "summary: >\n"
                       "  folded\n"
                       "description: *alias\n"
                       "b: 2\n"
                       "summary: &anchor value\n"
                       "summary: !!str 5\n"
                       "summary: \"unterminated\n"
                       "description:\n"
                       "c: 3\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(parse_skip(text, &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);

    CHECK(zod_yaml_len(root) == 3);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "a")), "1") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "b")), "2") == 0);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(root, "c")), "3") == 0);
    zod_yaml_free(doc);
}

static void test_skip_flow_keys(void) {
    const char *text = "a: { description: x, k: v }\n"
                       "b: { description: x }\n"
                       "c: {\n"
                       "  summary: s,\n"
                       "  k: v,\n"
                       "  description: [1, { description: n }],\n"
                       "}\n"
                       "d: [{ description: x, id: 1 }, { id: 2, summary: y }]\n"
                       "e:\n"
                       "  { description: x, k: v }\n"
                       "\"201\": { description: Created }\n"
                       "f: { k: v, description: x }\n";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(parse_skip(text, &doc, &err) == ZOD_YAML_OK);

    const zod_yaml_node *root = zod_yaml_root(doc);
    const zod_yaml_node *d    = zod_yaml_get(root, "d");

    CHECK(zod_yaml_len(zod_yaml_get(root, "a")) == 1);
    CHECK(strcmp(zod_yaml_key_at(zod_yaml_get(root, "a"), 0), "k") == 0);
    CHECK(zod_yaml_kind_of(zod_yaml_get(root, "b")) == ZOD_YAML_MAP);
    CHECK(zod_yaml_len(zod_yaml_get(root, "b")) == 0);
    CHECK(zod_yaml_len(zod_yaml_get(root, "c")) == 1);
    CHECK(strcmp(zod_yaml_str(zod_yaml_path(root, "c", "k", NULL)), "v") == 0);
    CHECK(zod_yaml_len(d) == 2);
    CHECK(zod_yaml_len(zod_yaml_at(d, 0)) == 1);
    CHECK(strcmp(zod_yaml_str(zod_yaml_get(zod_yaml_at(d, 0), "id")), "1") == 0);
    CHECK(zod_yaml_len(zod_yaml_at(d, 1)) == 1);
    CHECK(zod_yaml_len(zod_yaml_get(root, "e")) == 1);
    CHECK(zod_yaml_len(zod_yaml_get(root, "201")) == 0);
    CHECK(zod_yaml_len(zod_yaml_get(root, "f")) == 1);
    zod_yaml_free(doc);

    expect_skip_error("a: { description: *x, k: v }\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_skip_error("a: { k: 1, k: 2, description: x }\n", ZOD_YAML_ERR_DUPLICATE_KEY, 1);
    expect_skip_error("a: {\n  description: x,\n  k: 1,\n  k: 2,\n}\n", ZOD_YAML_ERR_DUPLICATE_KEY, 4);
}

static void test_skip_all_keys(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(parse_skip("description: a\nsummary: b\n", &doc, &err) == ZOD_YAML_OK);
    CHECK(zod_yaml_kind_of(zod_yaml_root(doc)) == ZOD_YAML_MAP);
    CHECK(zod_yaml_len(zod_yaml_root(doc)) == 0);
    zod_yaml_free(doc);
}

static void test_skip_off(void) {
    const char *text = "description: kept\nid: 1\n";
    zod_yaml_options none_listed = { NULL, 0 };
    zod_yaml_options empty_list  = { SKIP_KEYS, 0 };
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_parse_opts(text, strlen(text), &doc, &err, NULL) == ZOD_YAML_OK);
    CHECK(zod_yaml_len(zod_yaml_root(doc)) == 2);
    zod_yaml_free(doc);

    CHECK(zod_yaml_parse_opts(text, strlen(text), &doc, &err, &none_listed) == ZOD_YAML_OK);
    CHECK(zod_yaml_len(zod_yaml_root(doc)) == 2);
    zod_yaml_free(doc);

    CHECK(zod_yaml_parse_opts(text, strlen(text), &doc, &err, &empty_list) == ZOD_YAML_OK);
    CHECK(zod_yaml_len(zod_yaml_root(doc)) == 2);
    zod_yaml_free(doc);
}

static void test_skip_errors(void) {
    expect_skip_error("description: |\n  x\nbad line\n", ZOD_YAML_ERR_SYNTAX, 3);
    expect_skip_error("a: 1\nsummary: x\nb: [\n", ZOD_YAML_ERR_SYNTAX, 3);
    expect_skip_error("description: [\n  a,\n", ZOD_YAML_ERR_SYNTAX, 1);
    expect_skip_error("title: |\n  x\n", ZOD_YAML_ERR_UNSUPPORTED, 1);
    expect_skip_error("a: 1\na: 2\ndescription: x\n", ZOD_YAML_ERR_DUPLICATE_KEY, 2);
    expect_skip_error("description: x\n\tb: 1\n", ZOD_YAML_ERR_SYNTAX, 2);
}

static void test_skip_load(void) {
    const char *text = "description: |\n  x\nid: 1\n";
    zod_yaml_options options = { SKIP_KEYS, 2 };
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    write_file(text, strlen(text));

    CHECK(zod_yaml_load(LOAD_TEST_PATH, &doc, &err) == ZOD_YAML_ERR_UNSUPPORTED);
    CHECK(zod_yaml_load_opts(LOAD_TEST_PATH, &doc, &err, &options) == ZOD_YAML_OK);
    CHECK(zod_yaml_len(zod_yaml_root(doc)) == 1);
    zod_yaml_free(doc);
    remove(LOAD_TEST_PATH);
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
    test_parse_list_in_map();
    test_parse_root_list();
    test_parse_map_items();
    test_parse_root_map_items();
    test_parse_item_nested_list();
    test_parse_flow_list();
    test_parse_flow_map();
    test_parse_flow_items();
    test_parse_flow_errors();
    test_parse_flow_key_lead();
    test_flow_error_column();
    test_parse_multiline_flow();
    test_parse_multiline_items();
    test_multiline_flow_errors();
    test_multiline_error_column();
    test_flow_trailing_comma();
    test_flow_empty_value_error();
    test_parse_flow_next_line();
    test_flow_next_line_errors();
    test_flow_next_line_column();
    test_flow_depth_and_cap();
    test_parse_quoted_keys();
    test_parse_quote_errors();
    test_quote_error_column();
    test_parse_list_errors();
    test_parse_reserved_lead();
    test_reserved_lead_column();
    test_parse_reserved_inside();
    test_load_file();
    test_load_empty_file();
    test_load_io_errors();
    test_load_parse_error();
    test_load_size_limit();
    test_parse_empty();
    test_free_null();
    test_flow_closer_dedent();
    test_skip_keys();
    test_skip_unsupported_values();
    test_skip_flow_keys();
    test_skip_all_keys();
    test_skip_off();
    test_skip_errors();
    test_skip_load();

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}

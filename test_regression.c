#include <stdio.h>
#include <string.h>

#define FIXTURE_PATH "test.yaml"

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

static void expect_text(const zod_yaml_node *node, const char *want) {
    const char *got = zod_yaml_str(node);

    CHECK(got != NULL && strcmp(got, want) == 0);
}

static void test_fixture_file(void) {
    zod_yaml_doc  *doc = NULL;
    zod_yaml_error err;

    CHECK(zod_yaml_load(FIXTURE_PATH, &doc, &err) == ZOD_YAML_OK);
    if (doc == NULL) {
        return;
    }

    const zod_yaml_node *root = zod_yaml_root(doc);

    CHECK(zod_yaml_len(root) == 14);
    expect_text(zod_yaml_get(root, "x-version"), "v1");

    const zod_yaml_node *tags = zod_yaml_get(root, "tags");
    CHECK(zod_yaml_len(tags) == 3);
    expect_text(zod_yaml_get(zod_yaml_at(tags, 2), "description"),
                "Long text with \"inner quotes\", it's, (parens), and a hash#inside.");

    const zod_yaml_node *group_tags =
        zod_yaml_get(zod_yaml_at(zod_yaml_get(root, "x-groups"), 0), "tags");
    CHECK(zod_yaml_len(group_tags) == 3);
    expect_text(zod_yaml_at(group_tags, 1), "Beta Gamma");

    CHECK(zod_yaml_len(zod_yaml_path(root, "x-clients", "unknown", "applies_to", NULL)) == 0);
    CHECK(zod_yaml_len(zod_yaml_path(root, "x-actors", "guest", "reaches", NULL)) == 3);

    const zod_yaml_node *errors = zod_yaml_get(root, "x-errors");
    CHECK(zod_yaml_len(errors) == 3);
    expect_text(zod_yaml_get(zod_yaml_at(errors, 1), "message"), "Missing, or no access");

    const zod_yaml_node *defs = zod_yaml_get(root, "x-policy-defs");
    CHECK(zod_yaml_len(defs) == 3);
    CHECK(zod_yaml_has(defs, "pAccess.Read") == ZOD_YAML_FOUND);
    CHECK(zod_yaml_has(defs, "pLimit.Max") == ZOD_YAML_FOUND);
    CHECK(zod_yaml_has(defs, "pRate") == ZOD_YAML_FOUND);
    CHECK(zod_yaml_has(defs, "pAccess") == ZOD_YAML_MISSING);
    CHECK(strcmp(zod_yaml_key_at(defs, 0), "pAccess.Read") == 0);
    CHECK(strcmp(zod_yaml_key_at(defs, 2), "pRate") == 0);
    expect_text(zod_yaml_path(defs, "pAccess.Read", "kind", NULL), "permission");
    expect_text(zod_yaml_path(defs, "pAccess.Read", "description", NULL),
                "Level read or higher. Owner, grant, or token.");
    expect_text(zod_yaml_path(defs, "pAccess.Read", "deny", NULL), "not_found");
    expect_text(zod_yaml_at(zod_yaml_path(defs, "pLimit.Max", "constants", NULL), 1), "MAX_SIZE");
    CHECK(zod_yaml_len(zod_yaml_path(defs, "pLimit.Max", "constants", NULL)) == 2);
    expect_text(zod_yaml_path(defs, "pRate", "kind", NULL), "throttle");

    const zod_yaml_node *post   = zod_yaml_path(root, "paths", "/a/create", "post", NULL);
    const zod_yaml_node *before = zod_yaml_path(post, "x-policies", "before_load", NULL);
    CHECK(zod_yaml_len(zod_yaml_get(post, "x-behavior")) == 2);
    CHECK(zod_yaml_len(before) == 2);
    expect_text(zod_yaml_path(zod_yaml_at(before, 1), "pRate", "window", NULL), "1h");
    expect_text(zod_yaml_path(post, "responses", "201", "description", NULL), "Created");
    expect_text(zod_yaml_path(root, "paths", "/a/items/{item_id}", "get", "responses", "202",
                              "description", NULL),
                "Accepted. Sent if it exists.");

    const zod_yaml_node *schemas = zod_yaml_path(root, "components", "schemas", NULL);
    expect_text(zod_yaml_path(root, "components", "parameters", "ItemId", "schema", "type", NULL),
                "string");
    expect_text(
        zod_yaml_at(zod_yaml_path(schemas, "Item", "properties", "status", "enum", NULL), 1),
        "two");
    expect_text(zod_yaml_at(zod_yaml_path(schemas, "Request", "properties", "assignments", "items",
                                          "properties", "side", "enum", NULL),
                            2),
                "other");
    zod_yaml_free(doc);
}

int main(void) {
    test_fixture_file();

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}

#include <stdio.h>
#include <string.h>

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

int main(void) {
    test_parse_flat_map();
    test_parse_empty();
    test_free_null();

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}

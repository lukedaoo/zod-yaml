#include <stdio.h>

#include "zodyaml.h"

static void print_list(const char *label, const zod_yaml_node *list) {
    printf("%s:", label);
    for (size_t i = 0; i < zod_yaml_len(list); i++) {
        printf(" %s", zod_yaml_str(zod_yaml_at(list, i)));
    }
    printf("\n");
}

static void print_policy(const zod_yaml_node *policy) {
    if (zod_yaml_kind_of(policy) == ZOD_YAML_SCALAR) {
        printf(" %s", zod_yaml_str(policy));
        return;
    }

    printf(" %s(", zod_yaml_key_at(policy, 0));

    const zod_yaml_node *params = zod_yaml_val_at(policy, 0);
    for (size_t i = 0; i < zod_yaml_len(params); i++) {
        printf("%s%s=%s", i > 0 ? " " : "", zod_yaml_key_at(params, i),
               zod_yaml_str(zod_yaml_val_at(params, i)));
    }
    printf(")");
}

static void print_policies(const char *route, const char *stage, const zod_yaml_node *list) {
    printf("%s %s:", route, stage);
    for (size_t i = 0; i < zod_yaml_len(list); i++) {
        print_policy(zod_yaml_at(list, i));
    }
    printf("\n");
}

static void print_route_policies(const zod_yaml_node *paths) {
    static const char *const stages[] = { "before_load", "after_load" };

    for (size_t i = 0; i < zod_yaml_len(paths); i++) {
        const zod_yaml_node *methods = zod_yaml_val_at(paths, i);
        if (zod_yaml_kind_of(methods) != ZOD_YAML_MAP) {
            continue;
        }

        for (size_t j = 0; j < zod_yaml_len(methods); j++) {
            const zod_yaml_node *operation = zod_yaml_val_at(methods, j);
            if (zod_yaml_has(operation, "x-policies") == ZOD_YAML_MISSING) {
                continue;
            }

            const zod_yaml_node *policies = zod_yaml_get(operation, "x-policies");

            char route[128];
            snprintf(route, sizeof route, "%s %s", zod_yaml_key_at(methods, j),
                     zod_yaml_key_at(paths, i));

            for (size_t k = 0; k < sizeof stages / sizeof stages[0]; k++) {
                if (zod_yaml_has(policies, stages[k]) == ZOD_YAML_FOUND) {
                    print_policies(route, stages[k], zod_yaml_get(policies, stages[k]));
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    const char    *path = argc > 1 ? argv[1] : "test.yaml";
    zod_yaml_doc  *doc  = NULL;
    zod_yaml_error err;

    if (zod_yaml_load(path, &doc, &err) != ZOD_YAML_OK) {
        fprintf(stderr, "%s:%d:%d: %s\n", path, err.line, err.column, err.message);
        return 1;
    }

    const zod_yaml_node *root = zod_yaml_root(doc);

    printf("title: %s\n", zod_yaml_str(zod_yaml_path(root, "info", "title", NULL)));
    print_list("public tags", zod_yaml_path(zod_yaml_at(zod_yaml_get(root, "x-groups"), 0), "tags", NULL));

    const zod_yaml_node *defs = zod_yaml_get(root, "x-policy-defs");
    for (size_t i = 0; i < zod_yaml_len(defs); i++) {
        const zod_yaml_node *def = zod_yaml_val_at(defs, i);

        printf("policy %s: %s\n", zod_yaml_key_at(defs, i), zod_yaml_str(zod_yaml_get(def, "kind")));
    }

    const zod_yaml_node *errors = zod_yaml_get(root, "x-errors");
    for (size_t i = 0; i < zod_yaml_len(errors); i++) {
        const zod_yaml_node *error = zod_yaml_at(errors, i);

        printf("error %s %s: %s\n", zod_yaml_str(zod_yaml_get(error, "status")),
               zod_yaml_str(zod_yaml_get(error, "code")),
               zod_yaml_str(zod_yaml_get(error, "message")));
    }

    print_route_policies(zod_yaml_get(root, "paths"));

    if (zod_yaml_has(root, "missing") == ZOD_YAML_MISSING) {
        printf("no 'missing' key\n");
    }

    zod_yaml_free(doc);
    return 0;
}

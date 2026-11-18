#include <cstdio>

#include "zodyaml.hpp"

static void print_policy(const zod::yaml::Node &policy) {
    if (policy.kind() == zod::yaml::Kind::Scalar) {
        std::printf(" %s", policy.as_str());
        return;
    }

    zod::yaml::Node params = policy.value_at(0);

    std::printf(" %s(", policy.key_at(0));
    for (std::size_t i = 0; i < params.size(); i++) {
        std::printf("%s%s=%s", i > 0 ? " " : "", params.key_at(i), params.value_at(i).as_str());
    }
    std::printf(")");
}

static void print_route_policies(const zod::yaml::Node &paths) {
    static const char *const stages[] = { "before_load", "after_load" };

    for (std::size_t i = 0; i < paths.size(); i++) {
        zod::yaml::Node methods = paths.value_at(i);
        if (methods.kind() != zod::yaml::Kind::Map) {
            continue;
        }

        for (std::size_t j = 0; j < methods.size(); j++) {
            zod::yaml::Node operation = methods.value_at(j);
            if (!operation.has("x-policies")) {
                continue;
            }

            zod::yaml::Node policies = operation["x-policies"];
            for (const char *stage : stages) {
                if (!policies.has(stage)) {
                    continue;
                }

                zod::yaml::Node list = policies[stage];
                std::printf("%s %s %s:", methods.key_at(j), paths.key_at(i), stage);
                for (std::size_t k = 0; k < list.size(); k++) {
                    print_policy(list[k]);
                }
                std::printf("\n");
            }
        }
    }
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "test.yaml";

    try {
        zod::yaml::Document doc  = zod::yaml::Document::load(path);
        zod::yaml::Node     root = doc.root();

        std::printf("title: %s\n", root.path({ "info", "title" }).as_str());

        zod::yaml::Node tags = root["x-groups"][0]["tags"];
        std::printf("public tags:");
        for (std::size_t i = 0; i < tags.size(); i++) {
            std::printf(" %s", tags[i].as_str());
        }
        std::printf("\n");

        zod::yaml::Node defs = root["x-policy-defs"];
        for (std::size_t i = 0; i < defs.size(); i++) {
            const char *name = defs.key_at(i);

            std::printf("policy %s: %s\n", name, defs[name]["kind"].as_str());
        }

        zod::yaml::Node errors = root["x-errors"];
        for (std::size_t i = 0; i < errors.size(); i++) {
            std::printf("error %s %s: %s\n", errors[i]["status"].as_str(),
                        errors[i]["code"].as_str(), errors[i]["message"].as_str());
        }

        print_route_policies(root["paths"]);

        if (!root.has("missing")) {
            std::printf("no 'missing' key\n");
        }
    } catch (const zod::yaml::Error &error) {
        std::fprintf(stderr, "%s:%d:%d: %s\n", path, error.line(), error.column(), error.what());
        return 1;
    }
    return 0;
}

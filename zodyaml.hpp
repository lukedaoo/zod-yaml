#ifndef ZOD_YAML_HPP
#define ZOD_YAML_HPP

#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <type_traits>

#include "zodyaml.h"

namespace zod {
namespace yaml {

enum class Kind { None, Map, List, Scalar };

class Node {
public:
    explicit Node(const zod_yaml_node *raw) : raw_(raw) {}

    Kind kind() const { return static_cast<Kind>(zod_yaml_kind_of(raw_)); }

    const char *as_str() const {
        const char *text = zod_yaml_str(raw_);
        return text == nullptr ? "" : text;
    }

    Node operator[](const char *key) const { return Node(zod_yaml_get(raw_, key)); }

    template <typename Index,
              typename = typename std::enable_if<std::is_integral<Index>::value>::type>
    Node operator[](Index index) const {
        return at(static_cast<std::size_t>(index));
    }

    bool has(const char *key) const { return zod_yaml_has(raw_, key) == ZOD_YAML_FOUND; }

    Node path(std::initializer_list<const char *> keys) const {
        const zod_yaml_node *node = raw_;
        for (const char *key : keys) {
            node = zod_yaml_get(node, key);
        }
        return Node(node);
    }

    std::size_t size() const { return zod_yaml_len(raw_); }

    Node at(std::size_t index) const {
        check(Kind::List, "at", index);
        return Node(zod_yaml_at(raw_, index));
    }

    const char *key_at(std::size_t index) const {
        check(Kind::Map, "key_at", index);
        return zod_yaml_key_at(raw_, index);
    }

    Node value_at(std::size_t index) const {
        check(Kind::Map, "value_at", index);
        return Node(zod_yaml_val_at(raw_, index));
    }
private:
    static const char *kind_name(Kind kind) {
        switch (kind) {
            case Kind::Map:
                return "map";
            case Kind::List:
                return "list";
            case Kind::Scalar:
                return "scalar";
            case Kind::None:
                break;
        }
        return "none";
    }

    void check(Kind want, const char *call, std::size_t index) const {
        if (kind() != want) {
            std::fprintf(stderr, "zod-yaml: %s needs a %s, node is a %s\n", call, kind_name(want),
                         kind_name(kind()));
            return;
        }
        if (index >= size()) {
            std::fprintf(stderr, "zod-yaml: %s index %zu out of range, size is %zu\n", call, index,
                         size());
        }
    }

    const zod_yaml_node *raw_;
};

class Document {
public:
    static Document parse(const char *text, std::size_t len,
                          std::initializer_list<const char *> skip_keys = {}) {
        zod_yaml_doc          *raw = nullptr;
        zod_yaml_error         err = {};
        const zod_yaml_options options = { skip_keys.begin(), skip_keys.size() };
        if (zod_yaml_parse_opts(text, len, &raw, &err, &options) != ZOD_YAML_OK) {
            std::fprintf(stderr, "zod-yaml: %d:%d: %s\n", err.line, err.column, err.message);
        }
        return Document(raw, err);
    }

    static Document load(const char *path, std::initializer_list<const char *> skip_keys = {}) {
        return load(path, skip_keys.begin(), skip_keys.size());
    }

    static Document load(const char *path, const char *const *skip_keys, std::size_t skip_count) {
        zod_yaml_doc          *raw = nullptr;
        zod_yaml_error         err = {};
        const zod_yaml_options options = { skip_keys, skip_count };
        if (zod_yaml_load_opts(path, &raw, &err, &options) != ZOD_YAML_OK) {
            std::fprintf(stderr, "zod-yaml: %s:%d:%d: %s\n", path, err.line, err.column,
                         err.message);
        }
        return Document(raw, err);
    }

    Document(Document &&other) noexcept : raw_(other.raw_), err_(other.err_) {
        other.raw_ = nullptr;
    }

    Document &operator=(Document &&other) noexcept {
        if (this != &other) {
            zod_yaml_free(raw_);
            raw_       = other.raw_;
            err_       = other.err_;
            other.raw_ = nullptr;
        }
        return *this;
    }

    Document(const Document &)            = delete;
    Document &operator=(const Document &) = delete;

    ~Document() { zod_yaml_free(raw_); }

    explicit operator bool() const { return raw_ != nullptr; }

    /* status is ZOD_YAML_OK when the document loaded. */
    const zod_yaml_error &error() const { return err_; }

    Node root() const { return Node(zod_yaml_root(raw_)); }
private:
    Document(zod_yaml_doc *raw, const zod_yaml_error &err) : raw_(raw), err_(err) {}

    zod_yaml_doc  *raw_;
    zod_yaml_error err_;
};

}
}

#endif

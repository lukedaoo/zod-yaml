#ifndef ZOD_YAML_HPP
#define ZOD_YAML_HPP

#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>

#include "zodyaml.h"

namespace zod {
namespace yaml {

enum class Kind { None, Map, List, Scalar };

class Error : public std::runtime_error {
public:
    Error(const zod_yaml_error &raw)
        : std::runtime_error(raw.message),
          status_(raw.status),
          line_(raw.line),
          column_(raw.column) {}

    zod_yaml_status status() const { return status_; }
    int             line() const { return line_; }
    int             column() const { return column_; }
private:
    zod_yaml_status status_;
    int             line_;
    int             column_;
};

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
    static Document parse(const char *text, std::size_t len) {
        zod_yaml_doc  *raw = nullptr;
        zod_yaml_error err;
        if (zod_yaml_parse(text, len, &raw, &err) != ZOD_YAML_OK) {
            throw Error(err);
        }
        return Document(raw);
    }

    static Document load(const char *path) {
        zod_yaml_doc  *raw = nullptr;
        zod_yaml_error err;
        if (zod_yaml_load(path, &raw, &err) != ZOD_YAML_OK) {
            throw Error(err);
        }
        return Document(raw);
    }

    Document(Document &&other) noexcept : raw_(other.raw_) { other.raw_ = nullptr; }

    Document &operator=(Document &&other) noexcept {
        if (this != &other) {
            zod_yaml_free(raw_);
            raw_       = other.raw_;
            other.raw_ = nullptr;
        }
        return *this;
    }

    Document(const Document &)            = delete;
    Document &operator=(const Document &) = delete;

    ~Document() { zod_yaml_free(raw_); }

    Node root() const { return Node(zod_yaml_root(raw_)); }
private:
    explicit Document(zod_yaml_doc *raw) : raw_(raw) {}

    zod_yaml_doc *raw_;
};

}
}

#endif

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

struct Value;

using Array = std::vector<Value>;
using Table = std::unordered_map<std::string, Value>;

struct Value : std::variant<std::string, int64_t, double, bool, Array, Table> {
    using variant::variant;

    template<typename T>
    bool is() const {
        return std::holds_alternative<T>(*this);
    }

    template<typename T>
    T require() const {
        if (!is<T>())
            throw std::bad_variant_access{};

        return std::get<T>(*this);
    }

    template<typename T>
    T as() const {
        if constexpr (std::is_same_v<T, float>) {
            return static_cast<float>(require<double>());
        }
        else if constexpr (
            std::is_integral_v<T> &&
            !std::is_same_v<T, bool> &&
            !std::is_same_v<T, int64_t>
        ) {
            return static_cast<T>(require<int64_t>());
        }
        else {
            return require<T>();
        }
    }
};

class Document {
public:
    Document() = default;

    explicit Document(Table root)
        : root_(std::move(root)) {}

    const Value* get(std::string_view key) const {
        auto it = root_.find(std::string(key));

        if (it == root_.end())
            return nullptr;

        return &it->second;
    }

    Value* get(std::string_view key) {
        auto it = root_.find(std::string(key));

        if (it == root_.end())
            return nullptr;

        return &it->second;
    }

    const Table& root() const {
        return root_;
    }

    Table& root() {
        return root_;
    }

private:
    Table root_;
};
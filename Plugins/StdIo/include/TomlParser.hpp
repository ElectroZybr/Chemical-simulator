#pragma once

#include <filesystem>
#include <string>
#include <stdexcept>

#include <toml++/toml.hpp>

#include "Document.hpp"
#include "ParserAPI.hpp"


class TomlParser final : public ParserAPI {
public:
    std::string_view extension() const override { return ".toml"; }
    
    Document parseFile(const std::filesystem::path& path) const override {
        Document document;

        try {
            const toml::table table = toml::parse_file(path.string());

            parseTable(table, document.root());
        }
        catch (const toml::parse_error& error) {
            // TODO
        }

        return document;
    }

private:
    static void parseTable(
        const toml::table& table,
        Table& output
    ) {
        for (const auto& [key, node] : table) {
            const std::string keyName = std::string(key.str());

            if (const auto* child = node.as_table()) {
                Table childTable;
                parseTable(*child, childTable);

                output.emplace(
                    keyName,
                    std::move(childTable)
                );

                continue;
            }

            if (const auto* array = node.as_array()) {
                Array parameters;
                parameters.reserve(array->size());

                for (const auto& element : *array)
                    parameters.emplace_back(parseValue(element));

                output.emplace(
                    keyName,
                    std::move(parameters)
                );

                continue;
            }

            output.emplace(
                keyName,
                parseValue(node)
            );
        }
    }

    static Value parseValue(const toml::node& node) {
        if (const auto* value = node.as_string())
            return std::string(value->get());

        if (const auto* value = node.as_integer())
            return int64_t(value->get());

        if (const auto* value = node.as_floating_point())
            return double(value->get());

        if (const auto* value = node.as_boolean())
            return bool(value->get());

        throw std::runtime_error(
            "Unsupported TOML value"
        );
    }
};
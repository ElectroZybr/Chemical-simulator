#pragma once

#include <filesystem>
#include <string_view>

class Document;

class ParserAPI {
public:
    virtual std::string_view extension() const = 0;
    virtual Document parseFile(const std::filesystem::path& path) const = 0;
};
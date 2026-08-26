#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Lattice {

struct StartupEntry {
    std::string name;
    bool enabled = false;
    bool host = false;
};

class StartupConfig {
public:
    explicit StartupConfig(const std::filesystem::path& path) {
        parse(path);
    }

    const std::vector<StartupEntry>& entries() const noexcept {
        return entries_;
    }

    const StartupEntry* find(std::string_view name) const noexcept {
        for (const auto& entry : entries_)
            if (entry.name == name)
                return &entry;
        return nullptr;
    }

private:
    void parse(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file)
            throw std::runtime_error("Failed to open startup config: " + path.string());

        std::string line;
        bool inStartup = false;

        while (std::getline(file, line)) {
            trim(line);
            if (line.empty() || line[0] == '#')
                continue;

            if (line == "[Startup]") {
                inStartup = true;
                continue;
            }

            if (line.front() == '[') {
                inStartup = false;
                continue;
            }

            if (!inStartup)
                continue;

            const auto equal = line.find('=');
            if (equal == std::string::npos)
                continue;

            std::string name = line.substr(0, equal);
            std::string value = line.substr(equal + 1);

            trim(name);
            trim(value);

            StartupEntry entry;
            entry.name = std::move(name);

            if (value.front() == '[') {
                if (value.back() != ']')
                    throw std::runtime_error("Invalid startup value: " + value);

                value = value.substr(1, value.size() - 2);

                std::vector<std::string> values;
                size_t start = 0;

                while (start < value.size()) {
                    const size_t comma = value.find(',', start);
                    std::string item = value.substr(
                        start,
                        comma == std::string::npos ? std::string::npos : comma - start
                    );

                    trim(item);
                    unquote(item);

                    if (!item.empty())
                        values.push_back(std::move(item));

                    if (comma == std::string::npos)
                        break;

                    start = comma + 1;
                }

                for (const auto& item : values) {
                    if (item == "on")
                        entry.enabled = true;
                    else if (item == "off")
                        entry.enabled = false;
                    else if (item == "host")
                        entry.host = true;
                }
            } else {
                unquote(value);

                if (value == "on")
                    entry.enabled = true;
                else if (value == "off")
                    entry.enabled = false;
                else if (value == "host")
                    entry.host = true;
            }

            entries_.push_back(std::move(entry));
        }
    }

    static void trim(std::string& value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            value.clear();
            return;
        }

        const auto last = value.find_last_not_of(" \t\r\n");
        value = value.substr(first, last - first + 1);
    }

    static void unquote(std::string& value) {
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\'')))
            value = value.substr(1, value.size() - 2);

        trim(value);
    }

    std::vector<StartupEntry> entries_;
};

} // namespace Lattice
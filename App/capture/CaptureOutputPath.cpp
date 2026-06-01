#include "CaptureOutputPath.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <format>
#include <optional>
#include <string>

namespace {
    std::optional<uint32_t> parseDailyCaptureIndex(const std::filesystem::path& path, std::string_view datePrefix,
                                                   std::string_view extension) {
        if (path.extension() != extension) {
            return std::nullopt;
        }

        const std::string stem = path.stem().string();
        const std::string prefix = std::string(datePrefix) + "_";
        if (!stem.starts_with(prefix)) {
            return std::nullopt;
        }

        uint32_t index = 0;
        const std::string_view number(stem.data() + prefix.size(), stem.size() - prefix.size());
        if (number.empty()) {
            return std::nullopt;
        }

        for (char c : number) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return std::nullopt;
            }
            index = index * 10 + static_cast<uint32_t>(c - '0');
        }
        return index;
    }
}

std::filesystem::path capture_utils::makeDatedCaptureOutputPath(const std::filesystem::path& outputDirectory,
                                                                std::string_view extension) {
    const auto now = std::chrono::system_clock::now();
    const std::string datePrefix = std::format("{:%Y-%m-%d}", std::chrono::floor<std::chrono::days>(now));

    uint32_t nextIndex = 1;
    std::error_code fsError;
    for (std::filesystem::directory_iterator it(outputDirectory, fsError), end; !fsError && it != end; it.increment(fsError)) {
        if (fsError || !it->is_regular_file(fsError)) {
            continue;
        }
        if (const std::optional<uint32_t> index = parseDailyCaptureIndex(it->path(), datePrefix, extension)) {
            nextIndex = std::max(nextIndex, *index + 1);
        }
    }

    return outputDirectory / std::format("{}_{}{}", datePrefix, nextIndex, extension);
}

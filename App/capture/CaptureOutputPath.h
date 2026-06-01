#pragma once

#include <filesystem>
#include <string_view>

namespace capture_utils {
    [[nodiscard]] std::filesystem::path makeDatedCaptureOutputPath(const std::filesystem::path& outputDirectory,
                                                                   std::string_view extension);
}

#include "BmRunner/Data.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <string_view>

#include "BmRunner/Support.h"
using namespace Lattice;

namespace fs = std::filesystem;

namespace Benchmarks::BmRunner {
    namespace {
        std::optional<std::string> extractJsonObjectAfterKey(std::string_view text, std::string_view key) {
            const std::size_t keyPos = text.find(key);
            if (keyPos == std::string_view::npos) {
                return std::nullopt;
            }

            const std::size_t start = text.find('{', keyPos);
            if (start == std::string_view::npos) {
                return std::nullopt;
            }

            int depth = 0;
            bool inString = false;
            bool escaped = false;
            for (std::size_t i = start; i < text.size(); ++i) {
                const char ch = text[i];
                if (inString) {
                    if (escaped) {
                        escaped = false;
                    } else if (ch == '\\') {
                        escaped = true;
                    } else if (ch == '"') {
                        inString = false;
                    }
                    continue;
                }

                if (ch == '"') {
                    inString = true;
                    continue;
                }

                if (ch == '{') {
                    ++depth;
                } else if (ch == '}') {
                    --depth;
                    if (depth == 0) {
                        return std::string(text.substr(start, i - start + 1));
                    }
                }
            }

            return std::nullopt;
        }

        std::vector<std::string> extractJsonObjects(std::string_view text) {
            std::vector<std::string> objects;
            int depth = 0;
            bool inString = false;
            bool escaped = false;
            std::size_t start = std::string_view::npos;

            for (std::size_t i = 0; i < text.size(); ++i) {
                const char ch = text[i];
                if (inString) {
                    if (escaped) {
                        escaped = false;
                    } else if (ch == '\\') {
                        escaped = true;
                    } else if (ch == '"') {
                        inString = false;
                    }
                    continue;
                }

                if (ch == '"') {
                    inString = true;
                    continue;
                }

                if (ch == '{') {
                    if (depth == 0) {
                        start = i;
                    }
                    ++depth;
                } else if (ch == '}') {
                    --depth;
                    if (depth == 0 && start != std::string_view::npos) {
                        objects.emplace_back(text.substr(start, i - start + 1));
                        start = std::string_view::npos;
                    }
                }
            }

            return objects;
        }

        std::optional<std::string> extractStringField(std::string_view object, const std::string& key) {
            const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
            const std::string objectText(object);
            std::smatch match;
            if (std::regex_search(objectText, match, pattern)) {
                return match[1].str();
            }
            return std::nullopt;
        }

        std::optional<double> extractNumberField(std::string_view object, const std::string& key) {
            const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
            const std::string objectText(object);
            std::smatch match;
            if (std::regex_search(objectText, match, pattern)) {
                return std::stod(match[1].str());
            }
            return std::nullopt;
        }
    }

    BenchmarkData parseBenchmarkJson(const std::string& jsonText) {
        constexpr std::string_view benchmarksKey = "\"benchmarks\"";
        const std::size_t keyPos = jsonText.find(benchmarksKey);
        if (keyPos == std::string::npos) {
            die("JSON is missing the benchmarks section.");
        }

        const std::size_t arrayStart = jsonText.find('[', keyPos);
        const std::size_t arrayEnd = jsonText.find(']', arrayStart);
        if (arrayStart == std::string::npos || arrayEnd == std::string::npos) {
            die("Failed to extract the benchmarks array from JSON.");
        }

        BenchmarkData data;
        data.rawContextJson = extractJsonObjectAfterKey(jsonText, "\"context\"").value_or("{}");

        for (const auto& object : extractJsonObjects(std::string_view(jsonText).substr(arrayStart, arrayEnd - arrayStart + 1))) {
            BenchmarkItem item;
            item.runName = extractStringField(object, "run_name").value_or(
                extractStringField(object, "name").value_or(""));
            if (item.runName.empty()) {
                continue;
            }

            item.runType = extractStringField(object, "run_type").value_or("");
            item.aggregateName = extractStringField(object, "aggregate_name").value_or("");
            item.realTime = extractNumberField(object, "real_time");
            item.cpuTime = extractNumberField(object, "cpu_time");
            item.itemsPerSecond = extractNumberField(object, "items_per_second");
            data.benchmarks.push_back(std::move(item));
            data.rawBenchmarkJsons.push_back(object);
        }

        return data;
    }

    std::unordered_map<std::string, RowMetrics> buildRows(const BenchmarkData& data) {
        std::unordered_map<std::string, RowMetrics> rows;
        for (const auto& item : data.benchmarks) {
            auto& row = rows[item.runName];
            if (item.runType == "aggregate") {
                if (item.aggregateName == "median") {
                    row.realMedian = item.realTime;
                    row.cpuMedian = item.cpuTime;
                    if (item.itemsPerSecond) {
                        row.ips = item.itemsPerSecond;
                    }
                } else if (item.aggregateName == "mean") {
                    row.realMean = item.realTime;
                    if (!row.ips && item.itemsPerSecond) {
                        row.ips = item.itemsPerSecond;
                    }
                } else if (item.aggregateName == "cv" && item.realTime) {
                    row.realCv = *item.realTime * 100.0;
                }
            } else if (item.runType == "iteration") {
                if (!row.realMedian) {
                    row.realMedian = item.realTime;
                }
                if (!row.cpuMedian) {
                    row.cpuMedian = item.cpuTime;
                }
                if (!row.ips && item.itemsPerSecond) {
                    row.ips = item.itemsPerSecond;
                }
            }
        }
        return rows;
    }

    std::optional<BenchmarkData> loadBenchmarkDataIfExists(const fs::path& path) {
        if (!fs::is_regular_file(path)) {
            return std::nullopt;
        }
        return parseBenchmarkJson(readTextFile(path));
    }

    std::string serializeBenchmarkJson(const BenchmarkData& data) {
        std::ostringstream out;
        out << "{\n";
        out << "  \"context\": " << (data.rawContextJson.empty() ? "{}" : data.rawContextJson) << ",\n";
        out << "  \"benchmarks\": [\n";
        for (std::size_t i = 0; i < data.rawBenchmarkJsons.size(); ++i) {
            out << data.rawBenchmarkJsons[i];
            if (i + 1 != data.rawBenchmarkJsons.size()) {
                out << ",";
            }
            out << "\n";
        }
        out << "  ]\n";
        out << "}\n";
        return out.str();
    }

    void writeBenchmarkJson(const fs::path& path, const BenchmarkData& data) {
        std::ofstream output(path);
        if (!output) {
            die("Failed to write file: " + path.string());
        }
        output << serializeBenchmarkJson(data);
    }

    std::unordered_map<std::string, BenchmarkMeta> loadBenchMetadata(const fs::path& benchmarksRoot) {
        std::unordered_map<std::string, BenchmarkMeta> meta;
        const std::regex linePattern(R"meta(@bench_meta\s+\{.*"id":"([^"]+)".*"label":"([^"]+)".*"group":"([^"]+)".*\})meta");

        for (const auto& entry : fs::recursive_directory_iterator(benchmarksRoot)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".cpp") {
                continue;
            }

            std::ifstream input(entry.path());
            if (!input) {
                continue;
            }

            std::string line;
            while (std::getline(input, line)) {
                std::smatch match;
                if (!std::regex_search(line, match, linePattern)) {
                    continue;
                }

                meta[match[1].str()] = BenchmarkMeta{
                    .label = match[2].str(),
                    .group = match[3].str(),
                };
            }
        }

        return meta;
    }
}

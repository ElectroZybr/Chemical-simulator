#include "BmRunner/Support.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace Lattice;

namespace fs = std::filesystem;

namespace Benchmarks::BmRunner {
    namespace {
        struct SceneOption {
            std::string_view key;
            std::string_view label;
        };

        constexpr std::array kScenes{
            SceneOption{"ideal_crystal3d", "Ideal Crystal 3D"},
            SceneOption{"crystal3d", "Crystal 3D"},
            SceneOption{"crystal2d", "Crystal 2D"},
            SceneOption{"random_gas2d", "Random Gas 2D"},
        };
    }

    [[noreturn]] void die(const std::string& message) {
        std::cerr << "Error: " << message << '\n';
        std::exit(1);
    }

    std::string readTextFile(const fs::path& path) {
        std::ifstream input(path);
        if (!input) {
            die("Failed to open file: " + path.string());
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    std::string quoteShell(const std::string& value) {
        std::string result = "'";
        for (const char ch : value) {
            if (ch == '\'') {
                result += "'\\''";
            } else {
                result += ch;
            }
        }
        result += "'";
        return result;
    }

    std::string sceneLabel(std::string_view key) {
        for (const auto& scene : kScenes) {
            if (scene.key == key) {
                return std::string(scene.label);
            }
        }
        return std::string(key);
    }

    fs::path benchmarksRootFromExecutable(const char* argv0) {
        std::vector<fs::path> bases;
        bases.push_back(fs::current_path());

        std::error_code ec;
        const fs::path executablePath = fs::weakly_canonical(fs::absolute(argv0), ec);
        if (!ec) {
            bases.push_back(executablePath.parent_path());
        }

        for (const fs::path& base : bases) {
            const fs::path direct = base / "Lattice/benchmarks";
            if (fs::exists(direct / "bench.py")) {
                return direct;
            }
            if (fs::exists(base / "bench.py")) {
                return base;
            }

            for (fs::path current = base; !current.empty(); current = current.parent_path()) {
                const fs::path nested = current / "Lattice/benchmarks";
                if (fs::exists(nested / "bench.py")) {
                    return nested;
                }
                if (fs::exists(current / "bench.py")) {
                    return current;
                }
                if (current == current.root_path()) {
                    break;
                }
            }
        }

        die("Failed to locate the Lattice/benchmarks directory.");
    }

    fs::path resolveBenchmarkBinary(const fs::path& repoRoot) {
        std::vector<fs::path> candidates{
            repoRoot / "build" / "bench" / "benchmarks",
            repoRoot / "build" / "release" / "benchmarks",
            repoRoot / "build" / "benchmarks",
            repoRoot / "benchmarks",
        };

        for (const auto& path : candidates) {
            if (fs::is_regular_file(path)) {
                return path;
            }
        }
        return candidates.front();
    }

    fs::path resultsDir(const fs::path& benchmarksRoot) {
        return benchmarksRoot / "results";
    }

    void ensureResultsDir(const fs::path& dir) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) {
            die("Failed to create results/: " + ec.message());
        }
    }

    std::vector<fs::path> savedResults(const fs::path& dir) {
        std::vector<fs::path> files;
        if (!fs::exists(dir)) {
            return files;
        }

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end(), std::greater<>());
        return files;
    }

    std::string formatTimestamp(const fs::path& path) {
        std::string stem = path.stem().string();
        const auto underscore = stem.find('_');
        if (underscore != std::string::npos) {
            stem.replace(underscore, 1, " ");
        }

        int replaced = 0;
        for (char& ch : stem) {
            if (ch == '-' && replaced < 2) {
                ch = ':';
                ++replaced;
            }
        }
        return stem;
    }

    Config parseArgs(int argc, char** argv) {
        Config config;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto requireValue = [&](const std::string& flag) -> std::string {
                if (i + 1 >= argc) {
                    die("Expected a value after " + flag);
                }
                return argv[++i];
            };

            if (arg == "--filter") {
                config.filter = requireValue(arg);
            } else if (arg == "--save") {
                config.save = true;
            } else if (arg == "--list") {
                config.list = true;
            } else if (arg == "--repetitions") {
                config.repetitions = std::stoi(requireValue(arg));
            } else if (arg == "--min-time") {
                config.minTime = requireValue(arg);
            } else if (arg == "--scene") {
                config.scene = requireValue(arg);
            } else if (arg == "--help" || arg == "-h") {
                std::cout
                    << "Usage:\n"
                    << "  bench [--filter REGEX] [--save] [--repetitions N] [--min-time TIME] [--scene SCENE]\n"
                    << "  bench --list\n";
                std::exit(0);
            } else {
                die("Unknown argument: " + arg);
            }
        }

        return config;
    }

    std::string shortBenchId(std::string_view fullName) {
        const std::size_t first = fullName.find('/');
        if (first == std::string::npos) {
            return std::string(fullName);
        }
        const std::size_t second = fullName.find('/', first + 1);
        if (second == std::string::npos) {
            return std::string(fullName);
        }
        return std::string(fullName.substr(0, second));
    }

    std::string metaLookupId(std::string_view runName) {
        std::string id = shortBenchId(runName);
        constexpr std::string_view simulationFixture = "SimulationFixture/";
        if (id.rfind(simulationFixture, 0) == 0) {
            return "Fixture/" + id.substr(simulationFixture.size());
        }
        return id;
    }

    std::string benchArg(std::string_view fullName) {
        std::size_t pos = 0;
        for (int slash = 0; slash < 2; ++slash) {
            pos = fullName.find('/', pos);
            if (pos == std::string_view::npos) {
                return "-";
            }
            ++pos;
        }
        return std::string(fullName.substr(pos));
    }

    int atomCountForSceneKey(std::string_view key, int sceneExtent) {
        if (key == "ideal_crystal3d" || key == "crystal3d") {
            return sceneExtent * sceneExtent * sceneExtent;
        }
        if (key == "crystal2d" || key == "random_gas2d") {
            return sceneExtent * sceneExtent;
        }
        return sceneExtent;
    }
}

#pragma once

#include <string_view>
#include <vector>

#include <Lattice/Kernel/DynamicLibrary.hpp>
#include <Lattice/Tools/Logger.hpp>


class DLLoader {
public:
    bool load(const std::filesystem::path& path, std::string_view suffix) {
        if (std::filesystem::is_regular_file(path)) {
            return loadLibrary(path);
        }

        if (std::filesystem::is_directory(path)) {
            for (const auto& entry :
                std::filesystem::recursive_directory_iterator(path)) {

                if (!entry.is_regular_file())
                    continue;

                const auto& file = entry.path();

                if (file.extension() != DynamicLibrary::extension())
                    continue;

                if (!suffix.empty() &&
                    !file.stem().string().ends_with(suffix))
                    continue;

                loadLibrary(file);
            }

            return true;
        }

        Logger::error("DLLoader", "Path '{}' does not exist", path.string());
        return false;
    }

    DynamicLibrary* loadLibrary(const std::filesystem::path& path) {
        auto library = std::make_unique<DynamicLibrary>();

        if (!library->open(path)) {
            Logger::error("DLLoader", "Failed to open '{}': {}", path.string(), library->lastError());
            return nullptr;
        }

        DynamicLibrary* result = library.get();
        libraries.push_back(std::move(library));
        return result;
    }

    DynamicLibrary* find(std::string_view name) {
        for (auto& library : libraries) {
            if (library->path.stem() == name)
                return library.get();
        }

        return nullptr;
    }

    bool unload(std::string_view name) {
        for (auto it = libraries.begin(); it != libraries.end(); ++it) {
            if ((*it)->path.stem() != name)
                continue;

            libraries.erase(it);
            return true;
        }

        return false;
    }

    ~DLLoader() = default;

private:
    std::vector<std::unique_ptr<DynamicLibrary>> libraries;
};
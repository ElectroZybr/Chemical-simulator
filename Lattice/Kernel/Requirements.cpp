#include <Lattice/Kernel/Requirements.hpp>
#include <Lattice/Kernel/Registry.hpp>
#include <Lattice/Tools/Logger.hpp>
#include "Lattice/Tools/LogStyle.hpp"

#include <format>
#include <unordered_map>
#include <unordered_set>

namespace Lattice {

using ProvidedIndex = std::unordered_map<std::string, const PluginCatalog*>;

namespace {

ProvidedIndex makeProvidedIndex() {
    ProvidedIndex result;
    for (const auto& catalog : pluginCatalogs())
        for (const auto& provided : catalog.provided)
            result.emplace(provided, &catalog);
    return result;
}

const PluginCatalog* findCatalog(std::string_view name, const ProvidedIndex& index) {
    auto it = index.find(std::string(name));
    return it == index.end() ? nullptr : it->second;
}

void appendComposition(
    Logger::Tree& tree,
    const std::string& type,
    size_t depth,
    const ProvidedIndex& index,
    std::unordered_set<std::string>& seen)
{
    if (!seen.insert(type).second)
        return;

    const auto* catalog = findCatalog(type, index);
    if (!catalog)
        return;

    for (const auto& dep : catalog->deps) {
        tree.node(dep.type, depth);

        if (dep.kind == DepKind::Add)
            appendComposition(tree, dep.type, depth + 1, index, seen);
    }
}

ProvidedIndex indexProvided() {
    ProvidedIndex result;

    for (const auto& catalog : pluginCatalogs())
        for (const auto& provided : catalog.provided)
            result.emplace(provided, &catalog);

    return result;
}

const PluginCatalog* catalogFor(const ProvidedIndex& index, std::string_view name) {
    auto it = index.find(std::string(name));
    return it != index.end() ? it->second : nullptr;
}

std::vector<std::string> collectUniqueList(
    std::string_view name,
    const ProvidedIndex& index)
{
    std::vector<std::string> result;
    const auto* catalog = catalogFor(index, std::string(name));
    if (!catalog)
        return result;

    std::unordered_set<std::string> seen;

    auto walk = [&](auto&& self, const PluginCatalog& current) -> void {
        for (const auto& dep : current.deps) {
            if (dep.kind == DepKind::Require && seen.insert(dep.type).second)
                result.push_back(dep.type);

            if (dep.kind == DepKind::Add) {
                if (const auto* child = catalogFor(index, dep.type))
                    self(self, *child);
            }
        }
    };

    walk(walk, *catalog);
    return result;
}

} // namespace

std::vector<std::string> uniqueList( std::string_view name, const Registry& registry) {
    if (!registry.has(name)) {
        Logger::error(tag, "unknown component '{}'", name);
        return {};
    }

    const auto index = indexProvided();
    if (!catalogFor(index, std::string(name))) {
        Logger::error(tag, "no compile catalog for '{}'", name);
        return {};
    }

    return collectUniqueList(name, index);
}

std::vector<std::string> printUniqueList(std::string_view name, const Registry& registry) {
    const auto index = indexProvided();
    const auto requirements = collectUniqueList(name, index);

    Logger::Tree tree{"Dependencies"};

    for (const auto& requirement : requirements) {
        const bool exists = registry.has(requirement);

        tree.node(std::format("{}{}",
            exists ? Color::paint("✓ ", Color::ok)
                   : Color::paint("✗ ", Color::error),
            requirement
        ));
    }

    tree.print();
    return requirements;
}

bool check(std::string_view name, const Registry& registry) {
    if (!registry.has(name)) {
        Logger::error(tag, "unknown component '{}'", name);
        return false;
    }

    const auto index = indexProvided();
    if (!catalogFor(index, std::string(name))) {
        Logger::error(tag, "no compile catalog for '{}'", name);
        return false;
    }

    const auto requirements = collectUniqueList(name, index);

    for (const auto& requirement : requirements) {
        if (!registry.has(requirement)) {
            Logger::error(tag, "{} check failed", name);
            return false;
        }
    }

    Logger::ok(tag, "{} check passed", name);
    printCompositionTree(name);
    return true;
}

void printCompositionTree(std::string_view name) {
    const auto index = makeProvidedIndex();
    const auto* catalog = findCatalog(name, index);

    if (!catalog) {
        Logger::error(tag, "no compile catalog for '{}'", name);
        return;
    }

    Logger::Tree tree{std::string(name)};
    std::unordered_set<std::string> seen;
    appendComposition(tree, std::string(name), 0, index, seen);
    tree.print();
}

std::vector<CompileDep>& compileDepSink() {
    static std::vector<CompileDep> sink;
    return sink;
}

std::vector<PluginCatalog>& pluginCatalogs() {
    static std::vector<PluginCatalog> catalogs;
    return catalogs;
}

void recordPluginCatalog(PluginCatalog catalog) {
    Logger::info(tag, "plugin '{}' compile deps: {}", catalog.pluginId, catalog.deps.size());
    pluginCatalogs().push_back(std::move(catalog));
}

} // namespace Lattice
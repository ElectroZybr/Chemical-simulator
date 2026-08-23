#include <Lattice/Kernel/Requirements.hpp>
#include <Lattice/Kernel/Registry.hpp>
#include <Lattice/Tools/Logger.hpp>
#include "Lattice/Tools/LogStyle.hpp"

#include <format>
#include <unordered_map>
#include <unordered_set>

namespace Lattice {
namespace {

using ProvidedIndex = std::unordered_map<std::string, const PluginCatalog*>;

ProvidedIndex indexProvided() {
    ProvidedIndex byProvided;
    for (const auto& catalog : pluginCatalogs()) {
        for (const auto& provided : catalog.provided)
            byProvided.emplace(provided, &catalog);
    }
    return byProvided;
}

const PluginCatalog* catalogFor(const ProvidedIndex& byProvided, const std::string& type) {
    auto it = byProvided.find(type);
    return it == byProvided.end() ? nullptr : it->second;
}

bool producedHere(const PluginCatalog& catalog, const std::string& type) {
    for (const auto& dep : catalog.deps) {
        if ((dep.kind == DepKind::Add || dep.kind == DepKind::Use) && dep.type == type)
            return true;
    }
    return false;
}

void emitAdded(
    Logger::Tree& tree,
    const std::string& type,
    size_t depth,
    const ProvidedIndex& byProvided,
    std::unordered_set<std::string>& seenAdds)
{
    if (!seenAdds.insert(type).second)
        return;

    const PluginCatalog* catalog = catalogFor(byProvided, type);
    if (!catalog)
        return;

    std::unordered_set<std::string> emitted;
    for (const auto& dep : catalog->deps) {
        if (dep.type == type)
            continue;
        if (!emitted.insert(dep.type).second)
            continue;

        tree.node(dep.type, depth);
        if (dep.kind == DepKind::Add)
            emitAdded(tree, dep.type, depth + 1, byProvided, seenAdds);
    }
}

void printCompositionTree(
    std::string_view implName,
    const PluginCatalog& catalog,
    const ProvidedIndex& byProvided)
{
    Logger::Tree tree{std::string(implName)};
    std::unordered_set<std::string> emitted;
    std::unordered_set<std::string> seenAdds;
    seenAdds.insert(std::string(implName));

    for (const auto& dep : catalog.deps) {
        if (dep.kind == DepKind::Require && producedHere(catalog, dep.type))
            continue;
        if (dep.kind != DepKind::Require && dep.kind != DepKind::Add && dep.kind != DepKind::Use)
            continue;
        if (!emitted.insert(dep.type).second)
            continue;

        tree.node(std::format("{}", dep.type), 0);
        if (dep.kind == DepKind::Add)
            emitAdded(tree, dep.type, 1, byProvided, seenAdds);
    }
    tree.print();
}

void collectUniqueTypes(
    const PluginCatalog& catalog,
    const ProvidedIndex& byProvided,
    const Registry& registry,
    std::unordered_set<const PluginCatalog*>& seenCatalogs,
    std::unordered_set<std::string>& seenTypes,
    std::vector<std::string>& types,
    std::unordered_set<std::string>& optionalImpls)
{
    if (!seenCatalogs.insert(&catalog).second)
        return;

    auto push = [&](const std::string& type) {
        if (type.empty())
            return;
        if (seenTypes.insert(type).second)
            types.push_back(type);
    };

    for (const auto& dep : catalog.deps) {
        push(dep.type);
        if (!dep.impl.empty()) {
            push(dep.impl);
            optionalImpls.insert(dep.impl);
        }

        if (const Registry::TypeEntry* entry = registry.find(dep.type))
            push(entry->implements);

        if (dep.kind == DepKind::Use) {
            for (const auto& impl : registry.implementationsOf(dep.type)) {
                push(impl);
                optionalImpls.insert(impl);
            }
        }

        if (dep.kind == DepKind::Add) {
            if (const PluginCatalog* next = catalogFor(byProvided, dep.type))
                collectUniqueTypes(*next, byProvided, registry, seenCatalogs, seenTypes, types, optionalImpls);
        }
    }
}

void printUniqueList(
    const PluginCatalog& catalog,
    const ProvidedIndex& byProvided,
    const Registry& registry,
    bool& ok)
{
    std::unordered_set<const PluginCatalog*> seenCatalogs;
    std::unordered_set<std::string> seenTypes;
    std::unordered_set<std::string> optionalImpls;
    std::vector<std::string> types;
    collectUniqueTypes(catalog, byProvided, registry, seenCatalogs, seenTypes, types, optionalImpls);

    Logger::Tree tree{"Dependencies"};
    for (const auto& type : types) {
        const bool has = registry.has(type);
        const bool optional = optionalImpls.contains(type);
        const std::string mark = optional
            ? Color::paint("○ ", Color::brightMagenta)
            : has ? Color::paint("✓ ", Color::ok)
                  : Color::paint("✗ ", Color::error);
        tree.node(std::format("{}{}", mark, type), 0);
        if (!has && !optional)
            ok = false;
        if (optional && !has)
            ok = false;
    }
    tree.print();
}

} // namespace

std::vector<CompileDep>& compileDepSink() {
    static std::vector<CompileDep> sink;
    return sink;
}

std::vector<PluginCatalog>& pluginCatalogs() {
    static std::vector<PluginCatalog> catalogs;
    return catalogs;
}

void recordPluginCatalog(PluginCatalog catalog) {
    Logger::info("Requirements", "plugin '{}' compile deps: {}", catalog.pluginId, catalog.deps.size());
    pluginCatalogs().push_back(std::move(catalog));
}

std::vector<CompileDep> collectForService(std::string_view implName) {
    const auto byProvided = indexProvided();
    const PluginCatalog* root = catalogFor(byProvided, std::string(implName));
    if (!root)
        return {};

    std::vector<CompileDep> out;
    std::unordered_set<std::string> seenPlugins;

    auto walk = [&](auto&& self, const PluginCatalog& catalog) -> void {
        if (!seenPlugins.insert(catalog.pluginId).second)
            return;
        for (const auto& dep : catalog.deps) {
            out.push_back(dep);
            if (dep.kind != DepKind::Add)
                continue;
            if (const PluginCatalog* next = catalogFor(byProvided, dep.type))
                self(self, *next);
        }
    };
    walk(walk, *root);
    return out;
}

bool checkServiceRequirements(std::string_view implName, const Registry& registry) {
    if (!registry.hasImpl("ServiceAPI", implName) && !registry.has(implName)) {
        Logger::error("Requirements", "unknown service '{}'", implName);
        return false;
    }

    const auto byProvided = indexProvided();
    const PluginCatalog* catalog = catalogFor(byProvided, std::string(implName));
    if (!catalog) {
        Logger::error("Requirements", "no compile catalog for '{}'", implName);
        return false;
    }

    printCompositionTree(implName, *catalog, byProvided);

    bool ok = true;
    printUniqueList(*catalog, byProvided, registry, ok);

    if (!ok)
        Logger::error("Requirements", "{} check failed", implName);
    else
        Logger::ok("Requirements", "{} check passed", implName);

    return ok;
}

} // namespace Lattice

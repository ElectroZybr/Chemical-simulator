#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <Lattice/Kernel/TypeName.hpp>

namespace Lattice {

class Registry;

enum class DepKind : uint8_t {
    Require,
    Add,
    Use
};

struct CompileDep {
    std::string type;
    std::string impl;
    DepKind kind = DepKind::Require;
};

struct PluginCatalog {
    std::string pluginId;
    std::vector<CompileDep> deps;
    std::vector<std::string> provided;
};

std::vector<CompileDep>& compileDepSink();
std::vector<PluginCatalog>& pluginCatalogs();

bool check(std::string_view name, const Registry& registry);
std::vector<std::string> uniqueList( std::string_view name, const Registry& registry);
std::vector<std::string> printUniqueList(std::string_view name, const Registry& registry);
void printCompositionTree( std::string_view name);
void recordPluginCatalog(PluginCatalog catalog);

inline constexpr std::string_view tag = "Requirements";

#if defined(__GNUC__) || defined(__clang__)
#define LATTICE_DEP_HIDDEN __attribute__((visibility("hidden")))
#define LATTICE_DEP_USED [[gnu::used]]
#else
#define LATTICE_DEP_HIDDEN
#define LATTICE_DEP_USED
#endif

template<typename T, DepKind Kind>
struct DepNote {
    DepNote() {
        compileDepSink().push_back({
            std::string(typeName<T>()),
            {},
            Kind
        });
    }

    static DepNote instance;
};

template<typename T, DepKind Kind>
LATTICE_DEP_USED
LATTICE_DEP_HIDDEN
DepNote<T, Kind> DepNote<T, Kind>::instance{};

template<typename API, typename Impl>
struct DepNoteUse {
    DepNoteUse() {
        compileDepSink().push_back({
            std::string(typeName<API>()),
            std::string(typeName<Impl>()),
            DepKind::Use
        });
    }

    static DepNoteUse instance;
};

template<typename API, typename Impl>
LATTICE_DEP_USED
LATTICE_DEP_HIDDEN
DepNoteUse<API, Impl> DepNoteUse<API, Impl>::instance{};

template<typename T>
inline void noteRequire() {
    (void)&DepNote<T, DepKind::Require>::instance;
}

template<typename T>
inline void noteAdd() {
    (void)&DepNote<T, DepKind::Add>::instance;
}

template<typename T>
inline void noteUse() {
    (void)&DepNote<T, DepKind::Use>::instance;
}

template<typename API, typename Impl>
inline void noteUseImpl() {
    (void)&DepNoteUse<API, Impl>::instance;
}

} // namespace Lattice
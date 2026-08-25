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

void recordPluginCatalog(PluginCatalog catalog);
std::vector<CompileDep> collectForService(std::string_view implName);
bool checkRequirements(std::string_view implName, const Registry& registry);

#if defined(__GNUC__) || defined(__clang__)
#define LATTICE_DEP_HIDDEN __attribute__((visibility("hidden")))
#else
#define LATTICE_DEP_HIDDEN
#endif

template<typename T, DepKind Kind>
struct DepNote {
    DepNote() {
        compileDepSink().push_back(CompileDep{
            std::string(typeName<T>()),
            {},
            Kind
        });
    }
    static DepNote instance;
};

template<typename T, DepKind Kind>
#if defined(__GNUC__) || defined(__clang__)
[[gnu::used]]
#endif
LATTICE_DEP_HIDDEN DepNote<T, Kind> DepNote<T, Kind>::instance{};

template<typename API, typename Impl>
struct DepNoteUse {
    DepNoteUse() {
        compileDepSink().push_back(CompileDep{
            std::string(typeName<API>()),
            std::string(typeName<Impl>()),
            DepKind::Use
        });
    }
    static DepNoteUse instance;
};

template<typename API, typename Impl>
#if defined(__GNUC__) || defined(__clang__)
[[gnu::used]]
#endif
LATTICE_DEP_HIDDEN DepNoteUse<API, Impl> DepNoteUse<API, Impl>::instance{};

template<typename T>
inline void noteRequire() {
    volatile auto* p = &DepNote<T, DepKind::Require>::instance;
    (void)p;
}

template<typename T>
inline void noteAdd() {
    volatile auto* p = &DepNote<T, DepKind::Add>::instance;
    (void)p;
}

template<typename API>
inline void noteUse() {
    volatile auto* p = &DepNote<API, DepKind::Use>::instance;
    (void)p;
}

template<typename API, typename Impl>
inline void noteUseImpl() {
    volatile auto* p = &DepNoteUse<API, Impl>::instance;
    (void)p;
}

} // namespace Lattice

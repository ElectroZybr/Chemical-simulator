#pragma once

#include <any>
#include <cstdint>
#include <string>
#include <string_view>

#include "GUI/interface/panels/debug/DebugDrawers.h"

enum class DebugDisplayType : uint8_t { Series, Value };

struct DebugEntry {
    using DrawFn = void (*)(std::string_view label, const std::any& value);

    DebugEntry(std::string_view label = "", DebugDisplayType type = DebugDisplayType::Series, DrawFn draw = DebugDrawers::Float<2>)
        : label(label), type(type), draw(draw) {}
    std::string label;
    DebugDisplayType type;
    DrawFn draw;
};

inline DebugEntry DebugSeries(std::string_view label) { return {label, DebugDisplayType::Series, DebugDrawers::Float<2>}; }
inline DebugEntry DebugValue(std::string_view label, DebugEntry::DrawFn draw = DebugDrawers::Float<2>) {
    return {label, DebugDisplayType::Value, draw};
}

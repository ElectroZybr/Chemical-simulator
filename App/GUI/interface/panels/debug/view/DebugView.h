#pragma once
#include <any>
#include <deque>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "GUI/interface/panels/debug/DebugEntry.h"

class DebugView {
public:
    using Storage = std::variant<std::any, std::deque<float>>;
    using CustomDrawFn = std::function<void(float)>;

    DebugView(std::string_view title, std::initializer_list<DebugEntry> entries);
    DebugView(std::string_view title, CustomDrawFn customDraw);
    const char* getTitle() const { return title.data(); }
    void draw(float uiScale);

    void add_data(std::string_view label, std::any value);

    bool visible = true;

private:
    static constexpr int HISTORY_SIZE = 300;

    struct DebugData {
        DebugEntry entry;
        Storage storage;
    };

    std::string title;
    std::vector<DebugData> data;
    std::unordered_map<std::string, size_t> indicesByLabel;
    CustomDrawFn customDraw_;
};

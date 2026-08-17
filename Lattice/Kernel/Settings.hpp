#pragma once

#include <unordered_map>
#include <string>
#include <functional>
#include <variant>
#include <stdexcept>

namespace Lattice {

enum class ParamType { Bool, Int, Double, String };

struct ParamInfo {
    std::string key;        // "Verlet.dt"
    std::string group;      // "Verlet"
    std::string name;       // "dt"
    ParamType   type{};

    double min = 0, max = 0;
    bool hasRange = false;
};

class Settings {
public:
    using Value = std::variant<bool, int64_t, double, std::string>;
private:
    struct Entry {
        ParamInfo info;
        std::function<Value()> get;
        std::function<void(const Value&)> set;
    };

    std::unordered_map<std::string, Entry> entries_;

public:
    template<typename T>
    void bind(std::string_view group, std::string_view name, T* ptr,
              double min = 0, double max = 0, bool hasRange = false) {
        const std::string key = makeKey(group, name);
        Entry entry;
        entry.info.key = key;
        entry.info.group = std::string(group);
        entry.info.name = std::string(name);
        entry.info.type = typeOf<T>();
        entry.info.min = min;
        entry.info.max = max;
        entry.info.hasRange = hasRange;
        entry.get = [ptr]() -> Value { return Value{*ptr}; };
        entry.set = [ptr](const Value& v) { *ptr = std::get<T>(v); };
        entries_[key] = std::move(entry);
    }

    // перегрузка для float. Setting всегда использует Double 
    void bind(std::string_view group, std::string_view name, float* ptr,
              double min = 0, double max = 0, bool hasRange = false) {
        const std::string key = makeKey(group, name);
        Entry entry;
        entry.info = { key, std::string(group), std::string(name),
                ParamType::Double, min, max, hasRange };
        entry.get = [ptr]() -> Value {
            return Value{ static_cast<double>(*ptr) };
        };
        entry.set = [ptr](const Value& v) {
            *ptr = static_cast<float>(std::get<double>(v));
        };
        entries_[key] = std::move(entry);
    }

    void unbind(std::string_view group, std::string_view name) {
        entries_.erase(makeKey(group, name));
    }

    void unbindGroup(std::string_view group) {
        for (auto it = entries_.begin(); it != entries_.end(); ) {
            if (it->second.info.group == group)
                it = entries_.erase(it);
            else
                ++it;
        }
    }

    template<typename T>
    T get(std::string_view group, std::string_view name) const {
        return std::get<T>(find(makeKey(group, name)).get());
    }

    template<typename T>
    void set(std::string_view group, std::string_view name, T value) {
        auto& entry = find(makeKey(group, name));
        entry.set(Value{std::move(value)});
    }

    template<typename T>
    void set(std::string_view key, T value) {
        auto& entry = find(std::string(key));
        entry.set(Value{std::move(value)});
    }

    std::vector<ParamInfo> list() const {
        std::vector<ParamInfo> out;
        out.reserve(entries_.size());
        for (auto& [_, entry] : entries_)
            out.push_back(entry.info);
        return out;
    }

    std::vector<ParamInfo> listGroup(std::string_view group) const {
        std::vector<ParamInfo> out;
        for (auto& [_, entry] : entries_)
            if (entry.info.group == group)
                out.push_back(entry.info);
        return out;
    }

private:
    template<typename T>
    static ParamType typeOf() {
        if constexpr (std::is_same_v<T, bool>) return ParamType::Bool;
        else if constexpr (std::is_integral_v<T>) return ParamType::Int;
        else if constexpr (std::is_floating_point_v<T>) return ParamType::Double;
        else return ParamType::String;
    }

    static std::string makeKey(std::string_view group, std::string_view name) {
        return std::string(group) + "." + std::string(name);
    }

    Entry& find(const std::string& key) {
        auto it = entries_.find(key);
        if (it == entries_.end())
            throw std::runtime_error("Settings: unknown param '" + key + "'");
        return it->second;
    }

    const Entry& find(const std::string& key) const {
        return const_cast<Settings*>(this)->find(key);
    }
};
}
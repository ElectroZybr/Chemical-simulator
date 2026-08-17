#pragma once

#include <type_traits>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <functional>
#include <variant>
#include <stdexcept>
#include <vector>

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
        entry.info = { key, std::string(group), std::string(name), typeOf<T>(), min, max, hasRange };
        entry.get = [ptr]() -> Value { return valueMake(*ptr); };
        entry.set = [ptr](const Value& v) { *ptr = valueCast<T>(v); };
        entries_[key] = std::move(entry);
    }

    template<typename T, typename F>
    void bind(std::string_view group, std::string_view name, T* ptr, F&& onChange) {
        const std::string key = makeKey(group, name);
        Entry entry;
        entry.info = { key, std::string(group), std::string(name), typeOf<T>() };
        entry.get = [ptr]() -> Value { return valueMake(*ptr); };
        entry.set = makeSetter(ptr, std::forward<F>(onChange));
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
    template<typename>
    inline static constexpr bool always_false = false;

    template<typename T>
    static ParamType typeOf() {
        if constexpr (std::is_same_v<T, bool>) return ParamType::Bool;
        else if constexpr (std::is_integral_v<T>) return ParamType::Int;
        else if constexpr (std::is_floating_point_v<T>) return ParamType::Double;
        else if constexpr (std::is_same_v<T, std::string>) return ParamType::String;
        else static_assert(always_false<T>, "Unsupported Settings type");
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

    template<typename T, typename F>
    static std::function<void(const Value&)>
    makeSetter(T* ptr, F&& onChange) {
        return [ptr, onChange = std::forward<F>(onChange)](const Value& v) mutable {
            T value = valueCast<T>(v);
            *ptr = value;
            onChange(value);
        };
    }

    template<typename T>
    static T valueCast(const Value& value) {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            return static_cast<T>(std::get<double>(value));
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return std::get<bool>(value);
        }
        else if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(std::get<int64_t>(value));
        }
        else {
            return std::get<T>(value);
        }
    }

    template<typename T>
    static Value valueMake(T value) {
        if constexpr (std::is_same_v<T, bool>) {
            return Value{value};
        }
        else if constexpr (std::is_integral_v<T>) {
            return Value{static_cast<int64_t>(value)};
        }
        else if constexpr (std::is_floating_point_v<T>) {
            return Value{static_cast<double>(value)};
        }
        else {
            return Value{std::move(value)};
        }
    }
};
}
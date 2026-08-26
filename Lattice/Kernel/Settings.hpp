#pragma once

#include <type_traits>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <functional>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Lattice/Kernel/Exception.hpp>

namespace Lattice {

enum class ParamType { Bool, Int, Double, Vec2, Vec3, Vec4, String, Action };

struct ParamInfo {
    std::string key;
    std::string group;
    std::string name;
    ParamType   type{};

    double min = 0, max = 0;
    bool hasRange = false;
};

class Settings {
public:
    using Value = std::variant<bool, int64_t, double, glm::vec2, glm::vec3, glm::vec4, std::string>;
private:
    static constexpr std::string_view tag = "Settings";
    struct Entry {
        ParamInfo info;
        std::function<Value()>            get = nullptr;
        std::function<void(const Value&)> set = nullptr;
        std::function<void()>         handler = nullptr;
    };

    std::unordered_map<std::string, Entry> entries_;

public:
    template<typename T>
    void bind(std::string_view group, std::string_view name, T* ptr,
              double min = 0, double max = 0, bool hasRange = false) {
        const std::string key = makeKey(group, name);
        Entry& entry = entries_[key];
        entry.info = { key, std::string(group), std::string(name), typeOf<T>(), min, max, hasRange };
        entry.get = [ptr]() -> Value { return valueMake(*ptr); };
        entry.set = [ptr](const Value& v) { *ptr = valueCast<T>(v); };
    }

    template<typename T, typename F>
    void bind(std::string_view group, std::string_view name, T* ptr, F&& onChange) {
        const std::string key = makeKey(group, name);
        Entry& entry = entries_[key];
        entry.info = { key, std::string(group), std::string(name), typeOf<T>() };
        entry.get = [ptr]() -> Value { return valueMake(*ptr); };
        entry.set = makeSetter(ptr, std::forward<F>(onChange));
    }

    void on(std::string_view group, std::string_view name, std::function<void()> fn) {
        const std::string key = makeKey(group, name);
        Entry& entry = entries_[key];
        entry.info = { key, std::string(group), std::string(name), ParamType::Action };
        entry.handler = std::move(fn);
    }

    // для редкого вызова из gui, не использовать для частых событий (поиск по мапе)
    void fire(std::string_view key) {
        if (auto h = handler(key))
            h();
    }

    std::function<void()> handler(std::string_view key) const {
        const auto& e = find(std::string(key));
        if (e.info.type != ParamType::Action)
            throw Lattice::Exception(tag, "Settings: not an action: ", std::string(key));
        if (!e.handler)
            throw Lattice::Exception(tag, "Settings: action has no handler: ", std::string(key));
        return e.handler;
    }

    std::function<void()> tryHandler(std::string_view key) const {
        auto it = entries_.find(std::string(key));
        if (it == entries_.end()) return {};
        if (it->second.info.type != ParamType::Action) return {};
        return it->second.handler;
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

    bool hasValue(std::string_view key) const {
        auto it = entries_.find(std::string(key));
        return it != entries_.end()
            && it->second.info.type != ParamType::Action
            && static_cast<bool>(it->second.set);
    }

    ParamType type(std::string_view key) const;

    std::function<void()> makeToggle(std::string_view key) {
        const std::string k{key};
        return [this, k] {
            auto& e = find(k);
            if (e.info.type != ParamType::Bool)
                throw Lattice::Exception(tag, "toggle expects bool: ", k);
            const bool v = std::get<bool>(e.get());
            e.set(Value{!v});
        };
    }

    std::function<void()> makeAdd(std::string_view key, double delta) {
        const std::string k{key};
        return [this, k, delta] {
            auto& e = find(k);
            if (e.info.type == ParamType::Double) {
                double v = std::get<double>(e.get());
                e.set(Value{v + delta});
            } else if (e.info.type == ParamType::Int) {
                int64_t v = std::get<int64_t>(e.get());
                e.set(Value{v + static_cast<int64_t>(delta)});
            } else {
                throw Lattice::Exception(tag, "add expects int/double: ", k);
            }
        };
    }

    template<typename T>
    T getByKey(std::string_view key) const {
        return std::get<T>(find(std::string(key)).get());
    }

    template<typename T>
    void setByKey(std::string_view key, T value) {
        find(std::string(key)).set(Value{std::move(value)});
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

    void setFromString(std::string_view key, std::string_view str) {
        auto& e = find(std::string(key));
        switch (e.info.type) {
        case ParamType::Bool:
            e.set(Value{str == "true" || str == "1"});
            break;
        case ParamType::Int:
            e.set(Value{static_cast<int64_t>(std::stoll(std::string(str)))});
            break;
        case ParamType::Double:
            e.set(Value{std::stod(std::string(str))});
            break;
        case ParamType::String:
            e.set(Value{std::string(str)});
            break;
        default:
            throw Lattice::Exception(tag, "unsupported type for string set");
        }
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

    static std::string makeKey(std::string_view group, std::string_view name) {
        return std::string(group) + "." + std::string(name);
    }

    Entry& find(const std::string& key) {
        auto it = entries_.find(key);
        if (it == entries_.end())
            throw Lattice::Exception(tag, "Settings: unknown param '{}'", key);
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
    static ParamType typeOf() {
        if constexpr (std::is_same_v<T, bool>)
            return ParamType::Bool;
        else if constexpr (std::is_integral_v<T>)
            return ParamType::Int;
        else if constexpr (std::is_floating_point_v<T>)
            return ParamType::Double;
        else if constexpr (std::is_same_v<T, glm::vec2>)
            return ParamType::Vec2;
        else if constexpr (std::is_same_v<T, glm::vec3>)
            return ParamType::Vec3;
        else if constexpr (std::is_same_v<T, glm::vec4>)
            return ParamType::Vec4;
        else if constexpr (std::is_same_v<T, std::string>)
            return ParamType::String;
        else
            static_assert(always_false<T>, "Unsupported Settings type");
    }

    template<typename T>
    static Value valueMake(T value) {
        if constexpr (std::is_same_v<T, bool>)
            return value;
        else if constexpr (std::is_integral_v<T>)
            return static_cast<int64_t>(value);
        else if constexpr (std::is_floating_point_v<T>)
            return static_cast<double>(value);
        else
            return value;
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
};
}
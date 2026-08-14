#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

/*
Базовая инфраструктура runtime-модулей движка.

Этот файл описывает общий механизм регистрации и создания модулей, который
используется для интеграторов, force field, thermostat и других расширений,
подключаемых через плагины. Здесь находятся метаданные модуля, реестр доступных
реализаций и helper для владения активным экземпляром выбранного модуля.
Смысл этого слоя — держать создание и выбор модулей явными, легковесными и
независимыми от конкретного физического подмодуля или конкретного плагина.
*/

class IModule {
public:
    virtual ~IModule() = default;
};

template <typename TModule>
using ModuleFactory = std::unique_ptr<TModule> (*)();

template <typename TModule>
struct ModuleMeta {
    std::string id;
    std::string description;
    ModuleFactory<TModule> factory = nullptr;
};

template <typename TModule, typename TImplementation>
ModuleMeta<TModule> makeModuleMeta() {
    return ModuleMeta<TModule>{
        .id = std::string(TImplementation::id),
        .description = std::string(TImplementation::description),
        .factory = []() -> std::unique_ptr<TModule> {
            return std::make_unique<TImplementation>();
        },
    };
}

template <typename TModule>
class ModuleRegistry2 {
public:
    using Meta = ModuleMeta<TModule>;

    bool add(Meta meta) {
        if (meta.id.empty() || meta.factory == nullptr || find(meta.id) != nullptr) {
            return false;
        }

        items_.push_back(std::move(meta));
        return true;
    }

    const Meta* find(std::string_view id) const {
        for (const Meta& item : items_) {
            if (item.id == id) {
                return &item;
            }
        }
        return nullptr;
    }

    std::unique_ptr<TModule> create(std::string_view id) const {
        const Meta* meta = find(id);
        return meta != nullptr && meta->factory != nullptr ? meta->factory() : nullptr;
    }

    const std::vector<Meta>& items() const { return items_; }

private:
    std::vector<Meta> items_{};
};

template <typename TDerived, typename TModule>
class RegisteredModuleOwner {
public:
    RegisteredModuleOwner() = default;
    RegisteredModuleOwner(const RegisteredModuleOwner&) = delete;
    RegisteredModuleOwner& operator=(const RegisteredModuleOwner&) = delete;
    RegisteredModuleOwner(RegisteredModuleOwner&&) noexcept = default;
    RegisteredModuleOwner& operator=(RegisteredModuleOwner&&) noexcept = default;

    bool set(std::string_view id) {
        if constexpr (requires { TDerived::allowEmpty; }) {
            if constexpr (TDerived::allowEmpty) {
                if (id.empty()) {
                    currentId_.clear();
                    impl_.reset();
                    return true;
                }
            }
        }

        const ModuleMeta<TModule>* meta = TDerived::registry().find(id);
        if (meta == nullptr || meta->factory == nullptr) {
            return false;
        }

        std::unique_ptr<TModule> impl = meta->factory();
        if (!impl) {
            return false;
        }

        if constexpr (requires(TDerived& self, TModule & module) { self.onModuleSet(module); }) {
            static_cast<TDerived&>(*this).onModuleSet(*impl);
        }

        currentId_ = meta->id;
        impl_ = std::move(impl);
        return true;
    }

    std::string_view get() const { return currentId_; }
    TModule* active() const { return impl_.get(); }

protected:
    std::string currentId_{};
    std::unique_ptr<TModule> impl_{};
};

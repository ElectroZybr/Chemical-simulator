#pragma once

#include <filesystem>
#include <vector>

#include "Lattice/Kernel/ModuleRegistry.hpp"
#include "Lattice/Kernel/DynamicLibrary.hpp"
#include "Lattice/Kernel/PluginAPI.hpp"
#include "toml++/toml.hpp"

namespace Kernel {
struct LoadedPlugin {
    LoadedPlugin(DynamicLibrary&& library, PluginContext&& context)
        : library(std::move(library)),
          context(std::move(context)) {}
    DynamicLibrary library;
    PluginInfo info;
    PluginContext context;
    PluginInitFn init;
    PluginShutdownFn shutdown;
};

class PluginManager {
public:
/**
 * @brief Сканирует директорию и ищет манифесты плагинов.
 *
 * Найденные плагины добавляются в список кандидатов на загрузку.
 *
 * @param path Корневая директория поиска плагинов.
 */
    void scanDirectory(std::filesystem::path path);

/**
 * @brief Проверяет все найденные плагины и формирует очередь загрузки.
 *
 * Выполняет проверку зависимостей и добавляет доступные плагины
 * в очередь загрузки в правильном порядке.
 */
    void checkCandidates();

/**
 * @brief Загружает все плагины, прошедшие проверку зависимостей.
 *
 * Использует очередь загрузки, сформированную во время проверки.
 * Плагины загружаются в порядке, учитывающем их зависимости.
 */
    void loadCandidates();

    ~PluginManager();
private:
/**
 * @brief Читает манифест плагина из TOML-файла.
 *
 * Извлекает идентификатор, имя, версию, версию API ядра
 * и список зависимостей плагина.
 *
 * @param path Путь к файлу Plugin.toml.
 * @return Заполненная структура PluginManifest.
 *
 * @throws toml::parse_error Если файл имеет неверный TOML-синтаксис.
 */
    PluginManifest parseManifest(std::filesystem::path path);

/**
 * @brief Проверяет возможность загрузки плагина и его зависимостей.
 *
 * Рекурсивно проходит по дереву зависимостей плагина,
 * проверяет наличие необходимых плагинов и отсутствие циклических зависимостей.
 *
 * @param id Идентификатор проверяемого плагина.
 *
 * @return true, если плагин и все его зависимости могут быть загружены.
 * @return false, если найдена ошибка зависимости или циклическая зависимость.
 */
    bool canLoad(const std::string& id);

/**
 * @brief Добавляет плагин и его зависимости в очередь загрузки.
 *
 * Выполняет рекурсивный обход графа зависимостей и формирует порядок,
 * в котором плагины должны быть загружены.
 *
 * @param id Идентификатор плагина.
 *
 * @return true, если плагин добавлен в очередь загрузки.
 */
    bool prepareLoad(const std::string& id);

/**
 * @brief Загружает динамическую библиотеку плагина.
 *
 * Ищет бинарный файл в директории плагина, открывает библиотеку,
 * вызывает функцию инициализации и сохраняет загруженный плагин.
 *
 * @param plugin Загружаемый плагин.
 *
 * @return true, если загрузка прошла успешно.
 */
    bool loadPlugin(const PluginCandidate* pluginCandidate);
private:
    static constexpr std::string_view moduleName = "PluginManager";
    ModuleRegistry globalRegistry;
    std::vector<LoadedPlugin> plugins;
    std::unordered_map<std::string, PluginCandidate> candidates;
    std::vector<PluginCandidate*> loadQueue;
};
}
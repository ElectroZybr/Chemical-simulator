#include <Lattice/Tools/Fixture.hpp>
#include <Lattice/Tools/Tests.hpp>

#include <Lattice/Lattice.hpp>


namespace Lattice {

TEST(Plugin_Empty, RuntimeFixture,
    "Пустой плагин должен успешно загрузиться.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Empty"
    );

    fixture.pluginManager.checkCandidates();
    fixture.pluginManager.loadCandidates();

    auto* plugin = fixture.pluginManager.findCandidate("Empty");

    REQUIRE(plugin);
    REQUIRE(plugin->status == LoadStatus::Loaded);
}

TEST(Plugin_MissingDependency, RuntimeFixture,
    "Плагин с отсутствующей зависимостью не должен попасть в очередь загрузки.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/MissingDependency"
    );

    fixture.pluginManager.checkCandidates();

    auto* plugin = fixture.pluginManager.findCandidate("MissingDependency");

    REQUIRE(plugin);
    REQUIRE(plugin->status == LoadStatus::MissingDependency);
    REQUIRE(fixture.pluginManager.queue().empty());
}

TEST(Plugin_DependencyOrder, RuntimeFixture,
    "Зависимость должна находиться в очереди загрузки раньше зависимого плагина.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Provider"
    );

    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Consumer"
    );

    fixture.pluginManager.checkCandidates();

    const auto& queue = fixture.pluginManager.queue();

    REQUIRE(queue.size() == 2);

    REQUIRE(queue[0]->manifest.id == "Provider");
    REQUIRE(queue[1]->manifest.id == "Consumer");
}

TEST(Plugin_DependencyCycle, RuntimeFixture,
    "Циклическая зависимость должна быть обнаружена до загрузки библиотек.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/CycleA"
    );

    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/CycleB"
    );

    fixture.pluginManager.checkCandidates();

    auto* a = fixture.pluginManager.findCandidate("CycleA");
    auto* b = fixture.pluginManager.findCandidate("CycleB");

    REQUIRE(a);
    REQUIRE(b);

    REQUIRE(a->status == LoadStatus::DependencyCycle);
    REQUIRE(b->status == LoadStatus::DependencyCycle);

    REQUIRE(fixture.pluginManager.queue().empty());
}

TEST(Plugin_Provider, RuntimeFixture,
    "Плагин, предоставляющий API или компонент, должен успешно загрузиться.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Provider"
    );

    fixture.pluginManager.checkCandidates();
    fixture.pluginManager.loadCandidates();

    auto* plugin = fixture.pluginManager.findCandidate("Provider");

    REQUIRE(plugin);
    REQUIRE(plugin->status == LoadStatus::Loaded);
    REQUIRE(plugin->library != nullptr);
}

TEST(Plugin_Consumer, RuntimeFixture,
    "Плагин с корректной зависимостью должен успешно загрузиться вместе с ней.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Provider"
    );

    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Consumer"
    );

    fixture.pluginManager.checkCandidates();
    fixture.pluginManager.loadCandidates();

    auto* provider = fixture.pluginManager.findCandidate("Provider");
    auto* consumer = fixture.pluginManager.findCandidate("Consumer");

    REQUIRE(provider);
    REQUIRE(consumer);

    REQUIRE(provider->status == LoadStatus::Loaded);
    REQUIRE(consumer->status == LoadStatus::Loaded);

    REQUIRE(provider->library != nullptr);
    REQUIRE(consumer->library != nullptr);
}

TEST(Plugin_NoRegister, RuntimeFixture,
    "Плагин без plugin_register должен завершиться ошибкой загрузки.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/NoRegister"
    );

    fixture.pluginManager.checkCandidates();
    fixture.pluginManager.loadCandidates();

    auto* plugin = fixture.pluginManager.findCandidate("NoRegister");

    REQUIRE(plugin);
    REQUIRE(plugin->status == LoadStatus::Failed);
    REQUIRE(plugin->library == nullptr);
}

TEST(Plugin_FailingRegister, RuntimeFixture,
    "Плагин, вернувший false из plugin_register, не должен считаться загруженным.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/FailingRegister"
    );

    fixture.pluginManager.checkCandidates();
    fixture.pluginManager.loadCandidates();

    auto* plugin = fixture.pluginManager.findCandidate("FailingRegister");

    REQUIRE(plugin);
    REQUIRE(plugin->status == LoadStatus::Failed);
    REQUIRE(plugin->library == nullptr);
}

TEST(Plugin_CheckCandidatesTwice, RuntimeFixture,
    "Повторная проверка плагинов не должна дублировать очередь загрузки.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Provider"
    );

    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Consumer"
    );

    fixture.pluginManager.checkCandidates();

    const auto firstSize = fixture.pluginManager.queue().size();

    fixture.pluginManager.checkCandidates();

    const auto secondSize = fixture.pluginManager.queue().size();

    REQUIRE(firstSize == secondSize);
}

TEST(Plugin_LoadEmptyOnlyOnce, RuntimeFixture,
    "Повторная загрузка уже загруженного плагина не должна создавать вторую библиотеку.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Empty"
    );

    fixture.pluginManager.checkCandidates();
    fixture.pluginManager.loadCandidates();

    auto* plugin = fixture.pluginManager.findCandidate("Empty");

    REQUIRE(plugin);
    REQUIRE(plugin->status == LoadStatus::Loaded);

    auto* library = plugin->library;

    fixture.pluginManager.loadCandidates();

    REQUIRE(plugin->status == LoadStatus::Loaded);
    REQUIRE(plugin->library == library);
}

TEST(Plugin_DependencyLoad, RuntimeFixture,
    "Зависимость должна быть загружена до плагина, который от неё зависит.")
{
    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Provider"
    );

    fixture.pluginManager.scanDirectory(
        "Lattice/tests/TestPlugins/Consumer"
    );

    fixture.pluginManager.checkCandidates();
    fixture.pluginManager.loadCandidates();

    auto* provider = fixture.pluginManager.findCandidate("Provider");
    auto* consumer = fixture.pluginManager.findCandidate("Consumer");

    REQUIRE(provider);
    REQUIRE(consumer);

    REQUIRE(provider->status == LoadStatus::Loaded);
    REQUIRE(consumer->status == LoadStatus::Loaded);

    REQUIRE(provider->library != nullptr);
    REQUIRE(consumer->library != nullptr);

    REQUIRE(fixture.pluginManager.queue()[0] == provider);
    REQUIRE(fixture.pluginManager.queue()[1] == consumer);
}

}
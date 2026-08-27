#include <Lattice/Tools/Fixture.hpp>
#include <Lattice/Tools/Tests.hpp>

#include <Lattice/Lattice.hpp>


namespace Lattice {

class TestComponent {
public:
    int value = 0;
    bool configured = false;

    void configure(Components&) {
        configured = true;
    }
};

class TestAPI {
public:
    virtual ~TestAPI() = default;
};

class TestImplA : public TestAPI {
public:
    int value = 10;
};

class TestImplB : public TestAPI {
public:
    int value = 20;
};


TEST(Components_DeepTreeLookup, RuntimeFixture, 
"Поиск компонента должен подниматься по дереву родителей, но не заходить в соседние ветки. \
Child-ветка должна видеть свои компоненты и компоненты предков.")
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Components branchA(&fixture.registry, &fixture.root, "BranchA");
    branchA.add<TestComponent>("a");

    Components branchB(&fixture.registry, &fixture.root, "BranchB");
    branchB.add<TestComponent>("b");

    Components branchAChild(&fixture.registry, &branchA, "BranchAChild");
    branchAChild.add<TestComponent>("child");

    REQUIRE(fixture.root.find<TestComponent>("root").exists());
    REQUIRE(branchA.find<TestComponent>("root").exists());
    REQUIRE(branchAChild.find<TestComponent>("root").exists());

    REQUIRE(branchA.find<TestComponent>("a").exists());
    REQUIRE(branchAChild.find<TestComponent>("a").exists());

    REQUIRE(branchB.find<TestComponent>("b").exists());
    REQUIRE(!branchA.find<TestComponent>("b").exists());
    REQUIRE(!fixture.root.find<TestComponent>("missing").exists());
}

TEST(Components_Shadowing, RuntimeFixture, 
"Компонент в дочерней ветке должен скрывать компонент с тем же именем из родительской ветки. \
При этом оба объекта должны оставаться независимыми экземплярами.")
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("shared");

    Components branch(&fixture.registry, &fixture.root, "Branch");
    branch.add<TestComponent>("shared");

    auto rootComponent = fixture.root.require<TestComponent>("shared");
    auto branchComponent = branch.require<TestComponent>("shared");

    REQUIRE(rootComponent);
    REQUIRE(branchComponent);
    REQUIRE(&rootComponent.get() != &branchComponent.get());
}

TEST(Components_ShadowingDoesNotLeak, RuntimeFixture, 
"Одинаковые имена компонентов в соседних ветках не должны влиять друг на друга. \
Поиск из одной ветки не должен случайно находить локальный компонент другой ветки.")
 {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("shared");

    Components branchA(&fixture.registry, &fixture.root, "A");
    Components branchB(&fixture.registry, &fixture.root, "B");

    branchA.add<TestComponent>("shared");

    auto a = branchA.find<TestComponent>("shared");
    auto b = branchB.find<TestComponent>("shared");

    REQUIRE(a.exists());
    REQUIRE(b.exists());
    REQUIRE(a.get() != b.get());
}

TEST(Components_LocalCollect, RuntimeFixture, 
"Локальный поиск должен возвращать компоненты только из текущей ветки. \
Компоненты дочерних узлов не должны попадать в результат.")
 {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Components branch(&fixture.registry, &fixture.root, "Branch");
    branch.add<TestComponent>("a");
    branch.add<TestComponent>("b");

    Components child(&fixture.registry, &branch, "Child");
    child.add<TestComponent>("child");

    auto root = fixture.root.localCollect<TestComponent>();
    auto local = branch.localCollect<TestComponent>();
    auto nested = child.localCollect<TestComponent>();

    REQUIRE(root.size() == 1);
    REQUIRE(local.size() == 2);
    REQUIRE(nested.size() == 1);
}

TEST(Components_GlobalCollectDeepTree, RuntimeFixture, 
"Глобальный поиск должен обходить всё дерево компонентов независимо от глубины вложенности. \
В результат должны попасть компоненты из всех веток и дочерних узлов.")
 {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Components branchA(&fixture.registry, &fixture.root, "A");
    branchA.add<TestComponent>("a");

    Components branchB(&fixture.registry, &fixture.root, "B");
    branchB.add<TestComponent>("b");

    Components childA(&fixture.registry, &branchA, "ChildA");
    childA.add<TestComponent>("aa");

    Components childB(&fixture.registry, &branchB, "ChildB");
    childB.add<TestComponent>("bb");

    auto components = childA.globalCollect<TestComponent>();

    REQUIRE(components.size() == 5);
}

TEST(Components_GlobalCollectIgnoresInstanceName, RuntimeFixture, 
"Глобальный поиск должен находить все экземпляры компонента независимо от имени реализации.")
 {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("one");
    fixture.root.add<TestComponent>("two");
    fixture.root.add<TestComponent>("three");

    Components branch(&fixture.registry, &fixture.root, "Branch");
    branch.add<TestComponent>("four");
    branch.add<TestComponent>("five");

    auto components = fixture.root.globalCollect<TestComponent>();

    REQUIRE(components.size() == 5);
}

TEST(Components_RemoveDoesNotAffectParent, RuntimeFixture, 
"Удаление компонента из дочерней ветки не должно затрагивать компонент с тем же именем у родителя. \
Родительский компонент должен остаться доступным после удаления дочернего.")
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("shared");

    Components branch(&fixture.registry, &fixture.root, "Branch");
    branch.add<TestComponent>("shared");

    branch.remove<TestComponent>("shared");

    REQUIRE(!branch.find<TestComponent>("shared").exists());
    REQUIRE(fixture.root.find<TestComponent>("shared").exists());
}

TEST(Components_RemoveShadowDoesNotRevealWrongComponent, RuntimeFixture, 
"После удаления локального компонента поиск должен корректно продолжить поиск у родителя. \
Удаление индекса дочернего компонента не должно повреждать или скрывать родительский объект.")
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("shared");

    Components branch(&fixture.registry, &fixture.root, "Branch");
    branch.add<TestComponent>("shared");

    REQUIRE(branch.find<TestComponent>("shared").exists());

    branch.remove<TestComponent>("shared");

    REQUIRE(branch.find<TestComponent>("shared").exists());
}

TEST(Components_ConfigureDeepTree, RuntimeFixture, 
"configureAll должен вызвать configure для каждого компонента во всей ветке.\
Вызов должен корректно проходить через произвольную глубину дерева.") 
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Components branch(&fixture.registry, &fixture.root, "Branch");
    branch.add<TestComponent>("branch");

    Components child(&fixture.registry, &branch, "Child");
    child.add<TestComponent>("child");

    fixture.root.configureAll();

    REQUIRE(fixture.root.require<TestComponent>("root")->configured);
    REQUIRE(branch.require<TestComponent>("branch")->configured);
    REQUIRE(child.require<TestComponent>("child")->configured);
}

TEST(Components_ConfigureDoesNotConfigureTwice, RuntimeFixture, 
"Повторный вызов configureAll не должен приводить к неконтролируемому состоянию компонента. \
Компонент должен сохранять корректное сконфигурированное состояние.")
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    auto component = fixture.root.require<TestComponent>();

    fixture.root.configureAll();

    REQUIRE(component->configured);

    fixture.root.configureAll();

    REQUIRE(component->configured);
}

}
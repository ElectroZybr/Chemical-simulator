#include <Lattice/Tools/Fixture.hpp>
#include <Lattice/Tools/Tests.hpp>

#include <Lattice/Lattice.hpp>


namespace Lattice {

class TestComponent {
public:
    int value = 0;
    bool configured = false;

    void configure(Node&) {
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


TEST(Node_DeepTreeLookup, RuntimeFixture, 
"Поиск компонента должен подниматься по дереву родителей, но не заходить в соседние ветки. \
Child-ветка должна видеть свои компоненты и компоненты предков.")
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branchA = fixture.root.addFolder("BranchA");
    branchA.add<TestComponent>("a");

    Node& branchB = fixture.root.addFolder("BranchB");
    branchB.add<TestComponent>("b");

    Node& branchAChild = branchA.addFolder("BranchAChild");
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

TEST(Node_Shadowing, RuntimeFixture, 
"Компонент в дочерней ветке должен скрывать компонент с тем же именем из родительской ветки. \
При этом оба объекта должны оставаться независимыми экземплярами.")
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("shared");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("shared");

    auto rootComponent = fixture.root.require<TestComponent>("shared");
    auto branchComponent = branch.require<TestComponent>("shared");

    REQUIRE(rootComponent);
    REQUIRE(branchComponent);
    REQUIRE(&rootComponent.get() != &branchComponent.get());
}

TEST(Node_ShadowingDoesNotLeak, RuntimeFixture, 
"Одинаковые имена компонентов в соседних ветках не должны влиять друг на друга. \
Поиск из одной ветки не должен случайно находить локальный компонент другой ветки.")
 {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("shared");

    Node& branchA = fixture.root.addFolder("A");
    Node& branchB = fixture.root.addFolder("B");

    branchA.add<TestComponent>("shared");

    auto a = branchA.find<TestComponent>("shared");
    auto b = branchB.find<TestComponent>("shared");

    REQUIRE(a.exists());
    REQUIRE(b.exists());
    REQUIRE(a.get() != b.get());
}

TEST(Node_folderCollect, RuntimeFixture,
    "Поиск в папке должен возвращать компоненты из текущей папки и всех вложенных папок.") {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("a");
    branch.add<TestComponent>("b");

    Node& child = branch.addFolder("child");
    child.add<TestComponent>("c");

    Node& nested = child.addFolder("nested");
    nested.add<TestComponent>("d");

    auto root = fixture.root.folderCollect<TestComponent>();
    auto branchNode = branch.folderCollect<TestComponent>();
    auto childNode = child.folderCollect<TestComponent>();
    auto nestedNode = nested.folderCollect<TestComponent>();

    REQUIRE(root.size() == 5);
    REQUIRE(branchNode.size() == 4);
    REQUIRE(childNode.size() == 2);
    REQUIRE(nestedNode.size() == 1);
}


TEST(Node_directCollect, RuntimeFixture,
    "Поиск в папке должен возвращать только компоненты непосредственно принадлежащие текущей папке.") {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("a");
    branch.add<TestComponent>("b");

    Node& child = branch.addFolder("child");
    child.add<TestComponent>("c");

    Node& nested = child.addFolder("nested");
    nested.add<TestComponent>("d");

    auto root = fixture.root.directCollect<TestComponent>();
    auto branchNode = branch.directCollect<TestComponent>();
    auto childNode = child.directCollect<TestComponent>();
    auto nestedNode = nested.directCollect<TestComponent>();

    REQUIRE(root.size() == 1);
    REQUIRE(branchNode.size() == 2);
    REQUIRE(childNode.size() == 1);
    REQUIRE(nestedNode.size() == 1);
}

TEST(Node_GlobalCollectDeepTree, RuntimeFixture,
    "Глобальный поиск должен обходить всё дерево компонентов независимо от глубины вложенности. "
    "В результат должны попасть компоненты из всех веток и дочерних узлов.") {

    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branchA = fixture.root.addFolder("A");
    branchA.add<TestComponent>("a");

    Node& branchB = fixture.root.addFolder("B");
    branchB.add<TestComponent>("b");

    Node& childA = branchA.addFolder("ChildA");
    childA.add<TestComponent>("aa");

    Node& childB = branchB.addFolder("ChildB");
    childB.add<TestComponent>("bb");

    auto Node = childA.globalCollect<TestComponent>();

    REQUIRE(Node.size() == 5);
}

TEST(Node_GlobalCollectIgnoresInstanceName, RuntimeFixture, 
"Глобальный поиск должен находить все экземпляры компонента независимо от имени реализации.")
 {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("one");
    fixture.root.add<TestComponent>("two");
    fixture.root.add<TestComponent>("three");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("four");
    branch.add<TestComponent>("five");

    auto Node = fixture.root.globalCollect<TestComponent>();

    REQUIRE(Node.size() == 5);
}

TEST(Node_RemoveDoesNotAffectParent, RuntimeFixture,
    "Удаление компонента из дочерней ветки не должно удалять компонент родителя.") {

    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("shared");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("shared");

    branch.remove<TestComponent>("shared");
    REQUIRE(branch.folderCollect<TestComponent>().empty());
    REQUIRE(fixture.root.find<TestComponent>("shared").exists());
}

TEST(Node_RemoveShadowDoesNotRevealWrongComponent, RuntimeFixture, 
"После удаления локального компонента поиск должен корректно продолжить поиск у родителя. \
Удаление индекса дочернего компонента не должно повреждать или скрывать родительский объект.")
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("shared");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("shared");

    REQUIRE(branch.find<TestComponent>("shared").exists());

    branch.remove<TestComponent>("shared");

    REQUIRE(branch.find<TestComponent>("shared").exists());
}

TEST(Node_ConfigureDeepTree, RuntimeFixture, 
"configureAll должен вызвать configure для каждого компонента во всей ветке.\
Вызов должен корректно проходить через произвольную глубину дерева.") 
{
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("branch");

    Node& child = fixture.root.addFolder("Child");
    child.add<TestComponent>("child");

    fixture.root.configureAll();

    REQUIRE(fixture.root.require<TestComponent>("root")->configured);
    REQUIRE(branch.require<TestComponent>("branch")->configured);
    REQUIRE(child.require<TestComponent>("child")->configured);
}

TEST(Node_ConfigureDoesNotConfigureTwice, RuntimeFixture, 
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
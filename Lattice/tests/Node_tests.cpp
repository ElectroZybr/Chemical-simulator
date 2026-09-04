#include <Lattice/Tools/Fixture.hpp>
#include <Lattice/Tools/Tests.hpp>

#include <Lattice/Lattice.hpp>


namespace Lattice {

class TestComponent {
public:
    bool configured = false;

    void configure(Node&) {
        configured = true;
    }
};

TEST(Node_Add, RuntimeFixture) {
    REQUIRE(!fixture.root.find<Settings>().exists());

    fixture.root.add<Settings>();

    REQUIRE(fixture.root.find<Settings>().exists());
    REQUIRE(fixture.root.require<Settings>().exists());
}

TEST(Node_AddDuplicate, RuntimeFixture) {
    fixture.root.add<Settings>();
    fixture.root.add<Settings>();

    auto settings = fixture.root.globalCollect<Settings>();

    REQUIRE(settings.size() == 1);
}

TEST(Node_CustomInstance, RuntimeFixture) {
    fixture.root.add<Settings>("custom");

    REQUIRE(fixture.root.find<Settings>("custom").exists());
    REQUIRE(!fixture.root.find<Settings>("default").exists());
}

TEST(Node_InstanceIsolation, RuntimeFixture) {
    fixture.root.add<Settings>("first");
    fixture.root.add<Settings>("second");

    REQUIRE(fixture.root.find<Settings>("first").exists());
    REQUIRE(fixture.root.find<Settings>("second").exists());
    REQUIRE(!fixture.root.find<Settings>("default").exists());

    auto settings = fixture.root.globalCollect<Settings>();

    REQUIRE(settings.size() == 2);
}

TEST(Node_RequireMissing, RuntimeFixture) {
    bool thrown = false;

    try {
        fixture.root.require<Settings>();
    } catch (const Exception&) {
        thrown = true;
    }

    REQUIRE(thrown);
}

TEST(Node_RegisterAndAdd, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    REQUIRE(fixture.root.find<TestComponent>().exists());
    REQUIRE(fixture.root.require<TestComponent>().exists());
}

TEST(Node_Configure, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();
    fixture.root.add<TestComponent>();

    auto component = fixture.root.require<TestComponent>();

    REQUIRE(!component->configured);

    fixture.root.configureAll();

    REQUIRE(component->configured);
}

TEST(Node_GlobalCollect, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("first");
    fixture.root.add<TestComponent>("second");

    auto Node = fixture.root.globalCollect<TestComponent>();

    REQUIRE(Node.size() == 2);
}

TEST(Node_folderCollect, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branch = fixture.root;

    branch.add<TestComponent>("another");

    auto Node = branch.folderCollect<TestComponent>();

    REQUIRE(Node.size() == 2);
}

TEST(Node_ChildVisibility, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    auto child = fixture.root.find<TestComponent>();

    REQUIRE(child.exists());
}

TEST(Node_ParentLookup, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    Node& branch = fixture.root.addFolder("branch");

    REQUIRE(branch.find<TestComponent>().exists());
    REQUIRE(branch.require<TestComponent>().exists());
}

TEST(Node_Shadowing, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>();

    auto parent = fixture.root.find<TestComponent>();
    auto child = branch.find<TestComponent>();

    REQUIRE(parent.exists());
    REQUIRE(child.exists());

    // REQUIRE(parent.data != child.data);
}

TEST(Node_ChildInstanceLookup, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("child");

    REQUIRE(branch.find<TestComponent>("child").exists());
    REQUIRE(branch.find<TestComponent>("root").exists());
    REQUIRE(!branch.find<TestComponent>("missing").exists());
}

TEST(Node_GlobalCollectNested, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("child");

    auto Node = branch.globalCollect<TestComponent>();

    REQUIRE(Node.size() == 2);
}

TEST(Node_folderCollectNested, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Node& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("child");

    auto Node = branch.folderCollect<TestComponent>();

    REQUIRE(Node.size() == 1);
}

TEST(Node_Remove, RuntimeFixture) {
    fixture.root.add<Settings>();

    REQUIRE(fixture.root.find<Settings>().exists());

    fixture.root.remove<Settings>();

    REQUIRE(!fixture.root.find<Settings>().exists());
}

TEST(Node_RemoveInstance, RuntimeFixture) {
    fixture.root.add<Settings>("first");
    fixture.root.add<Settings>("second");

    fixture.root.remove<Settings>("first");

    REQUIRE(!fixture.root.find<Settings>("first").exists());
    REQUIRE(fixture.root.find<Settings>("second").exists());

    auto settings = fixture.root.globalCollect<Settings>();

    REQUIRE(settings.size() == 1);
}

TEST(Node_RemoveMissing, RuntimeFixture) {
    fixture.root.add<Settings>();

    fixture.root.remove<Settings>("missing");

    REQUIRE(fixture.root.find<Settings>().exists());
}

}
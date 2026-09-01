#include <Lattice/Tools/Fixture.hpp>
#include <Lattice/Tools/Tests.hpp>

#include <Lattice/Lattice.hpp>


namespace Lattice {

class TestComponent {
public:
    bool configured = false;

    void configure(Components&) {
        configured = true;
    }
};

TEST(Components_Add, RuntimeFixture) {
    REQUIRE(!fixture.root.find<Settings>().exists());

    fixture.root.add<Settings>();

    REQUIRE(fixture.root.find<Settings>().exists());
    REQUIRE(fixture.root.require<Settings>().exists());
}

TEST(Components_AddDuplicate, RuntimeFixture) {
    fixture.root.add<Settings>();
    fixture.root.add<Settings>();

    auto settings = fixture.root.globalCollect<Settings>();

    REQUIRE(settings.size() == 1);
}

TEST(Components_CustomInstance, RuntimeFixture) {
    fixture.root.add<Settings>("custom");

    REQUIRE(fixture.root.find<Settings>("custom").exists());
    REQUIRE(!fixture.root.find<Settings>("default").exists());
}

TEST(Components_InstanceIsolation, RuntimeFixture) {
    fixture.root.add<Settings>("first");
    fixture.root.add<Settings>("second");

    REQUIRE(fixture.root.find<Settings>("first").exists());
    REQUIRE(fixture.root.find<Settings>("second").exists());
    REQUIRE(!fixture.root.find<Settings>("default").exists());

    auto settings = fixture.root.globalCollect<Settings>();

    REQUIRE(settings.size() == 2);
}

TEST(Components_RequireMissing, RuntimeFixture) {
    bool thrown = false;

    try {
        fixture.root.require<Settings>();
    } catch (const Exception&) {
        thrown = true;
    }

    REQUIRE(thrown);
}

TEST(Components_RegisterAndAdd, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    REQUIRE(fixture.root.find<TestComponent>().exists());
    REQUIRE(fixture.root.require<TestComponent>().exists());
}

TEST(Components_Configure, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();
    fixture.root.add<TestComponent>();

    auto component = fixture.root.require<TestComponent>();

    REQUIRE(!component->configured);

    fixture.root.configureAll();

    REQUIRE(component->configured);
}

TEST(Components_GlobalCollect, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("first");
    fixture.root.add<TestComponent>("second");

    auto components = fixture.root.globalCollect<TestComponent>();

    REQUIRE(components.size() == 2);
}

TEST(Components_folderCollect, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Components& branch = fixture.root;

    branch.add<TestComponent>("another");

    auto components = branch.folderCollect<TestComponent>();

    REQUIRE(components.size() == 2);
}

TEST(Components_ChildVisibility, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    auto child = fixture.root.find<TestComponent>();

    REQUIRE(child.exists());
}

TEST(Components_ParentLookup, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    Components& branch = fixture.root.addFolder("branch");

    REQUIRE(branch.find<TestComponent>().exists());
    REQUIRE(branch.require<TestComponent>().exists());
}

TEST(Components_Shadowing, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>();

    Components& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>();

    auto parent = fixture.root.find<TestComponent>();
    auto child = branch.find<TestComponent>();

    REQUIRE(parent.exists());
    REQUIRE(child.exists());

    REQUIRE(parent.data != child.data);
}

TEST(Components_ChildInstanceLookup, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Components& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("child");

    REQUIRE(branch.find<TestComponent>("child").exists());
    REQUIRE(branch.find<TestComponent>("root").exists());
    REQUIRE(!branch.find<TestComponent>("missing").exists());
}

TEST(Components_GlobalCollectNested, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Components& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("child");

    auto components = branch.globalCollect<TestComponent>();

    REQUIRE(components.size() == 2);
}

TEST(Components_folderCollectNested, RuntimeFixture) {
    fixture.registry.registerComponent<TestComponent>();

    fixture.root.add<TestComponent>("root");

    Components& branch = fixture.root.addFolder("branch");
    branch.add<TestComponent>("child");

    auto components = branch.folderCollect<TestComponent>();

    REQUIRE(components.size() == 1);
}

TEST(Components_Remove, RuntimeFixture) {
    fixture.root.add<Settings>();

    REQUIRE(fixture.root.find<Settings>().exists());

    fixture.root.remove<Settings>();

    REQUIRE(!fixture.root.find<Settings>().exists());
}

TEST(Components_RemoveInstance, RuntimeFixture) {
    fixture.root.add<Settings>("first");
    fixture.root.add<Settings>("second");

    fixture.root.remove<Settings>("first");

    REQUIRE(!fixture.root.find<Settings>("first").exists());
    REQUIRE(fixture.root.find<Settings>("second").exists());

    auto settings = fixture.root.globalCollect<Settings>();

    REQUIRE(settings.size() == 1);
}

TEST(Components_RemoveMissing, RuntimeFixture) {
    fixture.root.add<Settings>();

    fixture.root.remove<Settings>("missing");

    REQUIRE(fixture.root.find<Settings>().exists());
}

}
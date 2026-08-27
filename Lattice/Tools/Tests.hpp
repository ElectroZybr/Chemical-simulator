#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <utility>


#define TEST2(name, Fixture) \
    TEST3(name, Fixture, "")

#define TEST3(name, Fixture, description) \
    static void name(Fixture& fixture); \
    static std::unique_ptr<::Lattice::TestFixture> _fixture_##name() { \
        return std::make_unique<Fixture>(); \
    } \
    static void _run_##name(::Lattice::TestFixture& fixture) { \
        name(static_cast<Fixture&>(fixture)); \
    } \
    static ::Lattice::TestRegistrar _test_##name(#name, description, _run_##name, _fixture_##name); \
    static void name(Fixture& fixture)

#define TEST_SELECT(_1, _2, _3, NAME, ...) NAME
#define TEST(...) TEST_SELECT(__VA_ARGS__, TEST3, TEST2)(__VA_ARGS__)

#define REQUIRE(condition) \
    ::Lattice::testRequire(static_cast<bool>(condition), #condition, __FILE__, __LINE__)

#define CHECK(condition) \
    ::Lattice::testCheck(static_cast<bool>(condition), #condition, __FILE__, __LINE__)


namespace Lattice {

class TestFixture {
public:
    virtual ~TestFixture() = default;
};

struct TestCase {
    std::string name;
    std::string description;
    void (*function)(TestFixture&);
    std::unique_ptr<TestFixture> (*createFixture)();
};

class TestRegistry {
public:
    static TestRegistry& instance();

    void add(TestCase test) {
        tests_.push_back(std::move(test));
    }

    const std::vector<TestCase>& tests() const {
        return tests_;
    }

    int runAll();

private:
    std::vector<TestCase> tests_;
};

class TestRegistrar {
public:
    TestRegistrar(
        std::string_view name,
        std::string_view description,
        void (*function)(TestFixture&),
        std::unique_ptr<TestFixture> (*createFixture)()
    ) {
        TestRegistry::instance().add({
            std::string(name),
            std::string(description),
            function,
            createFixture
        });
    }
};

void testRequire(bool condition, const char* expression, const char* file, int line);
void testCheck(bool condition, const char* expression, const char* file, int line);

} // namespace Lattice
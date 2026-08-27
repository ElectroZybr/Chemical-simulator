#include <Lattice/Tools/Tests.hpp>
#include <Lattice/Tools/Logger.hpp>
#include "Lattice/Tools/LogStyle.hpp"


namespace Lattice {

namespace {
struct TestFailure : std::exception {
    const char* what() const noexcept override {
        return "test requirement failed";
    }
};

thread_local bool currentTestFailed = false;
}

void testRequire(bool condition, const char* expression, const char* file, int line) {
    if (condition)
        return;

    currentTestFailed = true;
    Logger::warning("Test", "REQUIRE failed: {} ({}:{})", expression, file, line);
    throw TestFailure{};
}

void testCheck(bool condition, const char* expression, const char* file, int line) {
    if (condition)
        return;

    currentTestFailed = true;
    Logger::warning("Test", "CHECK failed: {} ({}:{})", expression, file, line);
}

TestRegistry& TestRegistry::instance() {
    static TestRegistry registry;
    return registry;
}

int TestRegistry::runAll() {
    int failed = 0;
    Logger::message("\n{}~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{}", Color::white, Color::reset);
    Logger::Scope testing("Tests", "{}{}Running", Color::white, Color::bold);
    for (TestCase& test : tests_) {
        Logger::message("");
        Logger::Scope testScope("Test", "'{}'", test.name);
        currentTestFailed = false;
        try {
            auto fixture = test.createFixture();
            test.function(*fixture);
        } catch (const TestFailure&) {
        } catch (const std::exception& e) {
            currentTestFailed = true;
            Logger::exception("test", "{}{}threw: {}", Color::brightRed, Color::bold, e.what());
        } catch (...) {
            currentTestFailed = true;
            Logger::exception("test", "{}{}threw unknown exception", Color::brightRed, Color::bold);
        }
        if (currentTestFailed) {
            ++failed;
            testScope.finishError("{}{}'{}' failed", Color::brightRed, Color::bold, test.name);
            Logger::message("{}Err string: {}", Color::brightRed, test.description);
        } else {
            testScope.finish("{}'{}' passed", Color::brightGreen, test.name);
        }
    }
    testing.finish("{} tests, {} failed", tests_.size(), failed);
    return failed;
}

} // namespace Lattice
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
    Logger::warning("Test", "<y>REQUIRE failed: {} ({}:{})</>", expression, file, line);
    throw TestFailure{};
}

void testCheck(bool condition, const char* expression, const char* file, int line) {
    if (condition)
        return;

    currentTestFailed = true;
    Logger::warning("Test", "<y>CHECK failed: {} ({}:{})</>", expression, file, line);
}

TestRegistry& TestRegistry::instance() {
    static TestRegistry registry;
    return registry;
}

int TestRegistry::runAll() {
    int failed = 0;
    Logger::message("\n<w>~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~</>");
    Logger::Scope testing("Tests", "<w><b>Running<//>");
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
            Logger::exception("test", "<r><b>threw: {}<//>", e.what());
        } catch (...) {
            currentTestFailed = true;
            Logger::exception("test", "<r><b>threw unknown exception<//>");
        }
        if (currentTestFailed) {
            ++failed;
            testScope.finishError("<r><b>'{}' failed<//>", test.name);
            if (!test.description.empty())
                Logger::message("<r>Err string:\n{}</>", test.description);
        } else {
            testScope.finish("<g><b>'{}' passed<//>", test.name);
        }
    }
    testing.finish("{} tests, {} failed", tests_.size(), failed);
    return failed;
}

} // namespace Lattice
#include <Lattice/Tools/Tests.hpp>
#include <Lattice/Tools/Logger.hpp>


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
    Logger::message("<w>~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~</>");
    Logger::Scope testing("Tests", "<w><b>Running<//>");
    for (TestCase& test : tests_) {
        Logger::Scope testScope("Test", "'{}'", test.name);
        currentTestFailed = false;
        try {
            auto fixture = test.createFixture();
            test.function(*fixture);
        } catch (const TestFailure&) {
        } catch (const std::exception& e) {
            currentTestFailed = true;
            Logger::exception("Test", "<r><b>threw: {}<//>", e.what());
        } catch (...) {
            currentTestFailed = true;
            Logger::exception("Test", "<r><b>threw unknown exception<//>");
        }
        if (currentTestFailed) {
            ++failed;
            if (!test.description.empty()) {
                Logger::warning("Desc", "<r><b>{}<//>", test.description);
            }
            testScope.finishError("<r><b>'{}' failed<//>", test.name);
        } else {
            testScope.finish("<g><b>'{}' passed<//>", test.name);
        }
    }
    if (failed == 0) {
        testing.finish("all {} tests passed", tests_.size());
    } else {
        testing.finishError("<b><g>{} passed,</> {} failure</>", tests_.size()-failed, failed);
    }
    return failed;
}

} // namespace Lattice
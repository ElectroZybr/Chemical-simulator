#include <Lattice/Kernel/Runtime.hpp>
#include <Lattice/Tools/Logger.hpp>
#include <string_view>

int runApplication(int argc, char** argv) {
    Logger::ConsoleMode consoleMode = Logger::ConsoleMode::Trace;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--trace") {
            consoleMode = Logger::ConsoleMode::Trace;
        } else if (arg == "--verbose" || arg == "-v") {
            if (consoleMode != Logger::ConsoleMode::Trace) {
                consoleMode = Logger::ConsoleMode::Verbose;
            }
        }
    }

    Logger::setConsoleMode(consoleMode);
    Lattice::Runtime runtime;
    runtime.loadPlugins("Plugins");
    if (runtime.check("ClassicMD", "Universe 1"))
        runtime.start("ClassicMD", "Universe 1");
    if (runtime.check("Window", "Window"))
        runtime.start("Window", "Window");
    runtime.run();
    return 0;
}

int main(int argc, char** argv) { return runApplication(argc, argv); }

#if defined(_WIN32)
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return runApplication(__argc, __argv); }
#endif

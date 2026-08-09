#include "App/Application.h"
#include "Lattice/Log.hpp"

#include <string_view>

int runApplication(int argc, char** argv) {
    Log::ConsoleMode consoleMode = Log::ConsoleMode::Trace;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--trace") {
            consoleMode = Log::ConsoleMode::Trace;
        } else if (arg == "--verbose" || arg == "-v") {
            if (consoleMode != Log::ConsoleMode::Trace) {
                consoleMode = Log::ConsoleMode::Verbose;
            }
        }
    }

    Log::setConsoleMode(consoleMode);
    Application application;
    return application.run();
}

int main(int argc, char** argv) { return runApplication(argc, argv); }

#if defined(_WIN32)
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return runApplication(__argc, __argv); }
#endif

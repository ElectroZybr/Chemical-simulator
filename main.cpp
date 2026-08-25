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
    runtime.registry().printRegistryTree();
    if (runtime.check("ClassicMD"))
        runtime.start("ClassicMD", "Universe 1");
    
    if (runtime.check("Window"))
        runtime.start("Window", "Window", ServiceLaunch::Host);

    if (runtime.check("ActionMap"))
        runtime.configure("ActionMap");

    if (runtime.check("IOSubsystem"))
        runtime.configure("IOSubsystem");

    runtime.roote()->configureAll();

    // Lattice::ConfigPaths paths;
    // paths.userDir = "User";
    // paths.pluginDirs = { "Plugins" };
    // paths.projectFile = "lattice.toml";

    // Lattice::ConfigLoader loader(std::move(paths));
    // Lattice::Config cfg = loader.load();

    // for (auto& w : cfg.warnings())
    //     Logger::warning("Config", "{}", w);

    // Lattice:: applyBinds(cfg);
    
    runtime.run();
    return 0;
}

int main(int argc, char** argv) { return runApplication(argc, argv); }

#if defined(_WIN32)
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return runApplication(__argc, __argv); }
#endif

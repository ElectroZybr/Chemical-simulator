#include <Lattice/Kernel/Runtime.hpp>
#include <Lattice/Tools/Logger.hpp>
#include <string_view>

int runApplication(int argc, char** argv) {
    Lattice::Runtime runtime;
    runtime.run(argc, argv);
    return 1;
}

int main(int argc, char** argv) { return runApplication(argc, argv); }

#if defined(_WIN32)
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return runApplication(__argc, __argv); }
#endif

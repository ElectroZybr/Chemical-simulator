#include "App/Application.h"

namespace {

int runApplication() {
    Application application;
    return application.run();
}

} // namespace

#ifdef _WIN32
#include <windows.h>

// Исправление бага: Windows-target имеет тип WIN32, поэтому линкер ожидает WinMain.
// Обе точки входа используют runApplication(), чтобы не дублировать логику запуска.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return runApplication();
}
#endif

int main() {
    return runApplication();
}

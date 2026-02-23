#include <SFML/Graphics.hpp>
#include <SFML/Window/WindowHandle.hpp>
#include <cmath>

#include <iostream>
#include <functional>
#include <memory>

#include "imgui.h"
#include "imgui-SFML.h"

#include "engine/physics/Atom.h"
#include "engine/physics/SpatialGrid.h"
#include "interface.h"

#include "engine/Simulation.h"

#define WIGHT   800
#define HEIGHT  600

#define FPS  60
#define LPS  10
#define Dt  0.01

#include <windows.h>
#include <vector>

struct MonitorInfo {
    RECT rect;  
};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM data) {
    auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(data);
    monitors->push_back({ *rect });
    return TRUE;
}

std::vector<MonitorInfo> getMonitors() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&monitors);
    return monitors;
}

int main() {
    sf::RenderWindow window(sf::VideoMode(WIGHT, HEIGHT), "Chemical-simulator");

    sf::Image icon;
    icon.loadFromFile("icon.png");
    window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

    // получить список мониторов
    auto monitors = getMonitors();

    // выброр монитора №1
    int targetMonitor = 1;

    if (targetMonitor < monitors.size()) {
        HWND hwnd = window.getSystemHandle();
        RECT r = monitors[targetMonitor].rect;
        SetWindowPos(hwnd, HWND_TOP, r.left, r.top, 0, 0, SWP_NOZORDER);
        ShowWindow(hwnd, SW_MAXIMIZE);
    }

    SpatialGrid grid(100, 100);
    Simulation simulation(window, grid);
    simulation.setCameraPos(50, 50);

    // simulation.drawGrid(true);
    simulation.drawBonds(true);

    // Atom* hydrogen_1 = simulation.createAtom(Vec3D(50.5, 50.86, 1), Vec3D(2, 0, 0), 1);
    // Atom* oxygen_1 = simulation.createAtom(Vec3D(50, 50, 1), Vec3D(0, 0, 0), 8);
    // Atom* hydrogen_2 = simulation.createAtom(Vec3D(51, 50, 1), Vec3D(0, 0, 0), 1);

    // simulation.addBond(hydrogen_1, oxygen_1);
    // simulation.addBond(hydrogen_2, oxygen_1);

    simulation.createRandomAtoms(1, 200);
    simulation.createRandomAtoms(8, 100);
    
    sf::Clock clock;
    double shotTmr;
    double simTmr;
    double logTmr;
    
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();

        simTmr += deltaTime;
        if (simTmr >= Dt/Interface::getSimulationSpeed()) {
            simulation.update(Dt);
            simTmr = 0;
        }

        shotTmr += deltaTime;
        if (shotTmr >= 1./FPS) {
            simulation.event();
            simulation.renderShot(shotTmr);
            shotTmr = 0;
        }

        logTmr += deltaTime;
        if (logTmr >= 1./LPS) {
            // simulation.logEnergies();
            // simulation.logAtomPos();
            // simulation.logMousePos();
            // simulation.logBondList();
            logTmr = 0;
        }
    }
    ImGui::SFML::Shutdown();
    
    return 0;
}
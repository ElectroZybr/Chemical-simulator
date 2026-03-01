#include <SFML/Graphics.hpp>
#include <SFML/Window/WindowHandle.hpp>
#include <cmath>

#include <iostream>
#include <functional>
#include <memory>
#include <chrono>

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

#include <vector>

int main() {
    sf::RenderWindow window(sf::VideoMode(WIGHT, HEIGHT), "Chemical-simulator");

    sf::Image icon;
    icon.loadFromFile("icon.png");
    window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

    // SpatialGrid grid(50, 50);
    Simulation simulation(window, 50, 100);
    simulation.setCameraPos(25, 25);

    // simulation.drawGrid(true);
    simulation.drawBonds(true);

    // Atom* hydrogen_1 = simulation.createAtom(Vec3D(50.5, 50.86, 1), Vec3D(2, 0, 0), 1);
    // Atom* oxygen_1 = simulation.createAtom(Vec3D(50, 50, 1), Vec3D(0, 0, 0), 8);
    // Atom* hydrogen_2 = simulation.createAtom(Vec3D(51, 50, 1), Vec3D(0, 0, 0), 1);

    // simulation.addBond(hydrogen_1, oxygen_1);
    // simulation.addBond(hydrogen_2, oxygen_1);

    for (int i = 0; i <= 15; i++) {
        for (int j = 0; j <= 15; j++) {
            simulation.createAtom(Vec3D(3+i*3, 3+j*3, 1), Vec3D(0, 0, 0), 1);
        }
    }

    // auto start = std::chrono::high_resolution_clock::now();

    // for (int i = 0; i < 100000; ++i) {
    //     hydrogen_1->SoftWalls(0.1);
    // }

    // auto end = std::chrono::high_resolution_clock::now();

    // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // std::cout << "Time: " << duration.count() << " microseconds\n";
    

    // simulation.createRandomAtoms(1, 200);
    // simulation.createRandomAtoms(8, 100);
    
    sf::Clock clock;
    double shotTmr;
    double simTmr;
    double logTmr;
    
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();

        simTmr += deltaTime;
        if (simTmr >= Dt/Interface::getSimulationSpeed()) {
            // auto start = std::chrono::high_resolution_clock::now();
            simulation.update(Dt);
            // auto end = std::chrono::high_resolution_clock::now();
            // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            // std::cout << "Time: " << duration.count() << " microseconds\n";
            simTmr = 0;
        }

        shotTmr += deltaTime;
        if (shotTmr >= 1./FPS) {
            // auto start = std::chrono::high_resolution_clock::now();
            simulation.event();
            simulation.renderShot(shotTmr);
            shotTmr = 0;
            // auto end = std::chrono::high_resolution_clock::now();
            // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            // std::cout << "Time: " << duration.count() << " microseconds\n";
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
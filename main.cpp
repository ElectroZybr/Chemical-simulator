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
#define LPS  30
#define Dt  0.01

#include <vector>

int main() {
    // sf::RenderWindow window(sf::VideoMode(WIGHT, HEIGHT), "Chemical-simulator");
    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "Chemical-simulator", sf::Style::Fullscreen);

    sf::Image icon;
    icon.loadFromFile("icon.png");
    window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

    SimBox box(Vec3D(0, 0, 0), Vec3D(100, 100, 3));
    Simulation simulation(window, box);
    simulation.setCameraPos(25, 25);
    simulation.setCameraZoom(20);
    // simulation.sim_box.setSizeBox(Vec3D(0, 0, 0), Vec3D(50, 50.5, 3));

    // simulation.drawGrid(true);
    simulation.drawBonds(true);

    // Atom* hydrogen_1 = simulation.createAtom(Vec3D(50.5, 50.86, 1), Vec3D(2, 0, 0), 1);
    // Atom* oxygen_1 = simulation.createAtom(Vec3D(50, 50, 1), Vec3D(0, 0, 0), 8);
    // Atom* hydrogen_2 = simulation.createAtom(Vec3D(51, 50, 1), Vec3D(0, 0, 0), 1);

    // simulation.addBond(hydrogen_1, oxygen_1);
    // simulation.addBond(hydrogen_2, oxygen_1);

    for (int i = 0; i <= 30; i++) {
        for (int j = 0; j <= 30; j++) {
            simulation.createAtom(Vec3D(3+i*3, 3+j*3, 1), randomUnitVector3D(0.5), 1);
        }
    }

    // for (int i = 0; i <= 15; i++) {
    //     for (int j = 0; j <= 15; j++) {
    //         simulation.createAtom(Vec3D(3+i*3, 3+j*3, 4), randomUnitVector3D(0.5), 1);
    //     }
    // }


    // auto start = std::chrono::high_resolution_clock::now();

    // for (int i = 0; i < 100000; ++i) {
    //     hydrogen_1->SoftWalls(0.1);
    // }

    // simulation.createRandomAtoms(1, 100);
    // simulation.createRandomAtoms(8, 100);
    
    sf::Clock clock;
    double shotTmr = 0.0;
    double simTmr = 0.0;
    double logTmr = 0.0;

    auto duration = std::chrono::microseconds::zero();
    int i = 0;
    int sim_step = 0;
    
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();

        simTmr += deltaTime;
        if (simTmr >= Dt/Interface::getSimulationSpeed()) {
            simulation.update(Dt);
            if (!Interface::getPause()) {
                // if (sim_step < 5000)
                //     simulation.setSizeBox(
                //         Vec3D(simulation.sim_box.start.x+0.001, simulation.sim_box.start.y+0.001, simulation.sim_box.start.z),
                //         Vec3D(simulation.sim_box.end.x-0.001, simulation.sim_box.end.y-0.001, simulation.sim_box.end.z));

                if (sim_step > 10000 && sim_step < 100000)
                    simulation.setSizeBox(
                        Vec3D(simulation.sim_box.start.x-0.001, simulation.sim_box.start.y-0.001, simulation.sim_box.start.z),
                        Vec3D(simulation.sim_box.end.x+0.001, simulation.sim_box.end.y+0.001, simulation.sim_box.end.z));
                simTmr = 0;
                sim_step++;
            }
        }

        shotTmr += deltaTime;
        if (shotTmr >= 1./FPS) {
            simulation.event();
            auto start = std::chrono::high_resolution_clock::now();
            simulation.renderShot(shotTmr);
            shotTmr = 0;
            auto end = std::chrono::high_resolution_clock::now();
            duration += std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            i++;
        }

        logTmr += deltaTime;
        if (logTmr >= 1./LPS) {
            std::cout << "AverageTime: " << duration.count()/i << " microseconds\n";
            duration = std::chrono::microseconds::zero();
            i = 0;
            // std::cout << "Zoom: " << simulation.render.camera.getZoom() << "\n";
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

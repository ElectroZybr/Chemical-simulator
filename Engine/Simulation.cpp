#include "Simulation.h"
#include "../interface.h"
#include "Tools.h"
#include <cmath>
#include <algorithm>

#include "imgui.h"
#include "imgui-SFML.h"

Simulation::Simulation(sf::RenderWindow& w, SpatialGrid& gr)
    : window(w), gameView(window.getDefaultView()), uiView(window.getDefaultView()), grid(gr), render(w, gameView, uiView) 
    {
        Interface::init(window);
        Tools::init(&window, &gameView, &render, &grid);
        Atom::setGrid(&grid);

        // резервируем место под создание атомов
        atoms.reserve(50000);

        int x_size = 2000, y_size = 2000;
        double k = 400;
        k /= x_size;

        std::vector<sf::Uint8> forcePixels(x_size * y_size * 4);
        for (int x = 0; x < x_size; x++) {
            for (int y = 0; y < y_size; y++) {
                // float v = float(x) / grid.sizeX;  // 0..1

                int idx = 4 * (y * x_size + x);

                forcePixels[idx + 0] = 255; // R
                forcePixels[idx + 1] = 0;                 // G
                forcePixels[idx + 2] = 0;                 // B
                forcePixels[idx + 3] = 0;               // A

                // расстояния до краёв
                int distL = x;
                int distR = (x_size - 1) - x;

                int distT = y;
                int distB = (y_size - 1) - y;

                // ширина зоны силы
                int border = x_size / 50;

                int alpha = 0;
                if (distL < border)
                    alpha += k * pow(distL - border, 2);

                if (distR < border)
                    alpha += k * pow(distR - border, 2);

                if (distT < border)
                    alpha += k * pow(distT - border, 2);

                if (distB < border)
                    alpha += k * pow(distB - border, 2);
                
                if (alpha > 255) alpha = 255;
                forcePixels[idx + 3] = (sf::Uint8)alpha;
            }
        }
        render.forceTexture.create(x_size, y_size);
        render.forceTexture.update(forcePixels.data());
    }

void Simulation::update(float dt) {
    if (!Interface::getPause()) {
            for (Atom& atom : atoms)
                atom.PredictPosition(dt);
            for (Atom& atom : atoms)
                atom.ComputeForces(dt);
            for (auto it = Bond::bonds_list.begin(); it != Bond::bonds_list.end(); ) {
                if (it->shouldBreak()) {
                    it->detach();
                    it = Bond::bonds_list.erase(it);
                } else {
                    ++it;
                }
            }
            for (Bond& bond : Bond::bonds_list)
                bond.forceBond(dt);
            for (Atom& atom : atoms)
                atom.CorrectVelosity(dt);
        }
}

void Simulation::renderShot(float deltaTime) {
    render.drawShot(atoms, grid, deltaTime);
}

void Simulation::event() {
    Interface::Update();
    sf::Vector2i mouse_pos = sf::Mouse::getPosition(window);
    sf::Event event;
    while (window.pollEvent(event)) {
        ImGui::SFML::ProcessEvent(window, event);

        if (event.type == sf::Event::Closed)
            window.close();

        if (event.type == sf::Event::Resized) {
            uiView.setSize(event.size.width, event.size.height);
            uiView.setCenter(event.size.width/2, event.size.height/2);
            // ImGui::GetIO().DisplaySize = ImVec2(event.size.width, event.size.height);
        }

        Interface::CheckEvent(event);

        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            float zoom = render.camera.getZoom();
            // создание атома
            if (!Interface::cursorHovered && Interface::getSelectedAtom() != -1) {
                
                atoms.emplace_back(Vec3D(Tools::screenToWorld(mouse_pos, zoom)-0.5),
                                   Vec3D(randomUnitVector3D() * randomInRange(1)), Interface::getSelectedAtom());


                Atom& atom = atoms.back();

                std::cout << "<Create atom> X: " << ((mouse_pos.x - (window.getSize().x / 2.f)) / zoom) + gameView.getCenter().x << 
                                            " Y: " << ((mouse_pos.y - (window.getSize().y / 2.f)) / zoom) + gameView.getCenter().y << 
                                        " | Type: " << Interface::getSelectedAtom() << 
                                    " | Adress: " << &atom << 
                                    " | Speed: " << atom.speed.x << " " << atom.speed.y << std::endl;

            } else {
                // Передвижение атома мышкой
                Vec2D world = Tools::screenToWorld(mouse_pos, zoom) - 0.5;
                std::unordered_set<Atom*>* block = grid.at(
                    grid.worldToCellX(world.x),
                    grid.worldToCellY(world.y)
                );

                if (block != nullptr && !block->empty() && !selectionFrameMoveFlag) {
                    selectedMoveAtom = *block->begin();
                    atomMoveFlag = true; 
                } else if (!Interface::cursorHovered && !selectionFrameMoveFlag) {
                    selectionFrameMoveFlag = true;
                    start_mouse_pos = sf::Mouse::getPosition(window);
                    Tools::selectionFrame(start_mouse_pos, sf::Mouse::getPosition(window), atoms);
                    render.drawSelectionFrame = true;
                }
            } 
        } if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
            atomMoveFlag = false;
            selectionFrameMoveFlag = false;
            render.drawSelectionFrame = false;
            Interface::drawToolTrip = false;
        } 
        render.camera.handleEvent(event, window);
    }

    if (selectionFrameMoveFlag) {
        Tools::selectionFrame(start_mouse_pos, sf::Mouse::getPosition(window), atoms);
    } 

    // Передвижение атома мышкой
    if (atomMoveFlag) {
        float zoom = render.camera.getZoom();
        selectedMoveAtom->coords.x = ((mouse_pos.x - (window.getSize().x / 2.f)) / zoom) + gameView.getCenter().x-0.5;
        selectedMoveAtom->coords.y = ((mouse_pos.y - (window.getSize().y / 2.f)) / zoom) + gameView.getCenter().y-0.5;
    }    
}

void Simulation::createRandomAtoms(int type, int quantity) {
    for (int i = 0; i < quantity; ++i)
        createAtom(Vec3D(std::rand() % grid.sizeX, std::rand() % grid.sizeY, 0), randomUnitVector3D(), type);
}

Atom* Simulation::createAtom(Vec3D start_coords, Vec3D start_speed, int type, bool fixed) {
    atoms.emplace_back(start_coords, start_speed, type, fixed);
    return &atoms.back();
}

void Simulation::addBond(Atom* a1, Atom* a2) {
    Bond::CreateBond(a1, a2);
}

double Simulation::AverageTemp() {
    // double T;
    // for (Atom& atom : atoms)
    //     T += atom.speed.length();
    // return T / atoms.size();
}

void Simulation::logEnergies() {
    float KE = 0.0f;
    float PE = 0.0f;

    // Кинетическая энергия
    for (Atom& atom : atoms) {
        KE += atom.kineticEnergy();
    }

    // Потенциальная энергия (уникальные пары) [по сетке]
    for (Atom& atom : atoms) {
        int curr_x = grid.worldToCellX(atom.coords.x), curr_y = grid.worldToCellY(atom.coords.y);
        int range = grid.worldRadiusToCellRange(2.0);
        for (int i = -range; i <= range; ++i) {
            for (int j = -range; j <= range; ++j) {
                if (auto cell = grid.at(curr_x-i, curr_y-j)) {
                    for (Atom* other : *cell) {
                        if(other <= &atom) continue;
                        PE += atom.pairPotentialEnergy(other);
                    }
                }
            }
        }
    }

    // for (size_t i = 0; i < atoms.size(); ++i) {
    //     for (size_t j = i + 1; j < atoms.size(); ++j) {
    //         PE += atoms[i].pairPotentialEnergy(atoms[j]);
    //     }
    // }

    float totalE = KE + PE;
    std::cout << "<Energy>"
              << " KE=" << KE
              << " | PE=" << PE
              << " | E=" << totalE
              << std::endl;
}

void Simulation::logAtomPos() {
    int i = 0;
    for (Atom& atom : atoms) {
        std::cout << "<Pos> Atom (" << i
                  << ") X " << atom.coords.x
                  << " | Y " << atom.coords.y
                  << " | Z " << atom.coords.z
                  << std::endl;
        i++;
    }
}

void Simulation::logBondList() {
    // for (Bond& bond : Bond::bonds_list) {
    //     std::cout 
    //         << bond.a << " " << bond.a->type << " "
    //         << bond.b << " " << bond.b->type
    //         << std::endl;
    // }
    for (Atom& atom : atoms) {
        if (atom.bonds.size() > 0) {
            std::cout << atom.bonds.size() << std::endl;
        }
    }
}

void Simulation::logMousePos() {
    sf::Vector2i mouse_pos = sf::Mouse::getPosition(window);
    Vec2D world_pos = Tools::screenToWorld(mouse_pos, render.camera.getZoom());
    std::cout << "<Mouse pos>"
              << " Screen: "
              << "X " << mouse_pos.x
              << " Y " << mouse_pos.y
              << " | World: "
              << "X " << world_pos.x
              << " Y " << world_pos.y
              << std::endl;
}

void Simulation::drawGrid(bool flag) {
    render.drawGrid = flag;
}

void Simulation::drawBonds(bool flag) {
    render.drawBonds = flag;
}

void Simulation::setCameraPos(double x, double y) {
    render.camera.setPosition(x, y);
}

Vec2D randomUnitVector2D() {
    double angle = (double)std::rand() / RAND_MAX * 2 * M_PI;
    return Vec2D(std::cos(angle), std::sin(angle));  // x = cos(θ), y = sin(θ)
}

Vec3D randomUnitVector3D() {
    double u = (double)std::rand() / RAND_MAX;       // in [0,1]
    double v = (double)std::rand() / RAND_MAX;       // in [0,1]

    double theta = 2.0 * M_PI * u;                   // азимут (0..2π)
    double z = 2.0 * v - 1.0;                        // равномерно по [-1,1]
    double r = std::sqrt(1.0 - z * z);               // радиус проекции на xy

    double x = r * std::cos(theta);
    double y = r * std::sin(theta);

    return Vec3D(x, y, z);
}

double randomInRange(int range) {
    return std::rand() % range + 1;
}


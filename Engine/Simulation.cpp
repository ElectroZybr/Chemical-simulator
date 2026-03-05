#include "Simulation.h"
#include "../interface.h"
#include "Tools.h"
#include <cmath>
#include <algorithm>

#include "imgui.h"
#include "imgui-SFML.h"

Simulation::Simulation(sf::RenderWindow& w, SimBox& box)
    : window(w), gameView(window.getDefaultView()), uiView(window.getDefaultView()), render(w, gameView, uiView), sim_box(box)
    {
        // sim_box = box;
        sim_box.setRenderer(&render);

        Interface::init(window);
        Tools::init(&window, &gameView, &render, &sim_box.grid, &sim_box);
        Atom::setGrid(&sim_box.grid);

        // резервируем место под создание атомов
        atoms.reserve(50000);

        // setSizeBox(sizeX, sizeY);
    }

void Simulation::update(float dt) {
    if (!Interface::getPause()) {
            for (Atom& atom : atoms)
                atom.PredictPosition(dt);
            for (Atom& atom : atoms)
                atom.ComputeForces(sim_box, dt);
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
    render.drawShot(atoms, sim_box, deltaTime);
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
                
                Vec2D world = Tools::screenToWorld(mouse_pos, zoom);
                Vec2D local(world.x - sim_box.start.x, world.y - sim_box.start.y);
                atoms.emplace_back(Vec3D(local-0.5),
                                   Vec3D(randomUnitVector3D() * randomInRange(1)), Interface::getSelectedAtom());


                Atom& atom = atoms.back();

                std::cout << "<Create atom> X: " << world.x << 
                                            " Y: " << world.y << 
                                        " | Type: " << Interface::getSelectedAtom() << 
                                    " | Adress: " << &atom << 
                                    " | Speed: " << atom.speed.x << " " << atom.speed.y << std::endl;

            } else {
                // Передвижение атома мышкой
                Vec2D world = Tools::screenToWorld(mouse_pos, zoom);
                Vec2D local(world.x - sim_box.start.x, world.y - sim_box.start.y);
                std::unordered_set<Atom*>* block = sim_box.grid.at(
                    sim_box.grid.worldToCellX(local.x - 0.5),
                    sim_box.grid.worldToCellY(local.y - 0.5)
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
        Vec2D world = Tools::screenToBox(mouse_pos, zoom);
        Vec2D delta = Vec2D(selectedMoveAtom->coords.x, selectedMoveAtom->coords.y) - world;
        selectedMoveAtom->force -= delta.normalized() * delta.length() * 100;
    }    
}

void Simulation::setSizeBox(Vec3D s, Vec3D e, int cellSize) {
    if (sim_box.setSizeBox(s, e, cellSize)) {
        Atom::setGrid(&sim_box.grid);

        for (Atom& atom : atoms) {
            const int cellX = sim_box.grid.worldToCellX(atom.coords.x);
            const int cellY = sim_box.grid.worldToCellY(atom.coords.y);
            sim_box.grid.insert(cellX, cellY, &atom);
        }
    }
}

void Simulation::createRandomAtoms(int type, int quantity) {
    const int r_cut = 3;
    for (int i = 0; i < quantity; ++i) {
        for (int j = 0; j < 10; ++j) {
            double r_x = std::rand() % sim_box.grid.sizeX;
            double r_y = std::rand() % sim_box.grid.sizeY;
            Vec3D coords(r_x, r_y, r_cut);
            if (!checkNeighbor(coords, 3)) {
                createAtom(coords, randomUnitVector3D(), type);
                break;
            }
        }
    }
}

bool Simulation::checkNeighbor(Vec3D coords, float delta) {
    int curr_x = sim_box.grid.worldToCellX(coords.x), curr_y = sim_box.grid.worldToCellY(coords.y);
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            if (auto cell = sim_box.grid.at(curr_x - i, curr_y - j)) {
                for (Atom* other : *cell) {
                    if ((coords - other->coords).length() < delta) return true;
                }
            }
        }
    }
    return false;
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
    for (Atom& atom : atoms) {
        if (atom.bonds.size() > 0) {
            std::cout << atom.bonds.size() << std::endl;
        }
    }
}

void Simulation::logMousePos() {
    sf::Vector2i mouse_pos = sf::Mouse::getPosition(window);
    Vec2D world_pos = Tools::screenToWorld(mouse_pos, render.camera.getZoom());
    Vec2D local_pos(world_pos.x - sim_box.start.x, world_pos.y - sim_box.start.y);
    std::cout << "<Mouse pos>"
              << " Screen: "
              << "X " << mouse_pos.x
              << " Y " << mouse_pos.y
              << " | World: "
              << "X " << world_pos.x
              << " Y " << world_pos.y
              << " | Local: "
              << "X " << local_pos.x
              << " Y " << local_pos.y
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

void Simulation::setCameraZoom(float new_zoom) {
    render.camera.setZoom(new_zoom);
}

Vec2D randomUnitVector2D() {
    double angle = (double)std::rand() / RAND_MAX * 2 * M_PI;
    return Vec2D(std::cos(angle), std::sin(angle));  // x = cos(θ), y = sin(θ)
}

Vec3D randomUnitVector3D(double amplitude) {
    double u = (double)std::rand() / RAND_MAX;       // in [0,1]
    double v = (double)std::rand() / RAND_MAX;       // in [0,1]

    double theta = 2.0 * M_PI * u;                   // азимут (0..2π)
    double z = 2.0 * v - 1.0;                        // равномерно по [-1,1]
    double r = std::sqrt(1.0 - z * z);               // радиус проекции на xy

    double x = r * std::cos(theta);
    double y = r * std::sin(theta);

    return Vec3D(x, y, z) * amplitude;  // масштабируем вектор до нужной амплитуды
}

double randomInRange(int range) {
    return std::rand() % range + 1;
}
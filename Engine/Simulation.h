#include <SFML/Graphics.hpp>
#include "Renderer.h"

#include "physics/Atom.h"
#include "physics/SpatialGrid.h"
#include "SimBox.h"

class Simulation {
public:
    Simulation(sf::RenderWindow& window, SimBox& sim_box);
    void update(float dt);
    void renderShot(float dt);
    void event();
    void setSizeBox(Vec3D s, Vec3D e, int cellSize = -1);
    void createRandomAtoms(int type, int quantity);
    Atom* createAtom(Vec3D start_coords, Vec3D start_speed, int type, bool fixed = false);
    void addBond(Atom* a1, Atom* a2);
    double AverageTemp();
    void logEnergies();
    void logAtomPos();
    void logMousePos();
    void logBondList();
    void drawGrid(bool flag);
    void drawBonds(bool flag);
    void setCameraPos(double x, double y);
    void setCameraZoom(float new_zoom);
    SimBox& sim_box;
    Renderer render;

private:
    sf::RenderWindow& window;
    sf::View gameView;
    sf::View uiView;;
    std::vector<Atom> atoms;

    bool atomMoveFlag = false;
    bool selectionFrameMoveFlag = false;
    Atom* selectedMoveAtom;
    sf::Vector2i start_mouse_pos;

    bool checkNeighbor(Vec3D coords, float delta);
};

Vec2D randomUnitVector2D();
Vec3D randomUnitVector3D(double amplitude = 1.0);
double randomInRange(int range);

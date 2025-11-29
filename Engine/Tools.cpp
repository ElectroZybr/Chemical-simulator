#include "Tools.h"

sf::RenderWindow* Tools::window = nullptr;
sf::View* Tools::gameView = nullptr;
Renderer* Tools::render = nullptr;
SpatialGrid* Tools::grid = nullptr;

void Tools::init(sf::RenderWindow* w, sf::View* gv, Renderer* r, SpatialGrid* gr) {
    window = w;
    gameView = gv;
    render = r;
    grid = gr;
}

void Tools::selectionFrame(sf::Vector2i start_mouse_pos, sf::Vector2i mouse_pos, std::vector<Atom>& atoms) {
    Vec2D start_pos = screenToWorld(start_mouse_pos, render->camera.getZoom());
    Vec2D pos = screenToWorld(mouse_pos, render->camera.getZoom());
    render->setSelectionFrame(start_pos, pos, render->camera.getZoom());

    double temp;

    if (start_pos.x - pos.x > 0) {
        temp = pos.x;
        pos.x = start_pos.x;
        start_pos.x = temp;
    }

    if (start_pos.y - pos.y > 0) {
        temp = pos.y;
        pos.y = start_pos.y;
        start_pos.y = temp;
    }
    
    int count = 0;
    for (Atom& atom : atoms) {
        if (atom.coords.x >= start_pos.x-0.8 && atom.coords.x <= pos.x && atom.coords.y >= start_pos.y-0.8 && atom.coords.y <= pos.y) {
            atom.isSelect = true;
            count++;
        }
        else atom.isSelect = false;
    }
    Interface::drawToolTrip = true;
    Interface::countSelectedAtom = count;
}

Vec2D Tools::screenToWorld(sf::Vector2i mouse_pos, float zoom) {
    return Vec2D(((mouse_pos.x - (window->getSize().x / 2.f)) / zoom) + gameView->getCenter().x,
                 ((mouse_pos.y - (window->getSize().y / 2.f)) / zoom) + gameView->getCenter().y);
}
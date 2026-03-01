#pragma once

#include <SFML/Graphics.hpp>
#include <list>
#include <vector>
#include "Camera.h"

#include "physics/Atom.h"
#include "physics/SpatialGrid.h"

class Renderer {
public:
    Renderer(sf::RenderWindow& window, sf::View& gameView, sf::View& uiView);
    void drawShot(const std::vector<Atom>& atoms, const SpatialGrid& grid, float deltaTime);
    Camera camera;
    float alpha = 0.05; // Коэфициент уменьшения по мере удаления атома
    bool drawGrid = false;
    bool drawBonds = false;
    float drawBondsZoom = 25;
    bool drawSelectionFrame = false;
    void setSelectionFrame(Vec2D start, Vec2D end, float scale);
    sf::Texture forceTexture;
    void wallImage(const SpatialGrid& grid);
    
private:
    sf::RenderWindow& window;
    sf::View& gameView;
    sf::View& uiView;
    
    std::vector<sf::Vertex> gridLines;
    sf::CircleShape atomShape;
    sf::RectangleShape frameShape;
    sf::RectangleShape forceFieldQuad;
    sf::Shader forceFieldShader;
    bool forceFieldShaderLoaded = false;
    void drawTransparencyMap(sf::RenderWindow& window, const SpatialGrid& grid);
    void drawForceField(const sf::Texture& forceTexture, const SpatialGrid& grid);
    int getWallForce(int coord, int min, int max);
};

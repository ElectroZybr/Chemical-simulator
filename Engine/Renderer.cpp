#include "Renderer.h"

#include "imgui.h"
#include "imgui-SFML.h"

Renderer::Renderer(sf::RenderWindow& w, sf::View& gv, sf::View& uv)
                    : window(w), gameView(gv), uiView(uv), camera(w, &gv), atomShape(5.f)
    {
        // camera.move(50, 50);
        const int gridSize = 50;
        for (int x = -1000; x <= 1000; x += gridSize) {
            gridLines.push_back(sf::Vertex(sf::Vector2f(x, -1000), sf::Color(60, 60, 60)));
            gridLines.push_back(sf::Vertex(sf::Vector2f(x, 1000), sf::Color(60, 60, 60)));
        }
        for (int y = -1000; y <= 1000; y += gridSize) {
            gridLines.push_back(sf::Vertex(sf::Vector2f(-1000, y), sf::Color(60, 60, 60)));
            gridLines.push_back(sf::Vertex(sf::Vector2f(1000, y), sf::Color(60, 60, 60)));
        }

        forceFieldShaderLoaded = forceFieldShader.loadFromFile("force_shader.frag", sf::Shader::Fragment);
        forceFieldQuad.setPosition(0.f, 0.f);

    }

void Renderer::wallImage(const SpatialGrid& grid) {
    constexpr int textureScale = 4;
    const int worldWidth = grid.sizeX * grid.cellSize;
    const int worldHeight = grid.sizeY * grid.cellSize;
    const int width = worldWidth * textureScale;
    const int height = worldHeight * textureScale;

    std::vector<sf::Uint8> forcePixels(width * height * 4, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = 4 * (y * width + x);
            forcePixels[idx + 0] = 255;
            forcePixels[idx + 1] = 0;
            forcePixels[idx + 2] = 0;

            int alpha = getWallForce(x, 0, width - 1) + getWallForce(y, 0, height - 1);
            if (alpha > 255.0f) alpha = 255.0f;
            forcePixels[idx + 3] = static_cast<sf::Uint8>(alpha);
        }
    }

    forceTexture.create(width, height);
    forceTexture.update(forcePixels.data());
    forceTexture.setSmooth(true);
}

int Renderer::getWallForce(int coord, int min, int max) {
    constexpr int border = 25;
    const double k = 255.0 / static_cast<double>(border * border);
    double force = 0.0;

    if (coord < min + border) {
        const double dist = static_cast<double>((min + border) - coord);
        force += k * dist * dist;
    }

    if (coord > max - border) {
        const double dist = static_cast<double>(coord - (max - border));
        force += k * dist * dist;
    }

    if (force > 255.0) force = 255.0;
    return static_cast<int>(force);
}

void Renderer::drawShot(const std::vector<Atom>& atoms, const SpatialGrid& grid, float deltaTime)
{
    // Управление камерой
    camera.handleInput(deltaTime, window);  
    // Обновление камеры
    camera.update(deltaTime, window);
    
    // Очистка
    window.clear(sf::Color(35, 35, 35, 255));

    // Рисуем игровые объекты
    window.setView(gameView);
    window.draw(&gridLines[0], gridLines.size(), sf::Lines);
    drawForceField(forceTexture, grid);

    if (drawGrid)
        drawTransparencyMap(window, grid);

    std::vector<const Atom*> sorted;
    for (const Atom& a : atoms) sorted.push_back(&a);
    std::sort(sorted.begin(), sorted.end(), [](const Atom* a, const Atom* b) {return a->coords.z > b->coords.z;});

    for (const Atom* atom : sorted) {
        atomShape.setPosition(atom->coords.x, atom->coords.y);
        atomShape.setRadius(atom->getProps().radius - (atom->coords.z * alpha));
        atomShape.setFillColor(atom->getProps().color);
        if (atom->isSelect)
            atomShape.setOutlineColor(sf::Color::Red);
        else
            atomShape.setOutlineColor(sf::Color::Black);
        atomShape.setOutlineThickness(-0.05);
        window.draw(atomShape);
    }

    if (drawBonds) {
        if (camera.getZoom() > drawBondsZoom) {
            for (const Bond& bond : Bond::bonds_list) {
                sf::Vertex line[] =
                {
                    sf::Vertex(sf::Vector2f(bond.a->coords.x+bond.a->getProps().radius - (bond.a->coords.z * alpha), bond.a->coords.y+bond.a->getProps().radius - (bond.a->coords.z * alpha))),
                    sf::Vertex(sf::Vector2f(bond.b->coords.x+bond.b->getProps().radius - (bond.b->coords.z * alpha), bond.b->coords.y+bond.b->getProps().radius - (bond.b->coords.z * alpha)))
                };
                // цвет линии
                line[0].color = sf::Color::Blue;
                line[1].color = sf::Color::Blue;

                window.draw(line, 2, sf::Lines);
            }
        }
    }

    if (drawSelectionFrame)
        window.draw(frameShape);

    // Рисуем интерфейс
    window.setView(uiView);
    ImGui::SFML::Render(window);
    
    // Вывод на экран
    window.display();
}

void Renderer::drawTransparencyMap(sf::RenderWindow& window, const SpatialGrid& grid)
{
    sf::RectangleShape cellRect(sf::Vector2f(
        static_cast<float>(grid.cellSize),
        static_cast<float>(grid.cellSize)
    ));
    cellRect.setFillColor(sf::Color(255, 0, 0, 120));
    cellRect.setOutlineColor(sf::Color(120, 0, 0, 180));
    cellRect.setOutlineThickness(-1.0f);

    for (int y = 0; y < grid.sizeY; ++y) {
        for (int x = 0; x < grid.sizeX; ++x) {
            if (auto cell = grid.at(x, y); cell && !cell->empty()) {
                cellRect.setPosition(
                    static_cast<float>(x * grid.cellSize),
                    static_cast<float>(y * grid.cellSize)
                );
                window.draw(cellRect);
            }
        }
    }
}

void Renderer::drawForceField(const sf::Texture& forceTexture, const SpatialGrid& grid) {
    if (!forceFieldShaderLoaded || forceTexture.getSize().x == 0 || forceTexture.getSize().y == 0) {
        return;
    }

    forceFieldShader.setUniform("field", forceTexture);

    forceFieldQuad.setSize(sf::Vector2f(
        static_cast<float>(grid.sizeX),
        static_cast<float>(grid.sizeY)
    ));
    forceFieldQuad.setTexture(&forceTexture, true);

    window.draw(forceFieldQuad, &forceFieldShader);
}

void Renderer::setSelectionFrame(Vec2D start, Vec2D end, float scale) {
    frameShape.setPosition(start.x, start.y);
    frameShape.setSize(sf::Vector2f(end.x - start.x, end.y - start.y));

    frameShape.setFillColor(sf::Color::Transparent);      // без заливки
    frameShape.setOutlineColor(sf::Color(60, 60, 60));    // цвет контура
    frameShape.setOutlineThickness(1.f / scale);          // толщина контура
}

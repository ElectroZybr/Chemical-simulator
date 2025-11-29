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
    drawForceField(forceTexture);

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
    // Создаем изображение нужного размера
    sf::Image image;
    image.create(grid.sizeX, grid.sizeY, sf::Color::Transparent);
    
    // Заполняем пиксели
    for (size_t y = 0; y < grid.sizeX; ++y) {
        for (size_t x = 0; x < grid.sizeY; ++x) {
            if (!grid.at(x, y)->empty())
                image.setPixel(x, y, sf::Color(255, 0, 0, 255));
        }
    }
    
    // Создаем текстуру из изображения
    sf::Texture texture;
    texture.loadFromImage(image);
    
    // Отрисовываем спрайт
    sf::Sprite sprite(texture);
    window.draw(sprite);
}

void Renderer::drawForceField(const sf::Texture& forceTexture) {
    sf::Shader shader;
    shader.loadFromFile("force_shader.frag", sf::Shader::Fragment);

    shader.setUniform("field", forceTexture);

    sf::RectangleShape screenQuad;
    screenQuad.setSize(sf::Vector2f(100, 100));
    screenQuad.setPosition(0, 0);
    screenQuad.setTexture(&forceTexture);

    window.draw(screenQuad, &shader);
}

void Renderer::setSelectionFrame(Vec2D start, Vec2D end, float scale) {
    frameShape.setPosition(start.x, start.y);
    frameShape.setSize(sf::Vector2f(end.x - start.x, end.y - start.y));

    frameShape.setFillColor(sf::Color::Transparent);      // без заливки
    frameShape.setOutlineColor(sf::Color(60, 60, 60));    // цвет контура
    frameShape.setOutlineThickness(1.f / scale);          // толщина контура
}
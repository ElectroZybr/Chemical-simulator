#include <SFML/Graphics.hpp>
#include <cmath>

#include <iostream>
#include <functional>
#include <memory>
#include <random>

#include "imgui.h"
#include "imgui-SFML.h"

#include "engine/Camera.h"
#include "engine/physics/Atom.h"
#include "engine/physics/SpatialGrid.h"
#include "interface.h"

#define WIGHT   800
#define HEIGHT  600


void drawTransparencyMap(sf::RenderWindow& window, const SpatialGrid* grid)
{
    // Создаем изображение нужного размера
    sf::Image image;
    image.create(grid->sizeX, grid->sizeY, sf::Color::Transparent);
    
    // Заполняем пиксели
    for (size_t y = 0; y < grid->sizeX; ++y) {
        for (size_t x = 0; x < grid->sizeY; ++x) {
            if (grid->at(x, y) != nullptr)
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

// void drawTransparencyMap(sf::RenderWindow& window, const SpatialGrid* grid)
// {
//     // Создаем текстуру из данных
//     sf::Texture texture;
//     texture.create(grid->sizeX, grid->sizeY);
    
//     // Конвертируем данные в формат RGBA
//     std::vector<sf::Uint8> pixels(grid->sizeX * grid->sizeY * 4);
//     for (size_t y = 0; y < grid->sizeY; ++y) {
//         for (size_t x = 0; x < grid->sizeX; ++x) {
//             size_t index = (y * grid->sizeX + x) * 4;
//             pixels[index] = 255;     // R
//             pixels[index+1] = 0;     // G
//             pixels[index+2] = 0;     // B
//             pixels[index+3] = grid->at(x, y); // A
//         }
//     }
    
//     texture.update(pixels.data());
    
//     // Простой шейдер для отрисовки
//     sf::Shader shader;
//     shader.loadFromMemory(
//         "void main() {"
//         "    gl_FragColor = texture2D(texture, gl_TexCoord[0].xy);"
//         "}", sf::Shader::Fragment);
    
//     // Отрисовываем
//     sf::Sprite sprite(texture);
//     window.draw(sprite, &shader);
// }

Vec2D randomUnitVector2D() {
    double angle = (double)std::rand() / RAND_MAX * 2 * M_PI;
    return Vec2D(std::cos(angle), std::sin(angle));  // x = cos(θ), y = sin(θ)
}

double randomInRange(int range) {
    return std::rand() % range + 1;
}

int main() {
    sf::RenderWindow window(sf::VideoMode(WIGHT, HEIGHT), "Chemical-simulator");
    window.setFramerateLimit(60);
    float dt = 1.0f / 240.0f;

    Interface::init(window);

    // std::vector<std::vector<unsigned char>> alphaMap(100, std::vector<unsigned char>(100));
    // for (int y = 0; y < 100; ++y) {
    //     for (int x = 0; x < 100; ++x) {
    //         alphaMap[y][x] = static_cast<unsigned char>((x + y) * 255 / 200);
    //     }
    // }

    // sf::Texture texture;
    // texture.create(10000, 10000);  // Создаем текстуру 10K×10K
    // texture.setSmooth(false);       // Отключаем сглаживание

    // sf::Shader computeShader, renderShader;
    // computeShader.loadFromFile("compute_shader.glsl", sf::Shader::Compute);
    // renderShader.loadFromFile("fragment_shader.glsl", sf::Shader::Fragment);

    // Создаем игровое окно и камеру
    sf::View gameView = window.getDefaultView();
    Camera camera(window, &gameView);

    // Создаем окно для интерфейса
    sf::View uiView = window.getDefaultView();
    
    // Создаем тестовые объекты
    sf::CircleShape circle(1.f);
    circle.setFillColor(sf::Color::Red);
    circle.setPosition(-1, -1);
    
    sf::Color wallColor = sf::Color(30, 30, 30, 255);
    sf::RectangleShape rect1(sf::Vector2f(50, 2000));
    rect1.setFillColor(wallColor);
    rect1.setPosition(-1000, -1000);
    sf::RectangleShape rect2(sf::Vector2f(2000, 50));
    rect2.setFillColor(wallColor);
    rect2.setPosition(-1000, -1000);
    sf::RectangleShape rect3(sf::Vector2f(50, 2000));
    rect3.setFillColor(wallColor);
    rect3.setPosition(950, -1000);
    sf::RectangleShape rect4(sf::Vector2f(2000, 50));
    rect4.setFillColor(wallColor);
    rect4.setPosition(-1000, 950);

    std::vector<sf::Vertex> gridLines;
    const int gridSize = 100;
    for (int x = -1000; x <= 1000; x += gridSize) {
        gridLines.push_back(sf::Vertex(sf::Vector2f(x, -1000), sf::Color(60, 60, 60)));
        gridLines.push_back(sf::Vertex(sf::Vector2f(x, 1000), sf::Color(60, 60, 60)));
    }
    for (int y = -1000; y <= 1000; y += gridSize) {
        gridLines.push_back(sf::Vertex(sf::Vector2f(-1000, y), sf::Color(60, 60, 60)));
        gridLines.push_back(sf::Vertex(sf::Vector2f(1000, y), sf::Color(60, 60, 60)));
    }

    SpatialGrid grid(10, 10);
    Atom::setGrid(&grid);

    std::vector<Atom> atoms;
    sf::CircleShape atomShape(5.f);

    atoms.push_back(Atom(5, 2, 1, Vec2D(0, 0), true));
    atoms.push_back(Atom(5, 8, 1, Vec2D(0, -1)));

    // atoms.push_back(Atom(2, 5, 1, Vec2D(1, 0)));
    // atoms.push_back(Atom(8, 5, 1, Vec2D(-1, 0)));
    
    sf::Clock clock;

    bool atomMoveFlag = false;
    Atom* selectedMoveAtom;
    
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
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

            // создание атома
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                float zoom = camera.getZoom();
                if (!Interface::cursorHovered && Interface::getSelectedAtom() != -1) {

                    // std::cout << getRandomInRange(10) << "\n";
                    // Vec2D rnd = randomUnitVector2D();
                    // std::cout << rnd.x() << "  " << rnd.y() << "  " << std::sqrt(rnd.x()*rnd.x() + rnd.y()*rnd.y()) << std::endl;
                    // SpatialGrid grid(10, 10);
                    // grid.put(5, 5, 6);
                    // std::cout << (int)grid.at(5, 5) << "  " << (int)grid.at(2, 2) << std::endl;
                    // for (int i = 0; i < 10; ++i) {
                    //     for (int j = 0; j < 10; ++j)
                    //         std::cout << (int)grid.at(i, j);
                    //     std::cout << std::endl;
                    // }
                    
                    atoms.push_back(Atom(((mouse_pos.x - (window.getSize().x / 2.f)) / zoom) + gameView.getCenter().x-0.25,
                                         ((mouse_pos.y - (window.getSize().y / 2.f)) / zoom) + gameView.getCenter().y-0.25,
                                           Interface::getSelectedAtom(), Vec2D(randomUnitVector2D() * randomInRange(5))));

                    Atom atom = atoms.back();

                    std::cout << "<Create atom> X: " << ((mouse_pos.x - (window.getSize().x / 2.f)) / zoom) + gameView.getCenter().x << 
                                              " Y: " << ((mouse_pos.y - (window.getSize().y / 2.f)) / zoom) + gameView.getCenter().y << 
                                         " | Type: " << Interface::getSelectedAtom() << 
                                        // " | Color: " << (int)atomColor.r << " " << (int)atomColor.g << " " << (int)atomColor.b << std::endl;
                                         " | Speed: " << atom.speed.x << " " << atom.speed.y << std::endl;

                                          
                    // std::cout << "Viev Center X: " << gameView.getCenter().x << " Y: " << gameView.getCenter().y << " | Mouse pos X: " << mouse_pos.x << " Y: " << mouse_pos.y << " | Zoom: " << zoom << std::endl;
                } else {
                    // Передвижение атома мышкой
                    selectedMoveAtom = grid.at((int)round(((mouse_pos.x - (window.getSize().x / 2.f)) / zoom) + gameView.getCenter().x-0.5), 
                    (int)round(((mouse_pos.y - (window.getSize().y / 2.f)) / zoom) + gameView.getCenter().y-0.5));
                    if (selectedMoveAtom != nullptr)
                        atomMoveFlag = true;                    
                } 
            } else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                atomMoveFlag = false;
            }
            
            camera.handleEvent(event, window);
        }


        Interface::Update();

        // Управление камерой
        camera.handleInput(deltaTime, window);  
        // Обновление камеры
        camera.update(deltaTime, window);
        
        // Очистка
        window.clear(sf::Color(35, 35, 35, 255));
        
        // Рисуем игровые объекты
        window.setView(gameView);
        window.draw(&gridLines[0], gridLines.size(), sf::Lines);
        window.draw(circle);
        window.draw(rect1);
        window.draw(rect2);
        window.draw(rect3);
        window.draw(rect4);
        drawTransparencyMap(window, &grid);
        // отрисовываем атомы
        for (auto& atom : atoms) {
            if (!Interface::getPause())    // Обновляем симуляцию если не пауза
                atom.Update(dt);
            atomShape.setPosition(atom.coords.x, atom.coords.y);
            atomShape.setRadius(atom.getProps().radius);
            atomShape.setFillColor(atom.getProps().color);

            window.draw(atomShape);
            // std::cout << atom.coords.x() << " " << atom.coords.y() << " " << atom.type << std::endl;
        }
        // Передвижение атома мышкой
        if (atomMoveFlag) {
            float zoom = camera.getZoom();
            selectedMoveAtom->coords = Vec2D(((mouse_pos.x - (window.getSize().x / 2.f)) / zoom) + gameView.getCenter().x-0.5,
                                             ((mouse_pos.y - (window.getSize().y / 2.f)) / zoom) + gameView.getCenter().y-0.5);
        }

        // Рисуем интерфейс
        window.setView(uiView);
        ImGui::SFML::Render(window);
        
        // Вывод на экран
        window.display();
    }
    ImGui::SFML::Shutdown();
    
    return 0;
}
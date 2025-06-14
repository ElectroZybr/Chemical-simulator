#include <SFML/Graphics.hpp>
#include <cmath>

#include <iostream>
#include <functional>
#include <memory>

#include "imgui.h"
#include "imgui-SFML.h"

#include "engine/Camera.h"
#include "engine/physics/Atom.h"
#include "interface.h"

#define WIGHT   800
#define HEIGHT  600


int main() {
    sf::RenderWindow window(sf::VideoMode(WIGHT, HEIGHT), "Chemical-simulator");
    window.setFramerateLimit(60);

    Interface::init(window);


    // Создаем игровое окно и камеру
    sf::View gameView = window.getDefaultView();
    Camera camera(window, &gameView);

    // Создаем окно для интерфейса
    sf::View uiView = window.getDefaultView();
    
    // Создаем тестовые объекты
    sf::CircleShape circle(10.f);
    circle.setFillColor(sf::Color::Red);
    circle.setPosition(-10, -10);
    
    sf::RectangleShape rect(sf::Vector2f(100, 100));
    rect.setFillColor(sf::Color::Blue);
    rect.setPosition(500, 400);

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

    std::vector<Atom> atoms;
    sf::CircleShape atomShape(5.f);
    
    sf::Clock clock;
    
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        window.setView(uiView);
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
            if (!Interface::cursorHovered && Interface::getSelectedAtom() != -1) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    float zoom = camera.getZoom();

                    atoms.push_back(Atom(((mouse_pos.x - (window.getSize().x / 2.f)) / zoom) + gameView.getCenter().x-0.25,
                                         ((mouse_pos.y - (window.getSize().y / 2.f)) / zoom) + gameView.getCenter().y-0.25,
                                           Interface::getSelectedAtom()));

                    sf::Color atomColor = atoms.back().getProps().color;

                    std::cout << "<Create atom> X: " << ((mouse_pos.x - (window.getSize().x / 2.f)) / zoom) + gameView.getCenter().x << 
                                              " Y: " << ((mouse_pos.y - (window.getSize().y / 2.f)) / zoom) + gameView.getCenter().y << 
                                         " | Type: " << Interface::getSelectedAtom() << 
                                        " | Color: " << (int)atomColor.r << " " << (int)atomColor.g << " " << (int)atomColor.b << std::endl;

                                          
                    // std::cout << "Viev Center X: " << gameView.getCenter().x << " Y: " << gameView.getCenter().y << " | Mouse pos X: " << mouse_pos.x << " Y: " << mouse_pos.y << " | Zoom: " << zoom << std::endl;
                }   
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
        window.draw(rect);
        // отрисовываем атомы
        for (const auto& atom : atoms) {
            atomShape.setPosition(atom.coords.x(), atom.coords.y());
            atomShape.setRadius(atom.getProps().radius);
            atomShape.setFillColor(atom.getProps().color);
            window.draw(atomShape);
            // std::cout << atom.coords.x() << " " << atom.coords.y() << " " << atom.type << std::endl;
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
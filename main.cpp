
#include <SFML/Graphics.hpp>
#include <cmath>
#include "engine/Camera.h"


void resizeView(const sf::RenderWindow& window, sf::View& view) {
    float aspectRatio = (float)window.getSize().x / (float)window.getSize().y;
    float viewRatio = view.getSize().x / view.getSize().y;
    
    if (aspectRatio > viewRatio) {
        // Окно шире, чем View - добавляем черные полосы сверху и снизу
        float newHeight = window.getSize().x / viewRatio;
        view.setSize(window.getSize().x, newHeight);
    } else {
        // Окно уже, чем View - добавляем черные полосы слева и справа
        float newWidth = window.getSize().y * viewRatio;
        view.setSize(newWidth, window.getSize().y);
    }
    
    view.setCenter(view.getSize().x / 2, view.getSize().y / 2);
}

int main() {
    sf::RenderWindow window(sf::VideoMode(800, 600), "2D Camera Example");
    // window.setFramerateLimit(60);

    // sf::View view(sf::FloatRect(0, 0, 800, 600));
    // window.setView(view);
    
    // Создаем камеру
    Camera camera(window);
    
    // Создаем тестовые объекты
    sf::CircleShape circle(50.f);
    circle.setFillColor(sf::Color::Red);
    circle.setPosition(300, 200);
    
    sf::RectangleShape rect(sf::Vector2f(100, 100));
    rect.setFillColor(sf::Color::Blue);
    rect.setPosition(500, 400);

    std::vector<sf::Vertex> gridLines;
    const int gridSize = 100;
    for (int x = -1000; x <= 1000; x += gridSize) {
        gridLines.push_back(sf::Vertex(sf::Vector2f(x, -1000), sf::Color(50, 50, 50)));
        gridLines.push_back(sf::Vertex(sf::Vector2f(x, 1000), sf::Color(50, 50, 50)));
    }
    for (int y = -1000; y <= 1000; y += gridSize) {
        gridLines.push_back(sf::Vertex(sf::Vector2f(-1000, y), sf::Color(50, 50, 50)));
        gridLines.push_back(sf::Vertex(sf::Vector2f(1000, y), sf::Color(50, 50, 50)));
    }
    
    sf::Clock clock;
    
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            camera.handleEvent(event, window);
        }
        
        
        // Управление камерой
        camera.handleInput(deltaTime, window);
        
        // Обновление камеры
        camera.update(deltaTime, window);
        
        // Отрисовка
        window.clear(sf::Color::Black);

        window.draw(&gridLines[0], gridLines.size(), sf::Lines);
        
        // Рисуем объекты
        window.draw(circle);
        window.draw(rect);
        
        // Можно добавить сетку или другие элементы мира
        // ...
        
        window.display();
    }
    
    return 0;
}

// #include <SFML/Graphics.hpp>
// #include <iostream>


// int main() {
//     sf::RenderWindow window(sf::VideoMode(800, 600), "Fixed View");

//     sf::RectangleShape rect(sf::Vector2f(200, 100));
//     rect.setFillColor(sf::Color::Green);
//     rect.setPosition(300, 250); // Координаты относительно View (800x600)

//     while (window.isOpen()) {
//         sf::Event event;
//         while (window.pollEvent(event)) {
//             if (event.type == sf::Event::Closed)
//                 window.close();
            
//             // Игнорируем изменение размеров окна (View не меняется)
//             if (event.type == sf::Event::Resized) {
//                 window.setView(sf::View(sf::FloatRect(0, 0, window.getSize().x, window.getSize().y)));
//             }
//         }

//         window.clear();
//         window.draw(rect);
//         window.display();
//     }

//     return 0;
// }
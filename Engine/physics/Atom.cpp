#include "Atom.h"
#include <iostream>
#include <cmath>
// #include <fstream>
// #include <vector>

SpatialGrid* Atom::grid = nullptr;

const std::array<StaticAtomicData, 118> Atom::properties = {{
        {0.0000,  0.0, 0.0, sf::Color::Transparent       },
        {1.0080, 2, 0.0, sf::Color(255, 255, 255, 255)},  // Водород
        {4.0026, 0.31, 0.0, sf::Color(0,   0,   0,   255)},  // Гелий
        {6.9390, 1.67, 0.0, sf::Color(0,   0,   0,   255)},  // Литий
        {9.0122, 1.12, 0.0, sf::Color(0,   0,   0,   255)},  // Бериллий
        {10.811, 0.87, 0.0, sf::Color(0,   0,   0,   255)},  // Бор
        {12.011, 0.67, 0.0, sf::Color(0,   0,   0,   255)},  // Углерод
        {14.007, 0.56, 0.0, sf::Color(80,  70,  230, 255)},  // Азот
        {15.999, 0.48, 0.0, sf::Color(255, 50,  50,  255)},  // Кислород
        {18.998, 0.42, 0.0, sf::Color(0,   0,   0,   255)},  // Фтор
        {20.179, 0.38, 0.0, sf::Color(0,   0,   0,   255)},  // Неон
        {22.990, 1.90, 0.0, sf::Color(0,   0,   0,   255)},  // Натрий
        {24.305, 1.45, 0.0, sf::Color(0,   0,   0,   255)},  // Марганец
        {26.981, 1.18, 0.0, sf::Color(0,   0,   0,   255)},  // Алюминий
        {28.086, 1.11, 0.0, sf::Color(0,   0,   0,   255)},  // Кремний
        {30.974, 0.98, 0.0, sf::Color(0,   0,   0,   255)},  // Фосфор
        {32.064, 0.88, 0.0, sf::Color(0,   0,   0,   255)},  // Сера
        {35.453, 0.79, 0.0, sf::Color(0,   0,   0,   255)},  // Хлор
        {39.948, 0.71, 0.0, sf::Color(0,   0,   0,   255)},  // Аргон
    }};

void Atom::setGrid(SpatialGrid* grid_ptr) {
    grid = grid_ptr;
}

Atom::Atom(float x, float y, int type, Vec2D start_speed) : coords(x, y), type(type), speed(start_speed) {}

void Atom::Update(double deltaTime) {
    if ((int)coords.x() >= 0 && (int)coords.x() <= grid->sizeX-5 && (int)coords.y() >= 0 && (int)coords.y() <= grid->sizeY-5) {
        char* matrix[5];
        grid->getMatrix5X5((int)coords.x(), (int)coords.y(), matrix);
        
        for(int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j) {
                matrix[i][j] = 0;
            }
        }
    }

    coords = coords + speed * deltaTime;

    if ((int)coords.x() >= 0 && (int)coords.x() <= grid->sizeX-1 && (int)coords.y() >= 0 && (int)coords.y() <= grid->sizeY-1) {
        // std::cout << (int)grid->at((int)coords.x(), (int)coords.y()) << std::endl;
        if (grid->at((int)coords.x(), (int)coords.y()) == -1) {
            if (coords.x() < 1 || coords.x() > grid->sizeX-1) {
                speed = Vec2D(-speed.x(), speed.y());
            } else {
                speed = Vec2D(speed.x(), -speed.y());
            }
            std::cout << "Collision" << std::endl;
        }
    }

    if ((int)coords.x() >= 0 && (int)coords.x() <= grid->sizeX-5 && (int)coords.y() >= 0 && (int)coords.y() <= grid->sizeY-5) {
        char* matrix[5];
        grid->getMatrix5X5((int)coords.x(), (int)coords.y(), matrix);
        double part;
        Vec2D frac(modf(coords.x(), &part), modf(coords.y(), &part));
        std::cout << frac.x() << "  " << frac.y() << std::endl;
        
        for(int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j) {
                float dist = std::sqrt((i-2-frac.x()) * (i-2-frac.x()) + (j-2-frac.y()) * (j-2-frac.y()));
                float normalized_dist = std::min(dist / 2.5f, 1.0f);
                matrix[i][j] += 255 * (1 - normalized_dist);
            }
        }
    }
}
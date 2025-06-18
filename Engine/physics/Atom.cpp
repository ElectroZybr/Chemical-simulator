#include "Atom.h"
#include <iostream>
#include <cmath>
// #include <fstream>
// #include <vector>

SpatialGrid* Atom::grid = nullptr;

const std::array<StaticAtomicData, 118> Atom::properties = {{
        {0.0000,  0.0, 0.0, sf::Color::Transparent       },
        {1.0080, 0.5, 0.0, sf::Color(255, 255, 255, 255)},  // Водород
        {4.0026, 0.31, 0.0, sf::Color(0,   0,   0,   255)},  // Гелий
        {6.9390, 1.67, 0.0, sf::Color(0,   0,   0,   255)},  // Литий
        {9.0122, 1.12, 0.0, sf::Color(0,   0,   0,   255)},  // Бериллий
        {10.811, 0.5, 0.0, sf::Color(0,   0,   0,   255)},  // Бор
        {12.011, 0.5, 0.0, sf::Color(0,   0,   0,   255)},  // Углерод
        {14.007, 0.5, 0.0, sf::Color(80,  70,  230, 255)},  // Азот
        {15.999, 0.5, 0.0, sf::Color(255, 50,  50,  255)},  // Кислород
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

Atom::Atom(float x, float y, int type, Vec2D start_speed, bool fixed) : coords(x, y), type(type), speed(start_speed), charge(0), prevCoords(x, y), isFixed(fixed) {}

void Atom::Update(double deltaTime) {
    grid->put((int)round(coords.x), (int)round(coords.y), nullptr);

    // Взаимодействие с соседними атомами
    Collision();

    // Vec2D temp = coords;
    // // Вычисляем новую позицию по методу Верле
    // coords = coords * 2.0f - prevCoords + speed * (deltaTime * deltaTime);
    
    if (isFixed == false)
        coords = coords + speed * deltaTime;

    // prevCoords = temp;


    // Проверка отражения от стен
    Bounce();

    grid->put((int)round(coords.x), (int)round(coords.y), this);
}

void Atom::Bounce() {
    // Отражение от стен
    double x = coords.x, y = coords.y;
    if (x < 1 || x > grid->sizeX-2) {
        if (x < 1) coords.x = 1;
        else coords.x = grid->sizeX-2;
        speed.x = -speed.x;
    } else if (y <1 || y > grid->sizeY-2) {
        if (y < 1) coords.y = 1;
        else coords.y = grid->sizeY-2;
        speed.y = -speed.y;
    }
}

void Atom::Collision() {
    // проверка коллизий
    int x = (int)round(coords.x), y = (int)round(coords.y);
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            if (grid->at(x-i, y-j) != nullptr) {
                Atom* other = grid->at(x-i, y-j);
                if(other == this) continue;

                float dx = other->coords.x - coords.x;
                float dy = other->coords.y - coords.y;
                float distance_sq = dx*dx + dy*dy;

                // Проверяем столкновение (сумма радиусов = 1, поэтому сравниваем с 1.0f)
                // if (distance_sq >= 1.0f) continue;

                // Вычисляем расстояние (избегаем деления на 0)
                float distance = sqrt(distance_sq);
                if (distance < 1e-10f) distance = 1e-10f;

                // Нормализованный вектор столкновения
                float nx = dx / distance;
                float ny = dy / distance;

                float morse = MorseForce(distance)*0.01;
                float delta_potential = MorsePotential(distance) - MorsePotential(r0);
                speed.x += morse * nx / getProps().mass;
                speed.y += morse * ny / getProps().mass;

                other->speed.x -= morse * nx / other->getProps().mass;
                other->speed.y -= morse * nx / other->getProps().mass;

                energy -= morse * nx / getProps().mass + morse * ny / getProps().mass;


                // energy += delta_potential * 0.5f;
                // other->energy += delta_potential * 0.5f;


                std::cout << "<Morse forse> " << morse << std::endl;
                

                // // Вычисляем относительную скорость
                // float vx = speed.x - other->speed.x;
                // float vy = speed.y - other->speed.y;
                
                // // Вычисляем угол столкновения
                // double dot = vx * nx + vy * ny;
                // double mag1 = sqrt(vx * vx + vy * vy); // Длина первого вектора
                // double mag2 = sqrt(nx * nx + ny * ny); // Длина второго вектора

                // if (mag1 == 0 || mag2 == 0)
                //     continue; // Если один из векторов нулевой, угол неопределён

                // double cosTheta = dot / (mag1 * mag2);
                // cosTheta = std::max(-1.0, std::min(1.0, cosTheta)); // Ограничение из-за погрешностей
                // double arccos = acos(cosTheta); // Возвращаем угол в радианах

                // double angle = arccos * 180.0 / M_PI;

                // // Если угол столкновения меньше 30 градусов
                // if (angle <= 30 && energy >= 0 && other->energy >= 0) {
                //     energy = -4.52;
                //     std::cout << "<Connection> Type: covalent  Energy: " << energy << std::endl;
                // } else { 
                    
 
                    // // Проекция относительной скорости на нормаль
                    // float velocity_along_normal = vx * nx + vy * ny;

                    // // Частицы удаляются друг от друга - столкновения нет
                    // if (velocity_along_normal < 0) continue;

                    // // Импульс столкновения (с учетом коэффициента упругости)
                    // const float restitution = 1.0f;
                    // float j = -(1.0f + restitution) * velocity_along_normal;
                    // j /= (1.0f / getProps().mass + 1.0f / other->getProps().mass);

                    // // Применяем импульс
                    // float impulse_x = j * nx;
                    // float impulse_y = j * ny;
                    
                    // speed.x += impulse_x / getProps().mass;
                    // speed.y += impulse_y / getProps().mass;
                    // other->speed.x -= impulse_x / other->getProps().mass;
                    // other->speed.y -= impulse_y / other->getProps().mass;

                    // // Коррекция позиций для предотвращения наложения
                    // const float percent = 0.2f; // обычно 20%-80%
                    // const float slop = 0.1f;   // допустимое проникновение
                    // float correction = std::max(1.0f - distance - slop, 0.0f) / 
                    //                 (1.0f / getProps().mass + 1.0f / other->getProps().mass) * percent;
                    
                    // float correction_x = correction * nx;
                    // float correction_y = correction * ny;
                    
                    // coords.x -= correction_x / getProps().mass;
                    // coords.y -= correction_y / getProps().mass;
                    // other->coords.x += correction_x / other->getProps().mass;
                    // other->coords.y += correction_y / other->getProps().mass;
                //     std::cout << "<Collision> Angle: " << angle << std::endl;
                // }
            }
        }
    }
}

float Atom::MorseForce(float distanse) {
    float exponent = std::exp(-a * (distanse - r0));
    return 2 * De * a * (1 - exponent) * exponent;
}

float Atom::MorsePotential(float distanse) {
    float exponent = std::exp(-a * (distanse - r0));
    return De * (1 - exponent) * (1 - exponent);
}
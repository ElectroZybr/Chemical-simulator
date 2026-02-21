#include "Atom.h"
#include <iostream>
#include <cmath>

SpatialGrid* Atom::grid = nullptr;
// std::list<Bond> Atom::bonds_list;

const std::array<StaticAtomicData, 118> Atom::properties = {{
        {0.0000, 0.0,  0, 0.0, sf::Color::Transparent       },
        {1.0080, 0.5,  1, 0.0, sf::Color(255, 255, 255, 255)},  // Водород
        {4.0026, 0.31, 0, 0.0, sf::Color(0,   0,   0,   255)},  // Гелий
        {6.9390, 1.67, 1, 0.0, sf::Color(0,   0,   0,   255)},  // Литий
        {9.0122, 1.12, 2, 0.0, sf::Color(0,   0,   0,   255)},  // Бериллий
        {10.811, 0.5,  3, 0.0, sf::Color(0,   0,   0,   255)},  // Бор
        {12.011, 0.5,  4, 0.0, sf::Color(0,   0,   0,   255)},  // Углерод
        {14.007, 0.5,  5, 0.0, sf::Color(80,  70,  230, 255)},  // Азот
        {15.999, 0.5,  2, 0.0, sf::Color(255, 50,  50,  255)},  // Кислород
        {18.998, 0.42, 1, 0.0, sf::Color(30,  255, 0,   255)},  // Фтор
        {20.179, 0.38, 0, 0.0, sf::Color(0,   0,   0,   255)},  // Неон
        {22.990, 1.90, 1, 0.0, sf::Color(0,   0,   0,   255)},  // Натрий
        {24.305, 1.45, 2, 0.0, sf::Color(0,   0,   0,   255)},  // Марганец
        {26.981, 1.18, 3, 0.0, sf::Color(0,   0,   0,   255)},  // Алюминий
        {28.086, 1.11, 4, 0.0, sf::Color(0,   0,   0,   255)},  // Кремний
        {30.974, 0.98, 5, 0.0, sf::Color(255, 150, 0,   255)},  // Фосфор
        {32.064, 0.88, 6, 0.0, sf::Color(255, 255, 0,   255)},  // Сера
        {35.453, 0.79, 7, 0.0, sf::Color(0,   255, 144, 255)},  // Хлор
        {39.948, 0.71, 0, 0.0, sf::Color(0,   0,   0,   255)},  // Аргон
    }};

void Atom::setGrid(SpatialGrid* grid_ptr) {
    grid = grid_ptr;
}

Atom::Atom(Vec3D start_coords, Vec3D start_speed, int type, bool fixed) : coords(start_coords), speed(start_speed), type(type), isFixed(fixed), acceleration(0, 0), PrevAcceleration(0, 0) {
    valence = getProps().maxValence;
    bonds.reserve(getProps().maxValence);
    Bond::bond_default_props.init();}
    //grid->insert((int)round(start_coords.x), (int)round(start_coords.y), this);}

void Atom::PredictPosition(double deltaTime) {
    int prev_x = (int)round(coords.x), prev_y = (int)round(coords.y);
    
    if (isFixed == false)
        Verlet(deltaTime);

    //Bounce();
    SoftWalls(deltaTime); 
    
    int curr_x = (int)round(coords.x), curr_y = (int)round(coords.y);
    if (prev_x != curr_x || prev_y != curr_y) {
        grid->erase(prev_x, prev_y, this);
        grid->insert(curr_x, curr_y, this);
    }

    PrevAcceleration = acceleration;
    acceleration = Vec3D(0, 0, 0);
}

void Atom::Bounce() {
    // Отражение от стен
    double x = coords.x, y = coords.y;
    if (x < 2 || x > grid->sizeX-3) {
        if (x < 2) coords.x = 2;
        else coords.x = grid->sizeX-3;
        speed.x = -speed.x;
    } 
    if (y < 2 || y > grid->sizeY-3) {
        if (y < 2) coords.y = 2;
        else coords.y = grid->sizeY-3;
        speed.y = -speed.y;
    }
}

void Atom::SoftWalls(double deltaTime) {
    double k = 1;
    double x = coords.x, y = coords.y, z = coords.z;

    // расстояния до краёв
    int distL = x;
    int distR = (grid->sizeX - 1) - x;

    int distT = y;
    int distB = (grid->sizeY - 1) - y;

    int distF = z;
    int distBk = (3 - 1) - z;

    int border = 2;

    if (distL < border) {
        speed.x += k * pow(distL - border, 2) * deltaTime;
    }

    if (distR < border) {
        speed.x -= k * pow(distR - border, 2) * deltaTime;
    }

    if (distT < border) {
        speed.y += k * pow(distT - border, 2) * deltaTime;
    }

    if (distB < border) {
        speed.y -= k * pow(distB - border, 2) * deltaTime;
    }

    if (distF < border) {
        speed.z += k * pow(distF - border, 2) * deltaTime;
    }

    if (distBk < border) {
        speed.z -= k * pow(distBk - border, 2) * deltaTime;
    }

    if (x < 0 || x > grid->sizeX-1) {
        if (x < 0) coords.x = 0;
        else coords.x = grid->sizeX-1;
        speed.x = -speed.x * 0.8f;
    } 
    if (y < 0 || y > grid->sizeY-1) {
        if (y < 0) coords.y = 0;
        else coords.y = grid->sizeY-1;
        speed.y = -speed.y * 0.8f;
    }
}

void Atom::ComputeForces(double deltaTime) {
    int curr_x = (int)round(coords.x), curr_y = (int)round(coords.y);
    // проверка взаимодействий с соседними атомами
    for (int i = -2; i <= 2; ++i) {
        for (int j = -2; j <= 2; ++j) {
            if (auto cell = grid->at(curr_x - i, curr_y - j)) {
                for (Atom* other : *cell) {
                    // хитрожопая проверка
                    if (other <= this) continue;

                    Vec3D delta = coords - other->coords;
                    float distance = sqrt(delta.dot(delta));
                    
                    bool flag = false;
                    // for (Bond* bond : bonds) {
                    //     if (this == bond->a && other == bond->b) {
                    //         flag = true;
                    //     }
                    // }

                    if (getProps().maxValence - valence == 2) {
                        Bond::angleForce(this, bonds[0], bonds[1]);
                    }
                    
                    if (!flag) {
                        if (distance < 1.3 * r0 && valence > 0 && other->valence > 0) {
                            Bond::CreateBond(this, other);
                        }
                        Vec3D force = Force(this, other, deltaTime);
                        // std::cout << "<Morse Force>" << force.x << std::endl;
                        // acceleration = acceleration - force / getProps().mass;
                        // other->acceleration = other->acceleration + force / other->getProps().mass;
                    }
                }
            }
        }
    }
}

Vec3D Atom::Force(Atom *a, Atom *b, double dt) {
    Vec3D delta = b->coords - a->coords;
    float distance_sq = delta.dot(delta);

    // Вычисляем расстояние (избегаем деления на 0)
    float distance = sqrt(distance_sq);
    if (distance < 1e-10f) distance = 1e-10f;
    Vec3D normal = delta / distance;

    // float force_magnitude = CovalentForce(distance);
    float force_magnitude = MorseForce(distance);

    return normal * force_magnitude;
}

void Atom::Euler(double dt) {
    coords += speed * dt;
}

void Atom::Verlet(double dt) {
    // Предсказание новой позиции на основе предыдущей и ускорения
    coords += speed * dt * 0.8 + acceleration * 0.5 * dt * dt;
}

void Atom::CorrectVelosity(double dt) {
    // Обновление скорости с использованием среднего ускорения
    speed += (PrevAcceleration + acceleration) * 0.5 * dt;
    //speed = speed * 0.999; // небольшое гашение колебаний
}

float Atom::MorseForce(float distanse) {
    float exp_a = std::exp(-a * (distanse - r0));
    return 2 * De * a * (exp_a * exp_a - exp_a);
}

float Atom::MorsePotential(float distanse) {
    float exponent = std::exp(-a * (distanse - r0));
    return De * (1 - exponent) * (1 - exponent);
}

float Atom::kineticEnergy() const {
    return 0.5f * getProps().mass * (speed.x * speed.x + speed.y * speed.y);
}

float Atom::pairPotentialEnergy(Atom* other) {
    float dx = other->coords.x - coords.x;
    float dy = other->coords.y - coords.y;
    float dist = std::sqrt(dx*dx + dy*dy);
    return MorsePotential(dist);
}
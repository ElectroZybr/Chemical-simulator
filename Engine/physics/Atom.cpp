#include "Atom.h"
#include <iostream>
#include <cmath>
// #include <fstream>
// #include <vector>

SpatialGrid* Atom::grid = nullptr;

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

Atom::Atom(float x, float y, int type, Vec2D start_speed, bool fixed) : coords(x, y), type(type), speed(start_speed), charge(0), prevCoords(x, y), isFixed(fixed) {
    valence = getProps().maxValence;
    connects.reserve(getProps().maxValence);}


void Atom::Update(double deltaTime) {
    int prev_x = (int)round(coords.x), prev_y = (int)round(coords.y);
    
    if (isFixed == false)
        coords = coords + speed * deltaTime;
    Bounce();
    
    int curr_x = (int)round(coords.x), curr_y = (int)round(coords.y);
    if (prev_x != curr_x || prev_y != curr_y) {
        grid->erase(prev_x, prev_y, this);
        grid->insert(curr_x, curr_y, this);
    }  
    // Взаимодействие с соседними атомами
    Collision(curr_x, curr_y, deltaTime);
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

void Atom::Collision(int curr_x, int curr_y, double deltaTime) {
    // проверка коллизий
    // int x = (int)round(coords.x), y = (int)round(coords.y);
    for (int i = -2; i <= 2; ++i) {
        for (int j = -2; j <= 2; ++j) {
            // if (grid->at(x-i, y-j) != nullptr) {
                for (Atom* other : *grid->at(curr_x-i, curr_y-j)) {
                    // хитрожопая проверка
                    if(other <= this) continue;
                    // std::cout << "<Interaction> this: " << this << " | other: " << other << std::endl;

                    float dx = other->coords.x - coords.x;
                    float dy = other->coords.y - coords.y;
                    float distance_sq = dx*dx + dy*dy;

                    // Вычисляем расстояние (избегаем деления на 0)
                    float distance = sqrt(distance_sq);
                    if (distance < 1e-10f) distance = 1e-10f;

                    float nx = dx / distance;
                    float ny = dy / distance;


                    if (distance < 0.74) {

                        valence = 0;
                    }

                    float dt_mass = deltaTime / getProps().mass;
                    float other_dt_mass = deltaTime / other->getProps().mass;

                    if (valence < getProps().maxValence) {
                        float bond_force = a * (distance - r0);
                        speed.x += bond_force * nx * dt_mass;
                        speed.y += bond_force * ny * dt_mass;
                        other->speed.x -= bond_force * nx * other_dt_mass;
                        other->speed.y -= bond_force * ny * other_dt_mass;
                    } 


                    // if (distance > 0.74 * 1.5f) {
                    //     other->valence--;
                    //     valence--;
                    // }

                    // if (valence > 0) {
                        // Нормализованный вектор столкновения
                        

                        // float force_x = force_magnitude * nx;
                        // float force_y = force_magnitude * ny;
                        

                        
                        float force_magnitude = MorseForce(distance);
                        speed.x += force_magnitude * nx * dt_mass;
                        speed.y += force_magnitude * ny * dt_mass;
                        other->speed.x -= force_magnitude * nx * other_dt_mass;
                        other->speed.y -= force_magnitude * ny * other_dt_mass;

                        std::cout << "<Interaction> morse: " << force_magnitude << std::endl;
                    // }

                    // float current_kinetic = 0.5f * (getProps().mass * (speed.x*speed.x + speed.y*speed.y) + 
                    //            other->getProps().mass * (other->speed.x*other->speed.x + other->speed.y*other->speed.y));
                    // float current_potential = MorsePotential(distance);
                    // float current_total = current_kinetic + current_potential;
                    
                    // energy = current_total;


                    // // логи
                    // static int counter = 0;
                    // counter++;
                    // if (counter == 20) {
                    //     if (!isFixed)
                    //         std::cout << "<Interaction> kinetic: " << current_kinetic << " | potential: " << current_potential << " | total: " << current_total << std::endl;
                    //     counter = 0;
                    // }

                    // float energy_error = energy - current_total;

                    // const float EPSILON = 1e-4f; 
                    // if (fabs(energy_error) > EPSILON) {
                    //     float total_mass = getProps().mass + other->getProps().mass;
                    //     float correction_factor = sqrt(1.0f + energy_error / current_kinetic);

                    //     speed.x *= correction_factor;
                    //     speed.y *= correction_factor;
                    //     // speed.x += (fabs(temp_energy - energy) * nx) / getProps().mass * deltaTime;
                    //     // speed.y += (fabs(temp_energy - energy) * ny) / getProps().mass * deltaTime;
                    // }


                    // if (temp_energy < energy) {
                    //     speed.x += ((temp_energy - energy) * nx) / getProps().mass * deltaTime;
                    //     speed.y += ((temp_energy - energy) * ny) / getProps().mass * deltaTime;
                    // }
                    // if (temp_energy > energy) {
                    //     speed.x -= ((energy - temp_energy) * nx) / getProps().mass * deltaTime;
                    //     speed.y -= ((energy - temp_energy) * ny) / getProps().mass * deltaTime;
                    // }

                    // energy = current_total;


                    // // Вычисляем изменение потенциальной энергии
                    // float current_U = MorsePotential(distance);
                    // // float prev_distance = /* предыдущее расстояние */;
                    // float prev_U = MorsePotential(prev_distance);
                    // float delta_U = current_U - prev_U;

                    // // Вычисляем работу как изменение кинетической энергии
                    // float delta_K = -delta_U; // По закону сохранения энергии

                    // // Распределяем изменение кинетической энергии между атомами
                    // float total_mass = getProps().mass + other->getProps().mass;
                    // float ratio1 = other->getProps().mass / total_mass;
                    // float ratio2 = getProps().mass / total_mass;

                    // // Вычисляем изменение скоростей
                    // float speed_change = sqrt(2 * fabs(delta_K) / total_mass);
                    // if (delta_K < 0) speed_change = -speed_change;

                    // speed.x += speed_change * nx * ratio1;
                    // speed.y += speed_change * ny * ratio1;
                    // // other->speed.x -= speed_change * nx * ratio2;
                    // // other->speed.y -= speed_change * ny * ratio2;

                    // // Обновляем "предыдущее расстояние" для следующего шага
                    // // prev_distance = distance;


                    // energy = force_magnitude + fabs(speed.x * getProps().mass) + fabs(speed.y * getProps().mass);















                    // float delta_potential = MorsePotential(distance) - MorsePotential(r0);
                    // speed.x += energy * nx / getProps().mass;
                    // speed.y += energy * ny / getProps().mass;
                    
                    // other->speed.x -= morse * nx / other->getProps().mass;
                    // other->speed.y -= morse * nx / other->getProps().mass;
                    
                    // energy -= morse * nx / getProps().mass + morse * ny / getProps().mass;
                    // if (energy < 0) energy = 0;

                    // if (morse > 0) {
                    //     energy += morse * nx;
                    //     speed.x += morse * nx;
                    // }
                    // if (morse < 0) {
                    //     if (energy > 0) {
                    //         energy -= morse * nx;
                    //         speed.x += morse * nx;
                    //     }
                    // }
                    // float energy_transfer = force_magnitude * 0.5f;

                    // float work = force_magnitude * (distance - r0) * 0.5f;
                    // if (work < 0) { // Отталкивание 
                    //     if (energy > work) {
                    //         // std::cout << "?????????????" << std::endl;
                    //         energy -= work;
                    //         speed.x += force_x / getProps().mass;
                    //         speed.y += force_y / getProps().mass;
                    //     }
                    // } else { // Притяжение
                    //     energy += -work; // force_magnitude отрицательный
                    //     // std::cout << "?????????????" << std::endl;;
                    //     speed.x += force_x / getProps().mass;
                    //     speed.y += force_y / getProps().mass;
                    // }
                    // speed.x += morse;
                    // other->speed.x -= force_x / getProps().mass;
                    // other->speed.y -= force_y / getProps().mass;
                    // if (energy < 0) energy = 0;

                    // energy += delta_potential * 0.5f;
                    // other->energy += delta_potential * 0.5f;

                    // if (!isFixed)
                    //     std::cout << "<Interaction> Work: " << " | energy: " << energy << std::endl;
                    

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
    // return -pow(1 / distanse, 2);
}

float Atom::MorsePotential(float distanse) {
    float exponent = std::exp(-a * (distanse - r0));
    return De * (1 - exponent) * (1 - exponent);
}
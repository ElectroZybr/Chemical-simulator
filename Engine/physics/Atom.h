#include <SFML/Graphics/Color.hpp>
#include <array>
#include "..\math\Vec2D.h"
#include "SpatialGrid.h"


// Общие данные для всех атомов одного типа
struct StaticAtomicData{
    const double mass;
    const double radius;
    const char maxValence;
    const double defaultCharge;
    const sf::Color color;
};


class Atom {
private:
    static SpatialGrid* grid;
    static const std::array<StaticAtomicData, 118> properties;
public:
    Vec2D coords;
    Vec2D prevCoords;
    Vec2D speed;
    int type;
    double charge;
    double energy;
    int valence;
    float r0 = 0.74;
    float De = 0.52;
    float a = 10;
    bool isFixed = false;
    float prev_distance = 0;

    Atom (float x, float y, int type, Vec2D start_speed, bool fixed = false);

    void Update(double deltaTime);
    void Bounce();
    void Collision(int curr_x, int curr_y, double deltaTime);

    float MorseForce(float distanse);
    float MorsePotential(float distanse);

    static void setGrid(SpatialGrid* grid);

    const StaticAtomicData& getProps() const {
        return properties.at(type);
    }
};
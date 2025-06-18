#include <SFML/Graphics/Color.hpp>
#include <array>
#include "..\math\Vec2D.h"
#include "SpatialGrid.h"


// Общие данные для всех атомов одного типа
struct StaticAtomicData{
    const double mass;
    const double radius;
    const double defaultCharge;
    const sf::Color color;
// Другие константные характеристики
};


class Atom {
private:
    static SpatialGrid* grid;
    static const std::array<StaticAtomicData, 118> properties;
    
    // static const std::unordered_map<int, AtomTypeData> typeData;
    // static StaticAtomicData[118]
public:
    Vec2D coords;
    Vec2D prevCoords;
    Vec2D speed;
    int type;
    double charge;
    double energy;
    int valence;
    float r0 = 0.74;
    float De = 4.52;
    float a = 1.92;
    bool isFixed = false;

    Atom (float x, float y, int type, Vec2D start_speed, bool fixed = false);

    void Update(double deltaTime);
    void Bounce();
    void Collision();

    float MorseForce(float distanse);
    float MorsePotential(float distanse);

    static void setGrid(SpatialGrid* grid);

    const StaticAtomicData& getProps() const {
        return properties.at(type);
    }
};
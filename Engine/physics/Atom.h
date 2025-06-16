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
    Vec2D speed;
    int type;
    double charge;
    Atom (float x, float y, int type, Vec2D start_speed);

    void Update(double deltaTime);

    static void setGrid(SpatialGrid* grid);

    const StaticAtomicData& getProps() const {
        return properties.at(type);
    }
};
#include <SFML/Graphics/Color.hpp>
#include "..\math\Vec2D.h"
// #include <unordered_map>
#include <array>


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
    static const std::array<StaticAtomicData, 118> properties;
    
    // static const std::unordered_map<int, AtomTypeData> typeData;
    // static StaticAtomicData[118]
public:
    Vec2D coords;
    int type;
    double charge;
    Atom (float x, float y, int type);


    const StaticAtomicData& getProps() const {
        return properties.at(type);
    }
    // const AtomTypeData& getTypeData() const {
    //     return typeData.at(type);
    // }
};
#include <SFML/Graphics.hpp>
#include <functional>

class Button {
private:
    sf::RectangleShape _collider;
    const std::function<void()> _click;
    const std::string _texture;

    bool _selected = false;
    bool _pressed = false;
    bool _checkBox = false;
public:
    Button(sf::RectangleShape collider, std::function<void()> click, std::string texture);

    void select();

    void unSelect();

    void press();

    void init();
};

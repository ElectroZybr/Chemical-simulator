#include "Mouse.h"
#include "../utils/Time.h"
#include "../Consts.h"

Vec2D Mouse::getMousePosition() const {
    sf::Vector2<int> pos = sf::Mouse::getPosition(*_screen->renderWindow());
    return Vec2D(pos.x, pos.y);
}

Vec2D Mouse::getMouseDisplacement() const {
    sf::Vector2<int> mousePos = sf::Mouse::getPosition(*_screen->renderWindow());
    sf::Vector2<int> center = sf::Vector2<int>(_screen->width() / 2, _screen->height() / 2);

    sf::Vector2<int> displacement = mousePos - center;
    setMouseInCenter();
    return Vec2D(displacement.x, displacement.y);
}

void Mouse::setMouseInCenter() const {
    sf::Vector2<int> center = sf::Vector2<int>(_screen->width() / 2, _screen->height() / 2);
    sf::Mouse::setPosition(center,*_screen->renderWindow());
}

bool Mouse::isButtonPressed(sf::Mouse::Button button) {
    return sf::Mouse::isButtonPressed(button);
}

bool Mouse::isButtonTapped(sf::Mouse::Button button) {
    if (!Mouse::isButtonPressed(button)) {
        return false;
    }

    if (_tappedButtons.count(button) == 0) {
        _tappedButtons.emplace(button, Time::time());
        return true;
    } else if ((Time::time() - _tappedButtons[button]) > Consts::TAP_DELAY) {
        _tappedButtons[button] = Time::time();
        return true;
    }
    return false;
}
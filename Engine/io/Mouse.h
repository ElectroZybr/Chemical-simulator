#ifndef SHOOTER_MOUSE_H
#define SHOOTER_MOUSE_H

#include <memory>
#include <utility>

#include "Screen.h"
#include "../math/Vec2D.h"

class Mouse final {
private:
    const std::shared_ptr<Screen> _screen;

    std::map<sf::Mouse::Button, double> _tappedButtons;
public:
    explicit Mouse(std::shared_ptr<Screen> screen) : _screen(std::move(screen)) {};

    // returns true if this _button is _pressed
    static bool isButtonPressed(sf::Mouse::Button button);

    // returns true if this _button is tapped and 1/5 sec passed (_button bouncing problem solved)
    bool isButtonTapped(sf::Mouse::Button button);

    [[nodiscard]] Vec2D getMousePosition() const;

    [[nodiscard]] Vec2D getMouseDisplacement() const;

    void setMouseInCenter() const;
};


#endif

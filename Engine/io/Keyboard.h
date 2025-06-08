//
// Created by Иван Ильин on 19.09.2021.
//

#ifndef SHOOTER_KEYBOARD_H
#define SHOOTER_KEYBOARD_H

#include <SFML/Graphics.hpp>

class Keyboard final {
private:
    std::map<sf::Keyboard::Key, double> _tappedKeys;
public:
    Keyboard() = default;

    // returns true if this key is _pressed
    static bool isKeyPressed(sf::Keyboard::Key key);

    // returns true if this key is tapped and 1/5 sec passed (_button bouncing problem solved)
    bool isKeyTapped(sf::Keyboard::Key key);
};


#endif //SHOOTER_KEYBOARD_H

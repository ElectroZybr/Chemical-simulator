//
// Created by Иван Ильин on 19.09.2021.
//

#include "Keyboard.h"
#include "../utils/Time.h"
#include "../Consts.h"

bool Keyboard::isKeyPressed(sf::Keyboard::Key key) {
    return sf::Keyboard::isKeyPressed(key);
}

bool Keyboard::isKeyTapped(sf::Keyboard::Key key) {
    if (!Keyboard::isKeyPressed(key)) {
        return false;
    }

    if (_tappedKeys.count(key) == 0) {
        _tappedKeys.emplace(key, Time::time());
        return true;
    } else if ((Time::time() - _tappedKeys[key]) > Consts::TAP_DELAY) {
        _tappedKeys[key] = Time::time();
        return true;
    }
    return false;
}
//
// Created by Иван Ильин on 07.10.2021.
//

// #ifndef SHOOTER_CONSTS_H
// #define SHOOTER_CONSTS_H

// #include <SFML/Graphics.hpp>

#include "math/Vec2D.h"

namespace Consts {
    const int STANDARD_SCREEN_WIDTH = 1920;
    const int STANDARD_SCREEN_HEIGHT = 1080;
//     const sf::Color BACKGROUND_COLOR = sf::Color(255, 255, 255);
    const std::string PROJECT_NAME = "engine";
    const bool USE_LOG_FILE = true;
    const bool USE_OPEN_GL = false;
    const bool SHOW_DEBUG_INFO = false;
    const bool SHOW_FPS_COUNTER = false;

    const double PI = 3.14159265358979323846264338327950288;
    const double EPS = 0.000001;

    const double EPA_EPS = 0.0001;

    const double RAY_CAST_MAX_DISTANCE = 10000;

    const std::string THIN_FONT = "engine/fonts/Roboto-Thin.ttf";
    const std::string MEDIUM_FONT = "engine/fonts/Roboto-Medium.ttf";

    const double LARGEST_TIME_STEP = 1.0 / 15.0;
    const double TAP_DELAY = 0.2;

    const Vec2D BEZIER[2] = {Vec2D{0.8, 0}, Vec2D{0.2, 1}};

    const unsigned NETWORK_VERSION = 3U;
    const int NETWORK_TIMEOUT = 5U;
    const int NETWORK_WORLD_UPDATE_RATE = 30;
    const double NETWORK_RELIABLE_RETRY_TIME = 1.0 / 20;
    const uint16_t NETWORK_MAX_CLIENTS = 64;

//     const sf::Color WHITE_COLORS[] = {
//             sf::Color(137, 135, 222), // blue
//             sf::Color(195, 155, 209), // pink
//             sf::Color(201, 137, 137), // red
//             sf::Color(116, 204, 135), // green
//             sf::Color(201, 171, 137), // orange
//     };

//     const sf::Color DARK_COLORS[] = {
//             sf::Color(16, 18, 69), // blue
//             sf::Color(77, 0, 62), // pink
//             sf::Color(99, 20, 20), // red
//             sf::Color(12, 46, 9), // green
//             sf::Color(97, 70, 51), // orange
//     };
}

// #endif //SHOOTER_CONSTS_H

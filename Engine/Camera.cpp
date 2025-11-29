#include <cmath>
#include "Camera.h"
#include "../interface.h"
#include <iostream>


Camera::Camera(sf::RenderWindow& window, sf::View* view, float moveSpeed, float zoomSpeed) 
    : view(view), position(0, 0), zoom(20.f), moveSpeed(moveSpeed), zoomSpeed(zoomSpeed),
        isDragging(false), lastMousePos(0, 0) {}
    
void Camera::update(float deltaTime, sf::RenderWindow& window) {
    view->setCenter(position);
    view->setSize(sf::Vector2f(window.getSize()) / zoom);
}

void Camera::move(float offsetX, float offsetY) {
    position.x += offsetX;
    position.y += offsetY;
}

void Camera::setPosition(float x, float y) {
    position.x = x;
    position.y = y;
}

sf::Vector2f Camera::getPosition() const {
    return position;
}

void Camera::zoomAt(float factor, sf::Vector2f mousePos, sf::RenderWindow& window) {
    // Изменяем уровень зума с учетом направления к курсору
    float prevZoom = zoom;
    zoom *= (1.f + factor * zoomSpeed);
    zoom = std::max(1.f, std::min(zoom, 500.f));

    speed = moveSpeed / zoom;
    
    // Плавное следование за указателем мыши при зуме
    if (zoom > 1.f && zoom < 500.f) {
        sf::Vector2i deltaPos = sf::Mouse::getPosition(window) - sf::Vector2i(window.getSize()) / 2;
        position += sf::Vector2f(deltaPos) * 0.1f / zoom * factor;
    }
}

float Camera::getZoom() const {
    return zoom;
}

void Camera::handleInput(float deltaTime, sf::RenderWindow& window) {
    // Управление WASD
    float deltaSpeed = speed * deltaTime;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) move(0, -deltaSpeed);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) move(0, deltaSpeed);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) move(-deltaSpeed, 0);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) move(deltaSpeed, 0);
}

void Camera::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    if (!Interface::cursorHovered && event.type == sf::Event::MouseButtonPressed && 
        event.mouseButton.button == sf::Mouse::Right) {
        // Начало перетаскивания
        isDragging = true;
        dragStartPixelPos = sf::Mouse::getPosition(window);
        dragStartCameraPos = position;
    } 
    else if (event.type == sf::Event::MouseMoved && isDragging) {
        // Перемещение камеры при перетаскивании
        sf::Vector2i currentPixelPos = sf::Mouse::getPosition(window);
        sf::Vector2f deltaPixel = sf::Vector2f(dragStartPixelPos.x - currentPixelPos.x,
                                               dragStartPixelPos.y - currentPixelPos.y);
        
        // Преобразуем разницу в пикселях в мировые координаты
        sf::Vector2f deltaWorld = window.mapPixelToCoords(
            sf::Vector2i(deltaPixel.x, deltaPixel.y), *view
        ) - window.mapPixelToCoords(sf::Vector2i(0, 0), *view);
        
        position = dragStartCameraPos + deltaWorld;
    }
    else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right) {
        // Конец перетаскивания
        isDragging = false;
    }
    else if (event.type == sf::Event::MouseWheelScrolled) {
        if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
            zoomAt(event.mouseWheelScroll.delta, sf::Vector2f(event.mouseWheelScroll.x, event.mouseWheelScroll.y), window);
        }
    }
}
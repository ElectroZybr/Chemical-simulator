#include "Button.h"

Button::Button(sf::RectangleShape collider, std::function<void()> click, std::string texture)
      : _collider(collider), _click(std::move(click)),
        _texture(std::move(texture)) {

}

void Button::select() {
    if (!_selected && !_pressed) {
        _button.setTextureRect(sf::IntRect(_selectedState.tx, _selectedState.ty, _w, _h));
        _selected = true;
    }
}

void Button::unSelect() {
    if (_selected && !_pressed) {
        _button.setTextureRect(sf::IntRect(_usualState.tx, _usualState.ty, _w, _h));
        _selected = false;
    }
}

void Button::press() {
    if (!_pressed) {
        _button.setTextureRect(sf::IntRect(_pressedState.tx, _pressedState.ty, _w, _h));
        if (_checkBox) {
            _pressed = true;
        }
        _click();
    } else {
        _button.setTextureRect(sf::IntRect(_usualState.tx, _usualState.ty, _w, _h));
        if (_checkBox) {
            _pressed = false;
        }
    }
}


#include <SFML/Graphics.hpp>
#include <functional>
#include <memory>

class Button {
private:
    sf::RectangleShape _collider;
    std::function<void()> _click;
    std::string _texturePath;
    
    // Текстура и спрайт для отображения
    std::shared_ptr<sf::Texture> _texture;
    sf::Sprite _button;
    
    // Состояния кнопки
    struct StateCoords {
        int tx, ty;
    } _usualState, _selectedState, _pressedState;
    
    int _w, _h; // Ширина и высота текстуры
    
    bool _selected = false;
    bool _pressed = false;
    bool _checkBox = false;

public:
    // Конструктор
    Button(sf::RectangleShape collider, 
           std::function<void()> click, 
           std::string texturePath,
           StateCoords usual = {0, 0},
           StateCoords selected = {0, 0},
           StateCoords pressed = {0, 0},
           int width = 0, int height = 0,
           bool isCheckBox = false)
        : _collider(collider),
          _click(std::move(click)),
          _texturePath(std::move(texturePath)),
          _usualState(usual),
          _selectedState(selected),
          _pressedState(pressed),
          _w(width),
          _h(height),
          _checkBox(isCheckBox) {
        init();
    }

    // Инициализация текстуры
    void init() {
        _texture = std::make_shared<sf::Texture>();
        if (!_texture->loadFromFile(_texturePath)) {
            throw std::runtime_error("Failed to load button texture: " + _texturePath);
        }
        
        _button.setTexture(*_texture);
        _button.setTextureRect(sf::IntRect(_usualState.tx, _usualState.ty, _w, _h));
        _button.setPosition(_collider.getPosition());
    }

    // Обработка выбора (наведения)
    void select() {
        if (!_selected && !_pressed) {
            _button.setTextureRect(sf::IntRect(_selectedState.tx, _selectedState.ty, _w, _h));
            _selected = true;
        }
    }

    // Снятие выбора
    void unSelect() {
        if (_selected && !_pressed) {
            _button.setTextureRect(sf::IntRect(_usualState.tx, _usualState.ty, _w, _h));
            _selected = false;
        }
    }

    // Нажатие кнопки
    void press() {
        if (!_pressed) {
            _button.setTextureRect(sf::IntRect(_pressedState.tx, _pressedState.ty, _w, _h));
            if (_checkBox) {
                _pressed = true;
            }
            _click();
        } else {
            _button.setTextureRect(sf::IntRect(_usualState.tx, _usualState.ty, _w, _h));
            if (_checkBox) {
                _pressed = false;
            }
        }
    }

    // Проверка, содержит ли кнопка точку
    bool contains(const sf::Vector2f& point) const {
        return _collider.getGlobalBounds().contains(point);
    }

    // Отрисовка кнопки
    void draw(sf::RenderWindow& window) const {
        window.draw(_button);
    }

    // Получение состояния (для чекбоксов)
    bool isPressed() const { return _pressed; }

    // Установка позиции
    void setPosition(const sf::Vector2f& position) {
        _collider.setPosition(position);
        _button.setPosition(position);
    }

    // Получение позиции
    sf::Vector2f getPosition() const {
        return _collider.getPosition();
    }
};
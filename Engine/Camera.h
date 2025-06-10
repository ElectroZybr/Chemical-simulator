#include <SFML/Graphics.hpp>
#include <cmath>

class Camera {
private:
    sf::View* view;
    sf::Vector2f position;
    float zoom;
    float speed;
    float moveSpeed;
    float zoomSpeed;

    bool isDragging;
    sf::Vector2f lastMousePos;

    sf::Vector2i dragStartPixelPos;
    sf::Vector2f dragStartCameraPos;
    
public:
    Camera(sf::RenderWindow& window, sf::View& view, float moveSpeed = 500.f, float zoomSpeed = 0.1f);
    
    void update(float deltaTime, sf::RenderWindow& window);
    
    void move(float offsetX, float offsetY);
    
    void setPosition(float x, float y);
    
    sf::Vector2f getPosition() const;
    
    void zoomAt(float factor, sf::Vector2f mousePos, sf::RenderWindow& window);
    
    float getZoom() const;
    
    const sf::View& getView() const;
    // void resize(sf::Vector2f newSize);
    
    void handleInput(float deltaTime, sf::RenderWindow& window);
    void handleEvent(const sf::Event& event, sf::RenderWindow& window);
};
#include <SFML/Graphics.hpp>
#include <SFML/System/Time.hpp>

int main() {
    sf::Clock clock;
    sf::RenderWindow window(sf::VideoMode({200, 200}), "Arcanoid");

    auto shape = sf::RectangleShape({100.f, 100.f});
    shape.setFillColor(sf::Color::Green);

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
        }
        auto size = sf::Vector2f{
            std::abs(100.f * std::sin(clock.getElapsedTime().asSeconds())),
            std::abs(100.f * std::cos(clock.getElapsedTime().asSeconds()))
        };

        shape.setSize(size);
        shape.setOrigin({size.x / 2.f, size.y / 2.f});
        shape.setPosition({100.f, 100.f});

        window.clear();
        window.draw(shape);
        window.display();
    }
}
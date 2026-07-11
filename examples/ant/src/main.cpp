#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <random>
#include <ranges>

#include "world.hpp"

static constexpr sf::Vector2u kModeSize{200, 200};

using namespace NCecs;

struct TPosition {
    sf::Vector2f pos;
};

struct TVelocity {
    sf::Vector2f dir;
    float vel;
};

struct TAnt {};

struct TFood {
    std::size_t capacity;
};

struct TColony {
    std::size_t ant_capacity;
    std::size_t food_capacity;
};

// clang-format off
using TAntWorld = TWorld<
    TTypeList<
        TTypeList<TPosition, TAnt>,
        TTypeList<TPosition, TAnt, TVelocity>,
        TTypeList<TPosition, TFood>,
        TTypeList<TPosition, TColony>
    >
>;

// clang-format on

class TGame {
  public:
    TGame(sf::RenderWindow* window)
        : window_(window) {
    }

    void init() {
        std::random_device rnd;
        std::mt19937 gen(rnd());
        std::uniform_real_distribution<float> x_dist{0, kModeSize.x};
        std::uniform_real_distribution<float> y_dist{0, kModeSize.y};
        for (auto i : std::views::iota(0, 1 << 10)) {
            sf::Vector2f pos{x_dist(gen), y_dist(gen)};
            world_.create<TPosition, TAnt>(TPosition{.pos = pos}, TAnt{});
        }
        world_.commit();
    }

    void update(float dt) {
        draw(dt);
        world_.commit();
    }

    void handle_event(const sf::Event& event) {
        if (auto* key_event = event.getIf<sf::Event::KeyPressed>()) {
            if (key_event->scancode == sf::Keyboard::Scancode::Space) {
                deinit();
                init();
                return;
            }
        }
    }

    void deinit() {
    }

  private:
    void draw(float dt) {
        world_.select<TPosition>().run([&](auto entity) {
            if constexpr (entity.template has<TAnt>()) {
                sf::Vertex vert(
                    entity.template get<TPosition>().pos, sf::Color::Magenta
                );
                window_->draw(&vert, 1, sf::PrimitiveType::Points);
            }
        });
    }

  private:
    sf::RenderWindow* window_;
    TAntWorld world_;
};

int main() {
    sf::VideoMode video_mode{kModeSize};
    sf::RenderWindow window(video_mode, "Ant");
    sf::Clock clock;

    TGame game(&window);
    game.init();

    clock.reset();
    while (window.isOpen()) {
        while (const auto& event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            game.handle_event(*event);
        }

        float dt = clock.reset().asSeconds();
        window.clear();
        game.update(dt);
        window.display();
    }

    game.deinit();

    return 0;
}
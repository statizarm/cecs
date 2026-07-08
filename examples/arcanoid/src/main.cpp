#include <SFML/Graphics.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <iostream>

#include "world.hpp"

using namespace NCecs;

struct TPosition {
    sf::Vector2f pos;
};

struct TBlock {
    sf::Vector2f size;
};

struct TBall {
    float radius;
};

struct TPlatform {
    sf::Vector2f size;
};

struct TVelocity {
    sf::Vector2f dir;
    float vel;
};

// clang-format off
using TArcanoidWorld = TWorld<
    TTypeList<
        TTypeList<TPosition, TBlock>,
        TTypeList<TPosition, TBall>,
        TTypeList<TPosition, TPlatform>,
        TTypeList<TPosition, TBall, TVelocity>,
        TTypeList<TPosition, TPlatform, TVelocity>
    >
>;
// clang-format on

static constexpr sf::Vector2u kModSize = {200, 200};

static constexpr float kEps = 1e-06;

static constexpr sf::Vector2f kDefaultBallPosition = {100.f, 190.f};
static constexpr float kDefaultBallVelocity        = {1000.f};
static constexpr float kDefaultBallRadius          = {2.f};

static constexpr sf::Vector2f kDefaultPlatformPosition = {100.f, 195.f};
static constexpr sf::Vector2f kDefaultPlatformSize     = {50.f, 4.f};
static constexpr float kDefaultPlatformVelocity        = {1000.f};

enum class EPlatformDirection {
    LEFT,
    RIGHT,
};

class TGame {
  public:
    TGame(sf::RenderWindow* window)
        : window_(window) {
    }

    void init(std::size_t level = 0) {
        current_level_ = level;
        init_level();
    }

    void deinit() {
        world_.clear();
    }

    void update(float dt) {
        static constexpr std::size_t kPhysicsIterations = 8;
        for (std::size_t i = 0; i < kPhysicsIterations; ++i) {
            move(dt / 8);
            resolve_collisions(dt / 8);
        }
        draw();
        world_.commit();
    }

    void handle_input(const sf::Event& event) {
        if (event.is<sf::Event::MouseButtonPressed>()) {
            shoot_ball(
                window_->mapPixelToCoords(sf::Mouse::getPosition(*window_))
            );
        } else if (
            auto* real_event = event.getIf<sf::Event::KeyPressed>();
            real_event && real_event->scancode == sf::Keyboard::Scancode::A
        ) {
            start_move_platform(EPlatformDirection::LEFT);
        } else if (
            auto* real_event = event.getIf<sf::Event::KeyPressed>();
            real_event && real_event->scancode == sf::Keyboard::Scancode::D
        ) {
            start_move_platform(EPlatformDirection::RIGHT);
        } else if (
            auto* real_event = event.getIf<sf::Event::KeyReleased>();
            real_event && real_event->scancode == sf::Keyboard::Scancode::A
        ) {
            stop_move_platform(EPlatformDirection::LEFT);
        } else if (
            auto* real_event = event.getIf<sf::Event::KeyReleased>();
            real_event && real_event->scancode == sf::Keyboard::Scancode::D
        ) {
            stop_move_platform(EPlatformDirection::RIGHT);
        }
    }

  private:
    void init_level() {
        create_ball();
        create_platform();
    }

    void create_ball() {
        auto ball_entity = world_.create<TPosition, TBall>(
            TPosition{.pos = kDefaultBallPosition},
            TBall{.radius = kDefaultBallRadius}
        );
        world_.commit();
    }

    void create_platform() {
        auto platform_entity = world_.create<TPosition, TPlatform>(
            TPosition{.pos = kDefaultPlatformPosition},
            TPlatform{.size = kDefaultPlatformSize}
        );
        world_.commit();
    }

    void shoot_ball(sf::Vector2f to) {
        world_.select<TPosition, TBall>().run([&](auto entity) {
            const auto& pos = entity.template get<TPosition>().pos;
            auto direction  = (to - pos).normalized();
            entity.template add<TVelocity>(direction, kDefaultBallVelocity);
        });
        world_.commit();
    }

    sf::Vector2f get_direction(EPlatformDirection dir) {
        if (dir == EPlatformDirection::LEFT) {
            return {-1.f, 0.f};
        } else if (dir == EPlatformDirection::RIGHT) {
            return {1.f, 0.f};
        }
        return {};
    }

    void start_move_platform(EPlatformDirection dir) {
        world_.select<TPlatform>().run([&](auto entity) {
            auto new_entity = entity.template add<TVelocity>(
                sf::Vector2f{0.f, 0.f}, kDefaultPlatformVelocity
            );

            auto& vel = new_entity.template get<TVelocity>();
            vel.dir += get_direction(dir);
            vel.vel = kDefaultPlatformVelocity;
            if (vel.dir.length() < kEps) {
                entity.template del<TVelocity>();
            }
            vel.dir = vel.dir.normalized();
        });
        world_.commit();
    }

    void stop_move_platform(EPlatformDirection dir) {
        world_.select<TPlatform>().run([&](auto entity) {
            auto new_entity = entity.template add<TVelocity>(
                sf::Vector2f{0.f, 0.f}, kDefaultPlatformVelocity
            );

            auto& vel = new_entity.template get<TVelocity>();
            vel.dir -= get_direction(dir);
            vel.vel = kDefaultPlatformVelocity;
            if (vel.dir.length() < kEps) {
                entity.template del<TVelocity>();
            }
            vel.dir = vel.dir.normalized();
        });
        world_.commit();
    }

    void move(float dt) {
        world_.select<TPosition, TVelocity>().run([dt](auto entity) {
            auto& pos       = entity.template get<TPosition>();
            const auto& vel = entity.template get<TVelocity>();
            pos.pos += dt * vel.vel * vel.dir;
        });
    }

    void resolve_collisions(float dt) {
        world_.select<TPosition, TVelocity, TBall>().run([dt](auto entity) {
            auto& pos = entity.template get<TPosition>().pos;
            auto r    = entity.template get<TBall>().radius;
            auto& dir = entity.template get<TVelocity>().dir;
            sf::Vector2f offset{0.f, 0.f};

            if (pos.x < r) {
                offset.x = (r - pos.x + kEps);
                dir.x    = -dir.x;
            } else if (pos.y < r) {
                offset.y = (r - pos.y + kEps);
                dir.y    = -dir.y;
            } else if (pos.x + r > kModSize.x) {
                offset.y = kModSize.x - (pos.x + r + kEps);
                dir.x    = -dir.x;
            } else if (pos.y + r > kModSize.y) {
                offset.y = kModSize.y - (pos.y + r + kEps);
                dir.y    = -dir.y;
            }

            if (offset.length() > 0) {
                std::cout << pos.x << ' ' << pos.y << std::endl;
                std::cout << offset.x << ' ' << offset.y << std::endl;
            }

            pos += offset;
        });
    }

    void draw() {
        world_.select<TPosition>().run([this](auto entity) {
            if constexpr (entity.template has<TBlock>()) {
                draw_block(
                    entity.template get<TPosition>(),
                    entity.template get<TBlock>()
                );
            } else if constexpr (entity.template has<TBall>()) {
                draw_ball(
                    entity.template get<TPosition>(),
                    entity.template get<TBall>()
                );
            } else if constexpr (entity.template has<TPlatform>()) {
                draw_platform(
                    entity.template get<TPosition>(),
                    entity.template get<TPlatform>()
                );
            }
        });
    }

    void draw_block(const TPosition& pos, const TBlock& block) {
        auto shape = sf::RectangleShape(block.size);
        shape.setFillColor(get_block_fill_color(block));
        shape.setOrigin({block.size.x / 2, block.size.y / 2});
        shape.setPosition(pos.pos);
        window_->draw(shape);
    }

    void draw_ball(const TPosition& pos, const TBall& ball) {
        auto shape = sf::CircleShape(ball.radius);
        shape.setFillColor(get_ball_fill_color(ball));
        shape.setPosition(pos.pos);
        window_->draw(shape);
    }

    void draw_platform(const TPosition& pos, const TPlatform& platform) {
        auto shape = sf::RectangleShape(platform.size);
        shape.setFillColor(get_platform_fill_color(platform));
        shape.setOrigin({platform.size.x / 2, platform.size.y / 2});
        shape.setPosition(pos.pos);
        window_->draw(shape);
    }

    sf::Color get_block_fill_color(const TBlock& block) {
        return sf::Color::Green;
    }

    sf::Color get_ball_fill_color(const TBall& ball) {
        return sf::Color::White;
    }

    sf::Color get_platform_fill_color(const TPlatform) {
        return sf::Color::White;
    }

  private:
    sf::RenderWindow* window_;
    TArcanoidWorld world_;
    std::size_t current_level_;
};

int main() {
    sf::Clock clock;
    sf::RenderWindow window(sf::VideoMode{kModSize}, "Arcanoid");

    TGame game(&window);

    game.init();

    clock.restart();
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            game.handle_input(*event);
        }

        window.clear();
        game.update(clock.getElapsedTime().asSeconds());
        window.display();

        clock.restart();
    }

    game.deinit();
}
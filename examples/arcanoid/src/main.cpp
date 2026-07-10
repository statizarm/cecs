#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <filesystem>
#include <iostream>
#include <random>

#include "utils.hpp"
#include "world.hpp"

using namespace NCecs;

struct TPosition {
    sf::Vector2f pos;
};

struct TBlock {
    sf::Vector2f size;
    std::size_t health;
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

template <CIsInstanceOf<TEntity> TLeft, CIsInstanceOf<TEntity> TRight>
struct TCollisionResponse {
    sf::Vector2f pos_offset;
    sf::Vector2f collision_normal;

    TLeft left;
    TRight right;
};

static constexpr sf::Vector2u kModSize = {200, 200};

static constexpr float kEps = 1e-06;

static constexpr sf::Vector2f kDefaultBallPosition = {100.f, 190.f};
static constexpr float kDefaultBallVelocity        = {200.f};
static constexpr float kDefaultBallRadius          = {2.f};

static constexpr std::size_t kWallHealth = static_cast<std::size_t>(-1);

static constexpr sf::Vector2f kDefaultPlatformPosition = {100.f, 195.f};
static constexpr sf::Vector2f kDefaultPlatformSize     = {50.f, 4.f};
static constexpr float kDefaultPlatformVelocity        = {200.f};

static constexpr std::size_t kMaxBlockHealth = 5;

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
        if (!ball_platfrom_sound_buffer_.loadFromFile(
                std::filesystem::current_path() /
                std::filesystem::path(
                    "assets/ball_platform_collision_sound.ogg"
                )
            )) {
            std::cerr << "Could not load ball_platform collision sound"
                      << std::endl;
        } else {
            ball_platform_sound_ = sf::Sound{ball_platfrom_sound_buffer_};
        }

        if (!ball_block_sound_buffer_.loadFromFile(
                std::filesystem::current_path() /
                std::filesystem::path("assets/ball_block_collision_sound.ogg")
            )) {
            std::cerr << "Could not load ball_block collision sound"
                      << std::endl;
        } else {
            ball_block_sound_ = sf::Sound{ball_block_sound_buffer_};
        }

        if (!ball_wall_sound_buffer_.loadFromFile(
                std::filesystem::current_path() /
                std::filesystem::path("assets/ball_wall_collision_sound.ogg")
            )) {
            std::cerr << "Could not load ball_wall collision sound"
                      << std::endl;
        } else {
            ball_wall_sound_ = sf::Sound{ball_wall_sound_buffer_};
        }

        init_level();
    }

    void deinit() {
        world_.clear();
    }

    void update(float dt) {
        static constexpr std::size_t kIterationsCount = 8;
        for (std::size_t i = 0; i < kIterationsCount; ++i) {
            move(dt / kIterationsCount);
            resolve_collisions(dt / kIterationsCount);
        }
        draw();
        world_.commit();

        if (is_loose()) {
            deinit();
            init(current_level_);
        }
        if (is_win()) {
            deinit();
            init(current_level_ + 1);
        }
    }

    bool is_loose() {
        return world_.select<TBall, TPosition>().run_batched(
            [](auto& subworld) {
                return std::distance(subworld.begin(), subworld.end()) == 0;
            }
        );
    }

    bool is_win() {
        return world_.select<TBlock, TPosition>().run_batched(
            [](auto& subworld) {
                for (auto entity : subworld) {
                    if (entity.template get<TBlock>().health != kWallHealth) {
                        return false;
                    }
                }
                return true;
            }
        );
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
        create_walls();

        static constexpr std::size_t kMinRows = 5;
        std::size_t rows_count                = kMinRows + current_level_;
        std::size_t columns_count             = 16;
        std::size_t max_health = std::min(1 + current_level_, kMaxBlockHealth);
        std::size_t min_health =
            std::max<std::size_t>(1, std::max<int>(0, -3 + current_level_));

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(min_health, max_health);

        for (std::size_t i = 0; i < rows_count; ++i) {
            for (std::size_t j = 0; j < columns_count; ++j) {
                sf::Vector2f size = {
                    static_cast<float>(kModSize.x) / columns_count,
                    kDefaultPlatformSize.y
                };
                sf::Vector2f pos = {
                    size.x * j + size.x / 2.f,
                    size.y * i + size.y / 2.f,
                };
                std::size_t health = dist(gen);
                world_.create<TPosition, TBlock>(
                    TPosition{.pos = pos},
                    TBlock{.size = size, .health = min_health}
                );
            }
        }
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

    void create_walls() {
        static constexpr std::array<std::pair<sf::Vector2f, sf::Vector2f>, 3>
            walls = {
                std::pair{
                    sf::Vector2f{0, kModSize.y / 2},
                    sf::Vector2f{2.f, kModSize.y}
                },
                std::pair{
                    sf::Vector2f{kModSize.x / 2, 0}, sf::Vector2f{kModSize.x, 2}
                },
                std::pair{
                    sf::Vector2f{kModSize.x, kModSize.y / 2},
                    sf::Vector2f{2.f, kModSize.y}
                },
            };
        for (const auto& wall : walls) {
            world_.create<TPosition, TBlock>(
                TPosition{.pos = wall.first},
                TBlock{.size = wall.second, .health = kWallHealth}
            );
        }
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
            } else {
                vel.dir = vel.dir.normalized();
            }
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
            } else {
                vel.dir = vel.dir.normalized();
            }
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

    template <CIsInstanceOf<TEntity> TLeft, CIsInstanceOf<TEntity> TRight>
    std::optional<TCollisionResponse<TLeft, TRight>> find_collision(
        TLeft left, TRight right
    ) {
        if constexpr (std::same_as<TLeft, TRight>) {
            if (left == right) {
                return {};
            }
        }

        sf::Vector2f pos_offset;
        sf::Vector2f collision_normal;

        if constexpr (
            left.template has<TBall>() &&
            (right.template has<TPlatform>() || right.template has<TBlock>())
        ) {
            sf::Vector2f aabb_pos = right.template get<TPosition>().pos;
            sf::Vector2f aabb_size;
            if constexpr (right.template has<TPlatform>()) {
                aabb_size = right.template get<TPlatform>().size;
            } else {
                aabb_size = right.template get<TBlock>().size;
            }
            sf::Vector2f circle_pos = left.template get<TPosition>().pos;
            float circle_radius     = left.template get<TBall>().radius;

            auto distance = circle_pos - aabb_pos;

            auto module_distance =
                sf::Vector2f{std::abs(distance.x), std::abs(distance.y)};
            auto diff = module_distance - (aabb_size / 2.f);

            if (distance.dot(distance) < aabb_size.dot(aabb_size) &&
                diff.x < circle_radius && diff.y < circle_radius) {
                if (std::abs(diff.x) < std::abs(diff.y)) {
                    collision_normal = {distance.x / std::abs(distance.x), 0.f};
                    pos_offset.x =
                        collision_normal.x * (circle_radius - diff.x + kEps);
                } else {
                    collision_normal = {0.f, distance.y / std::abs(distance.y)};
                    pos_offset.y =
                        collision_normal.y * (circle_radius - diff.y + kEps);
                }
            } else {
                return {};
            }
        } else if constexpr (
            left.template has<TPlatform>() && right.template has<TBlock>()
        ) {
            return {};
        } else {
            return {};
        }
        return {
            TCollisionResponse<TLeft, TRight>{
                .pos_offset       = pos_offset,
                .collision_normal = collision_normal,
                .left             = left,
                .right            = right,
            },
        };
    }

    template <CIsInstanceOf<TEntity> TLeft, CIsInstanceOf<TEntity> TRight>
    void handle_collision_response(TCollisionResponse<TLeft, TRight>& resp) {
        auto& pos = resp.left.template get<TPosition>().pos;
        pos       = pos + resp.pos_offset;
        if constexpr (TLeft::TArchetype::template has<TVelocity>::value) {
            auto& dir = resp.left.template get<TVelocity>().dir;
            dir       = (dir - 2.f * dir.projectedOnto(resp.collision_normal))
                            .normalized();
        }
        if constexpr (
            TLeft::TArchetype::template has<TBall>::value &&
            TRight::TArchetype::template has<TBlock>::value
        ) {
            auto& health = resp.right.template get<TBlock>().health;
            if (health != kWallHealth) {
                if (ball_block_sound_) {
                    ball_block_sound_->play();
                }
                --health;
            } else {
                if (ball_wall_sound_) {
                    ball_wall_sound_->play();
                }
            }
            if (health == 0) {
                resp.right.destroy();
            }
        } else if constexpr (
            TLeft::TArchetype::template has<TBall>::value &&
            TRight::TArchetype::template has<TPlatform>::value
        ) {
            if (ball_platform_sound_) {
                ball_platform_sound_->play();
            }
        }
        world_.commit();
    }

    void resolve_collisions(float dt) {
        auto& subworld = world_.select<TPosition>();
        world_.select<TPosition, TVelocity, TBall>().run([&](auto entity) {
            subworld.run([&](const auto positioned_entity) {
                auto collision_response =
                    find_collision(entity, positioned_entity);
                if (collision_response) {
                    handle_collision_response(*collision_response);
                }
            });
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
        auto border_shape = sf::RectangleShape(block.size);
        border_shape.setFillColor(sf::Color::Yellow);
        border_shape.setOrigin({block.size.x / 2, block.size.y / 2});
        border_shape.setPosition(pos.pos);
        window_->draw(border_shape);
        auto shape = sf::RectangleShape(block.size * 0.9f);
        shape.setFillColor(get_block_fill_color(block));
        shape.setOrigin({shape.getSize().x / 2, shape.getSize().y / 2});
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
        if (block.health == kWallHealth) {
            return sf::Color::White;
        }
        static constexpr std::array<sf::Color, kMaxBlockHealth> colors = {
            sf::Color::Green,
            sf::Color::Cyan,
            sf::Color::Blue,
            sf::Color::Magenta,
            sf::Color::Red,
        };
        return colors[block.health - 1];
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
    sf::SoundBuffer ball_platfrom_sound_buffer_;
    sf::SoundBuffer ball_block_sound_buffer_;
    sf::SoundBuffer ball_wall_sound_buffer_;
    std::optional<sf::Sound> ball_platform_sound_;
    std::optional<sf::Sound> ball_block_sound_;
    std::optional<sf::Sound> ball_wall_sound_;
};

int main() {
    sf::Clock clock;
    sf::RenderWindow window(sf::VideoMode{kModSize}, "Arcanoid");

    TGame game(&window);

    game.init();

    clock.restart();
    while (window.isOpen()) {
        float dt = clock.getElapsedTime().asSeconds();
        clock.restart();
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            game.handle_input(*event);
        }

        window.clear();
        game.update(dt);
        window.display();
    }

    game.deinit();
}
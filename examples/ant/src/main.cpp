#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <iostream>
#include <iterator>
#include <ostream>
#include <random>
#include <ranges>

#include "world.hpp"

static constexpr sf::Vector2u kModeSize{200, 200};

static constexpr std::size_t kMinInitialFoodCapacity = 600;
static constexpr std::size_t kMaxInitialFoodCapacity = 6000;

static constexpr std::size_t kMinInitialAntColonyCapacity = 1000;
static constexpr std::size_t kMaxAntColonyCapacity        = 10000;

static constexpr float kAntSearchingVelocity = 10.f;
static constexpr float kAntCarryingVelocity  = 5.5f;

static constexpr float kMinSearchingFeromone = 1.f;
static constexpr float kMinCarryingFeromone  = 1.f;

static constexpr float kAntFeromonesPerSecond       = 10.f;
static constexpr float kFeromoneDecreasingPerSecond = 10.f;

static constexpr std::size_t kPickFoodQuantity = 2;
static constexpr float kFoodPickDistance       = 1.f;
static constexpr float kEnterColonyDistance    = 1.f;
static constexpr float kDefaultAntLifetime     = 60.f;
static constexpr float kVisionRadius           = 10.f;

static sf::Font SFont;

using namespace NCecs;

enum class EAntState {
    SEARCHING_FOOD,
    CARRYING_FOOD_TO_COLONY,
};

struct TPosition {
    sf::Vector2f pos;
};

struct TVelocity {
    sf::Vector2f dir;
    float vel;
};

struct TAnt {
    EAntState state;
    float lifetime = kDefaultAntLifetime;
};

struct TFood {
    std::size_t capacity;
};

struct TColony {
    std::size_t ant_capacity;
};

struct TFeromone {
    float searching = kMinSearchingFeromone;
    float carrying  = kMinCarryingFeromone;
};

// clang-format off
using TAntWorld = TWorld<
    TTypeList<
        TTypeList<TPosition, TAnt, TVelocity>,
        TTypeList<TPosition, TFood>,
        TTypeList<TPosition, TColony>
    >
>;

// clang-format on

class TGame {
  public:
    TGame(sf::RenderWindow* window)
        : window_(window), world_(), gen_(), elapsed_time_(0.f) {
    }

    void init() {
        elapsed_time_ = 0.f;
        std::random_device rnd;
        gen_.seed(rnd());
        spawn_food();
        spawn_colony();
        init_feromone_map();
    }

    void update(float dt) {
        static constexpr std::size_t kIterationsCount = 1;

        ////////////////////////////////////////////////////////////
        float fps = 1 / dt;
        sf::Text text(SFont, std::format("FPS: {:2f}", fps), 8);
        text.setPosition({0.f, 0.f});
        window_->draw(text);
        ////////////////////////////////////////////////////////////

        if (std::floor(elapsed_time_ + dt) > std::floor(elapsed_time_)) {
            spawn_food();
        }
        elapsed_time_ += dt;

        for (auto _ : std::views::iota(std::size_t{0}, kIterationsCount)) {
            move(dt / kIterationsCount);
            update_ant_logic(dt);
        }

        draw(dt);
        world_.commit();

        for (auto& row : feromones_map_) {
            for (auto& feromone : row) {
                feromone.carrying = std::max(
                    feromone.carrying - kFeromoneDecreasingPerSecond * dt,
                    kMinCarryingFeromone
                );
                feromone.searching = std::max(
                    feromone.searching - kFeromoneDecreasingPerSecond * dt,
                    kMinSearchingFeromone
                );
            }
        }

        std::size_t ants_count =
            world_.select<TAnt>().run_batched([](auto& subworld) {
                return subworld.size();
            });

        sf::Text ants_count_text(
            SFont, std::format("ANTS COUNT: {}", ants_count), 8
        );
        ants_count_text.setPosition({0.f, 190.f});
        window_->draw(ants_count_text);
    }

    void handle_event(const sf::Event& event) {
        if (auto* key_event = event.getIf<sf::Event::KeyPressed>()) {
            if (key_event->scancode == sf::Keyboard::Scancode::Space) {
                deinit();
                init();
                return;
            }
        } else if (
            auto* mouse_event = event.getIf<sf::Event::MouseButtonPressed>()
        ) {
            if (mouse_event->button == sf::Mouse::Button::Left) {
                auto coords =
                    window_->mapPixelToCoords(sf::Mouse::getPosition(*window_));
                spawn_colony(coords);
            }
        }
    }

    void deinit() {
        world_.clear();
    }

  private:
    void spawn_food() {
        std::uniform_real_distribution<float> x_dist{0, kModeSize.x};
        std::uniform_real_distribution<float> y_dist{0, kModeSize.y};
        std::uniform_int_distribution<std::size_t> food_capacity_dist{
            kMinInitialFoodCapacity, kMaxInitialFoodCapacity
        };
        auto pos = sf::Vector2f{x_dist(gen_), y_dist(gen_)};
        world_.create<TPosition, TFood>(
            TPosition{.pos = pos}, TFood{.capacity = food_capacity_dist(gen_)}
        );
        world_.commit();
    }

    void spawn_colony() {
        std::uniform_real_distribution<float> x_dist{10, kModeSize.x - 10};
        std::uniform_real_distribution<float> y_dist{10, kModeSize.y - 10};
        auto pos = sf::Vector2f{x_dist(gen_), y_dist(gen_)};
        spawn_colony(pos);
    }

    void spawn_colony(const sf::Vector2f& pos) {
        std::uniform_int_distribution<std::size_t> ant_capacity_dist{
            kMinInitialAntColonyCapacity,
            kMaxAntColonyCapacity,
        };

        world_.create<TPosition, TColony>(
            TPosition{.pos = pos},
            TColony{.ant_capacity = ant_capacity_dist(gen_)}
        );
        world_.commit();
    }

    void init_feromone_map() {
        for (auto& row : feromones_map_) {
            for (auto& feromone : row) {
                feromone.searching = kMinSearchingFeromone;
                feromone.carrying  = kMinCarryingFeromone;
            }
        }
    }

    void move(float dt) {
        world_.select<TPosition, TVelocity>().run([&](auto entity) {
            auto& pos       = entity.template get<TPosition>().pos;
            const auto& vel = entity.template get<TVelocity>();
            pos += vel.dir * vel.vel * dt;
        });
    }

    void update_ant_logic(float dt) {
        auto& ant_subworld = world_.select<TPosition, TVelocity, TAnt>();
        ant_subworld.run([&](auto entity) {
            const auto& pos  = entity.template get<TPosition>();
            auto& vel        = entity.template get<TVelocity>();
            const auto state = entity.template get<TAnt>().state;
            vel.dir          = calc_ant_direction(
                pos.pos, vel.dir, entity.template get<TAnt>().state, dt
            );

            int x = static_cast<int>(pos.pos.x);
            int y = static_cast<int>(pos.pos.y);
            if (x >= 0 && x < feromones_map_.size() && y >= 0 &&
                y < feromones_map_[x].size()) {
                auto& val = state == EAntState::SEARCHING_FOOD
                              ? feromones_map_[x][y].carrying
                              : feromones_map_[x][y].searching;

                val += kAntFeromonesPerSecond * dt;
            }
        });

        ant_subworld.run([&](auto ant_entity) {
            const auto& ant_position = ant_entity.template get<TPosition>().pos;
            auto& ant                = ant_entity.template get<TAnt>();
            auto& ant_velocity       = ant_entity.template get<TVelocity>();
            if (ant.state == EAntState::SEARCHING_FOOD) {
                world_.select<TPosition, TFood>().run([&](auto food_entity) {
                    const auto& food_position =
                        food_entity.template get<TPosition>().pos;
                    auto& food = food_entity.template get<TFood>();
                    if ((ant_position - food_position).length() <
                        kFoodPickDistance) {
                        food.capacity -=
                            std::min(food.capacity, kPickFoodQuantity);
                        ant.state        = EAntState::CARRYING_FOOD_TO_COLONY;
                        ant_velocity.vel = kAntCarryingVelocity;
                    }
                    if (food.capacity == 0) {
                        food_entity.destroy();
                    }
                });
            } else if (ant.state == EAntState::CARRYING_FOOD_TO_COLONY) {
                world_.select<TPosition, TColony>().run(
                    [&](auto colony_entity) {
                        const auto& colony_position =
                            colony_entity.template get<TPosition>().pos;
                        auto& colony = colony_entity.template get<TColony>();

                        if ((ant_position - colony_position).length() <
                            kEnterColonyDistance) {
                            colony.ant_capacity += kPickFoodQuantity;
                            ant.state        = EAntState::SEARCHING_FOOD;
                            ant_velocity.vel = kAntSearchingVelocity;
                        }
                    }
                );
            }
        });

        world_.select<TPosition, TColony>().run([&](auto entity) {
            const auto& pos = entity.template get<TPosition>();
            auto& colony    = entity.template get<TColony>();
            std::uniform_real_distribution<float> ant_health_dist{
                kDefaultAntLifetime / 4, kDefaultAntLifetime * 2
            };
            while (colony.ant_capacity > 0) {
                world_.create<TPosition, TAnt, TVelocity>(
                    pos,
                    TAnt{
                        .state    = EAntState::SEARCHING_FOOD,
                        .lifetime = ant_health_dist(gen_)
                    },
                    TVelocity{
                        .dir = calc_ant_direction(
                            pos.pos, EAntState::SEARCHING_FOOD, dt
                        ),
                        .vel = kAntSearchingVelocity,
                    }
                );
                --colony.ant_capacity;
            }
        });

        world_.select<TAnt>().run([&](auto entity) {
            auto& ant = entity.template get<TAnt>();
            ant.lifetime -= dt;
            if (ant.lifetime < 0) {
                entity.destroy();
            }
        });
        world_.commit();
    }

    std::optional<sf::Vector2f> calc_by_vision_direction(
        const sf::Vector2f& pos, EAntState state
    ) {
        std::optional<sf::Vector2f> res;
        if (state == EAntState::SEARCHING_FOOD) {
            res = world_.select<TPosition, TFood>().run_batched(
                [&](auto& subworld) -> std::optional<sf::Vector2f> {
                    std::optional<sf::Vector2f> res;

                    for (auto entity : subworld) {
                        const auto& food_position =
                            entity.template get<TPosition>().pos;
                        if ((pos - food_position).length() < kVisionRadius) {
                            return (food_position - pos).normalized();
                        }
                    }
                    return res;
                }
            );
        } else if (state == EAntState::CARRYING_FOOD_TO_COLONY) {
            res = world_.select<TPosition, TColony>().run_batched(
                [&](auto& subworld) -> std::optional<sf::Vector2f> {
                    std::optional<sf::Vector2f> res;

                    float max = 999999.f;
                    for (auto entity : subworld) {
                        const auto& colony_position =
                            entity.template get<TPosition>().pos;
                        auto v = colony_position - pos;
                        auto l = v.length();
                        if (l < max) {
                            max = l;
                            res = v.normalized();
                        }
                    }
                    return res;
                }
            );
        }
        return res;
    }

    sf::Vector2f calc_ant_direction(
        const sf::Vector2f& pos, EAntState state, float dt
    ) {
        std::uniform_real_distribution<float> dist{-1.f, 1.f};

        return calc_ant_direction(
            pos, sf::Vector2f{dist(gen_), dist(gen_)}.normalized(), state, dt
        );
    }

    sf::Vector2f calc_ant_direction(
        const sf::Vector2f& pos, const sf::Vector2f& curr_dir, EAntState state,
        float dt
    ) {
        auto dir = calc_by_vision_direction(pos, state);
        if (dir) {
            return *dir;
        }

        std::array<sf::Vector2f, 4> directions{
            sf::Vector2f{1.f, 0.f},
            sf::Vector2f{0.f, 1.f},
            sf::Vector2f{-1.f, 0.f},
            sf::Vector2f{0.f, -1.f},
        };

        if (pos.x < 0) {
            directions[2] = {1.f, 0.f};
        } else if (pos.x > kModeSize.x) {
            directions[0] = {-1.f, 0.f};
        }
        if (pos.y < 0) {
            directions[3] = {0.f, 1.f};
        } else if (pos.y > kModeSize.y) {
            directions[1] = {0.f, -1.f};
        }

        std::array<float, 5> weights{0.f};
        for (std::size_t i = 1; i <= directions.size(); ++i) {
            const auto& dir           = directions[i - 1];
            const TFeromone& feromone = get_feromone(
                static_cast<int>(pos.x + dir.x), static_cast<int>(pos.y + dir.y)
            );
            float val = 1.f;
            if (state == EAntState::SEARCHING_FOOD) {
                val = feromone.searching;
            } else {
                val = feromone.carrying;
            }
            weights[i] = weights[i - 1] + val;
        }
        std::uniform_real_distribution<float> dist{weights[0], weights[4]};
        auto random    = dist(gen_);
        auto upper     = std::ranges::upper_bound(weights, random);
        auto lower     = std::prev(upper);
        const auto& v1 = directions[std::distance(weights.begin(), lower)];
        sf::Vector2f desired_dir = v1.rotatedBy(
            sf::radians(M_PI_2f * (1.f - (*upper - random) / (*upper - *lower)))
        );
        const auto& angle = curr_dir.angleTo(desired_dir);

        return curr_dir.rotatedBy(angle * dt);
    }

    void draw(float dt) {
        world_.select<TPosition>().run([&](auto entity) {
            if constexpr (entity.template has<TAnt>()) {
                sf::Vertex vert(
                    entity.template get<TPosition>().pos, sf::Color::Cyan
                );
                window_->draw(&vert, 1, sf::PrimitiveType::Points);
            } else if constexpr (entity.template has<TFood>()) {
                float radius =
                    0.1f +
                    static_cast<float>(entity.template get<TFood>().capacity) /
                        static_cast<float>(kMaxInitialFoodCapacity) * 4.f;
                sf::CircleShape food(radius);
                food.setOrigin({radius / 2, radius / 2});
                food.setPosition(entity.template get<TPosition>().pos);
                food.setFillColor(sf::Color::Magenta);
                window_->draw(food);
            } else if constexpr (entity.template has<TColony>()) {
                float radius = 2.f;
                sf::CircleShape colony(radius);
                colony.setOrigin({radius / 2, radius / 2});
                colony.setPosition(entity.template get<TPosition>().pos);
                colony.setFillColor(sf::Color::Blue);
                window_->draw(colony);
            }
        });
    }

    TFeromone get_feromone(int x, int y) {
        if (x < 0 || x >= kModeSize.x || y < 0 || y >= kModeSize.y) {
            return TFeromone{
                .searching = kMinSearchingFeromone,
                .carrying  = kMinCarryingFeromone,
            };
        }
        return feromones_map_[x][y];
    }

  private:
    std::array<std::array<TFeromone, kModeSize.y>, kModeSize.y> feromones_map_;

    sf::RenderWindow* window_;
    TAntWorld world_;

    std::mt19937 gen_;
    float elapsed_time_;
};

int main() {
    sf::VideoMode video_mode{kModeSize};
    sf::RenderWindow window(video_mode, "Ant");
    sf::Clock clock;

    TGame game(&window);
    game.init();

    if (!SFont.openFromFile("assets/font.ttf")) {
        std::cerr << "falied to load font" << std::endl;
    }

    clock.restart();
    while (window.isOpen()) {
        while (const auto& event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            game.handle_event(*event);
        }

        float dt = clock.restart().asSeconds();
        window.clear();
        game.update(dt);
        window.display();
    }

    game.deinit();

    return 0;
}
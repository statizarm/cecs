#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <random>
#include <ranges>

#include "world.hpp"

using namespace std::chrono_literals;

static constexpr sf::Vector2u kModeSize = {200, 200};
static constexpr std::array kColors     = {
    sf::Color{0xFF, 0x00, 0x7F, 0xff},
    sf::Color{0x3a, 0x0c, 0xa3, 0xff},
    sf::Color{0x04, 0x26, 0x30, 0xff},
    sf::Color{0x43, 0x61, 0xee, 0xff},
    sf::Color{0x4c, 0xc9, 0xF0, 0xff},

    sf::Color{0x04, 0x14, 0x21, 0xff},
    sf::Color{0x04, 0x26, 0x30, 0xff},
    sf::Color{0x4c, 0x72, 0x73, 0xff},
    sf::Color{0x86, 0xb9, 0xb0, 0xff},
    sf::Color{0xd0, 0xd6, 0xd6, 0xff},
};

static constexpr const sf::Color& kClearColor  = kColors[2];
static constexpr const std::array kBugsColors  = {kColors[0], kColors[4]};
static constexpr const std::array kSpotsColors = {kColors[1], kColors[3]};

static constexpr std::array kBugTypes = {std::size_t{0}, std::size_t{1}};

static constexpr std::size_t kMaxBugsCount       = 1 << 11;
static constexpr std::size_t kDefaultBugVelocity = 20.f;
static constexpr std::size_t kHearRadius         = 7.f;
static constexpr float kDefaultSpotShapeRadius   = 5.f;

struct TBug {
    std::size_t bug_type;
};
struct TSpot {
    std::size_t spot_type;
};
struct TPosition {
    sf::Vector2f pos;
};
struct TVelocity {
    sf::Vector2f dir;
    float vel;
};
struct TNotification {
    sf::Vector2f dst;
    float dist;
};
struct TNotifiedTag {};

// clang-format off
using TBugWorld = NCecs::TWorld<
    NCecs::TTypeList<
        NCecs::TTypeList<TBug, TPosition, TVelocity>,
        NCecs::TTypeList<TSpot, TPosition>,
        NCecs::TTypeList<TBug, TPosition, TVelocity, TNotification>,
        NCecs::TTypeList<TBug, TPosition, TVelocity, TNotification, TNotifiedTag>,
        NCecs::TTypeList<TBug, TPosition, TVelocity, TNotifiedTag>
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
        gen_.seed(rnd());

        spawn_spots();
        spawn_bugs();
    }

    void update(float dt) {
        update_bug_direction(dt);
        move(dt);
        notify_bug(dt);
        draw(dt);
    }

    void deinit() {
        world_.clear();
    }

    void handle_event(const sf::Event& event) {
        if (auto key_event = event.getIf<sf::Event::KeyPressed>()) {
            if (key_event->scancode == sf::Keyboard::Scancode::Space) {
                deinit();
                init();
            }
        }
    }

  private:
    void move(float dt) {
        world_.select<TPosition, TVelocity>().run([dt](auto entity) {
            auto& pos       = entity.template get<TPosition>().pos;
            const auto& vel = entity.template get<TVelocity>();
            pos             = pos + vel.dir * vel.vel * dt;
        });
    }

    void notify_bug(float dt) {
        world_.select<TSpot, TPosition>().run([&](auto spot_entity) {
            const auto& spot          = spot_entity.template get<TSpot>();
            const auto& spot_position = spot_entity.template get<TPosition>();
            world_.select<TBug, TPosition, TVelocity>().run(
                [&](auto bug_entity) {
                    const auto& bug_position =
                        bug_entity.template get<TPosition>();
                    auto& bug           = bug_entity.template get<TBug>();
                    const auto& bug_vel = bug_entity.template get<TVelocity>();

                    if (bug.bug_type != spot.spot_type) {
                        return;
                    }

                    auto dist = (bug_position.pos - spot_position.pos).length();
                    if (dist < bug_vel.vel * dt) {
                        bug.bug_type = (bug.bug_type + 1) % kBugTypes.size();
                        bug_entity.template del<TNotification>()
                            .template del<TNotifiedTag>();
                        return;
                    }

                    if constexpr (
                        !bug_entity.template has<TNotifiedTag>() &&
                        !bug_entity.template has<TNotification>()
                    ) {
                        if (dist < kHearRadius) {
                            bug_entity.template add<TNotifiedTag>()
                                .template add<TNotification>(
                                    spot_position.pos, dist
                                );
                        }
                    }
                }
            );
        });
        world_.commit();

        auto& subworld =
            world_.select<TBug, TPosition, TNotification, TNotifiedTag>();
        while (subworld.size()) {
            subworld.run([&](auto bug_entity) {
                const auto& bug_pos  = bug_entity.template get<TPosition>().pos;
                const auto& bug_type = bug_entity.template get<TBug>().bug_type;
                const auto& bug_notification =
                    bug_entity.template get<TNotification>();
                world_.select<TBug, TPosition>().run([&](auto entity) {
                    if constexpr (!entity.template has<TNotifiedTag>()) {
                        const auto& bug = entity.template get<TBug>();
                        if (bug.bug_type != bug_type) {
                            return;
                        }
                        const auto& pos = entity.template get<TPosition>().pos;
                        auto dist       = (pos - bug_pos).length();
                        if (dist > kHearRadius) {
                            return;
                        }

                        float curr_dist = bug_notification.dist + dist + 1.f;
                        if constexpr (entity.template has<TNotification>()) {
                            curr_dist =
                                entity.template get<TNotification>().dist;
                        }
                        dist += bug_notification.dist;
                        if (dist < curr_dist) {
                            std::array verts = {
                                sf::Vertex(pos, kBugsColors[bug.bug_type]),
                                sf::Vertex(bug_pos, kBugsColors[bug.bug_type])
                            };
                            window_->draw(
                                verts.data(),
                                verts.size(),
                                sf::PrimitiveType::Lines
                            );

                            entity.template add<TNotification>()
                                .template add<TNotifiedTag>()
                                .template get<TNotification>() =
                                TNotification{.dst = bug_pos, .dist = dist};
                        }
                    }
                });
                bug_entity.template del<TNotifiedTag>();
            });
            subworld.commit();
        }
    }

    void update_bug_direction(float dt) {
        world_.select<TBug, TPosition, TVelocity>().run([&](auto entity) {
            const auto& pos = entity.template get<TPosition>().pos;
            auto& vel       = entity.template get<TVelocity>();

            if (pos.x > kModeSize.x) {
                vel.dir.x = -std::abs(vel.dir.x);
            } else if (pos.x < 0) {
                vel.dir.x = std::abs(vel.dir.x);
            }

            if (pos.y > kModeSize.y) {
                vel.dir.y = -std::abs(vel.dir.y);
            } else if (pos.y < 0) {
                vel.dir.y = std::abs(vel.dir.y);
            }
        });

        world_.select<TBug, TPosition, TVelocity>().run([&](auto entity) {
            const auto& position = entity.template get<TPosition>();
            auto& vel            = entity.template get<TVelocity>();

            if constexpr (!entity.template has<TNotification>()) {
                std::uniform_real_distribution<> dist{-180.f, 180.f};
                vel.dir = vel.dir.rotatedBy(sf::degrees(dist(gen_)) * dt)
                              .normalized();
            } else if constexpr (entity.template has<TNotification>()) {
                const auto& notification = entity.template get<TNotification>();
                if ((notification.dst - position.pos).length() < vel.vel * dt) {
                    entity.template del<TNotification>();
                } else {
                    vel.dir = (notification.dst - position.pos).normalized();
                }
            }
        });
        world_.commit();
    }

    void draw(float dt) {
        world_.select<TPosition>().run([&](auto entity) {
            const auto& pos = entity.template get<TPosition>();
            if constexpr (entity.template has<TBug>()) {
                const auto& bug = entity.template get<TBug>();
                sf::Vertex vert{pos.pos, kBugsColors[bug.bug_type]};
                window_->draw(&vert, 1, sf::PrimitiveType::Points);
            } else if constexpr (entity.template has<TSpot>()) {
                const auto& spot = entity.template get<TSpot>();
                sf::CircleShape shape(kDefaultSpotShapeRadius);
                shape.setOrigin(
                    {kDefaultSpotShapeRadius, kDefaultSpotShapeRadius}
                );
                shape.setPosition(pos.pos);
                shape.setFillColor(kSpotsColors[spot.spot_type]);
                window_->draw(shape);
            }
        });
    }

    void spawn_spots() {
        std::uniform_real_distribution<float> x_dist(10.f, kModeSize.x - 10);
        std::uniform_real_distribution<float> y_dist(10.f, kModeSize.y - 10);
        for (auto type : kBugTypes) {
            sf::Vector2f pos{x_dist(gen_), y_dist(gen_)};
            world_.create<TSpot, TPosition>(
                TSpot{.spot_type = type}, TPosition{.pos = pos}
            );
        }
        world_.commit();
    }

    void spawn_bugs() {
        std::uniform_real_distribution<float> x_dist(10.f, kModeSize.x - 10);
        std::uniform_real_distribution<float> y_dist(10.f, kModeSize.y - 10);
        std::uniform_real_distribution<float> dir_dist{-1.f, 1.f};
        std::uniform_real_distribution<float> vel_dist{
            kDefaultBugVelocity / 2, kDefaultBugVelocity * 4
        };
        for (auto index : std::views::iota(std::size_t{0}, kMaxBugsCount)) {
            world_.create<TBug, TPosition, TVelocity>(
                TBug{.bug_type = index % kBugTypes.size()},
                TPosition{.pos = {x_dist(gen_), y_dist(gen_)}},
                TVelocity{
                    .dir = {dir_dist(gen_), dir_dist(gen_)},
                    .vel = vel_dist(gen_)
                }
            );
        }
        world_.commit();
    }

  private:
    sf::RenderWindow* window_;
    TBugWorld world_;
    std::mt19937 gen_;
};

int main() {
    sf::RenderWindow window{sf::VideoMode{kModeSize}, "screaming bugs"};
    sf::Clock clock;
    TGame game(&window);
    game.init();

    clock.restart();
    while (window.isOpen()) {
        while (const auto& event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else {
                game.handle_event(*event);
            }
        }

        window.clear(kClearColor);
        game.update(clock.restart().asSeconds());
        window.display();
    }
    game.deinit();
}
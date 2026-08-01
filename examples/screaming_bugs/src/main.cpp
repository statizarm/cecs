#include <EASTL/fixed_vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/hash_set.h>
#include <EASTL/heap.h>
#include <EASTL/priority_queue.h>
#include <EASTL/queue.h>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <future>
#include <mutex>
#include <random>
#include <ranges>
#include <thread>

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

static constexpr std::size_t kMaxBugsCount       = 1 << 14;
static constexpr std::size_t kDefaultBugVelocity = 10.f;
static constexpr float kHearRadius               = 2.5f;
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
    NCecs::TEntityID from;
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

namespace eastl {
template <>
struct hash<sf::Vector2i> {
    std::size_t operator()(const sf::Vector2i& v) const {
        return static_cast<std::size_t>(v.x) << 32 |
               static_cast<std::size_t>(v.y);
    }
};
}  // namespace eastl

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
        init_cache();
    }

    void update(float dt) {
        std::cout << 1 / dt << std::endl;
        static constexpr float kDt = 0.005;
        notify_bug(dt);

        while (dt > kDt) {
            update_bug_direction(kDt);
            move(kDt);

            dt -= kDt;
        }
        update_bug_direction(dt);
        move(dt);

        draw();
    }

    void deinit() {
        world_.clear();
        grid_cache_.clear();
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
        world_.select<TPosition, TVelocity>().run([this, dt](auto entity) {
            auto& pos       = entity.template get<TPosition>().pos;
            const auto& vel = entity.template get<TVelocity>();
            auto prev_pos   = pos;
            pos             = pos + vel.dir * vel.vel * dt;
            update_cache(prev_pos, pos, entity.id());
        });
    }

    sf::Vector2i get_cell(const sf::Vector2f& pos) {
        return {static_cast<int>(pos.x), static_cast<int>(pos.y)};
    }

    void init_cache() {
        world_.select<TPosition>().run([&](auto entity) {
            const auto& pos = entity.template get<TPosition>().pos;
            grid_cache_[get_cell(pos)].insert(entity.id());
        });
    }

    void update_cache(
        const sf::Vector2f& prev_pos, const sf::Vector2f& curr_pos,
        NCecs::TEntityID id
    ) {
        sf::Vector2i prev = get_cell(prev_pos);
        sf::Vector2i curr = get_cell(curr_pos);
        if (prev == curr) {
            return;
        }

        grid_cache_[prev].erase(id);
        grid_cache_[curr].insert(id);
    }

    template <typename TF>
    void for_nearest(sf::Vector2f pos, float radius, TF func) {
        int32_t up_x = static_cast<int32_t>(pos.x + radius);
        int32_t up_y = static_cast<int32_t>(pos.y + radius);
        for (int32_t x = static_cast<int32_t>(pos.x - radius); x <= up_x; ++x) {
            for (int32_t y = static_cast<int32_t>(pos.y - radius); y <= up_y;
                 ++y) {
                if (auto it = grid_cache_.find({x, y});
                    it != grid_cache_.end()) {
                    for (auto e_id : it->second) {
                        world_.get(e_id).call(func);
                    }
                }
            }
        }
    }

    void notify_bug(float dt) {
        world_.select<TNotifiedTag>().run([](auto entity) {
            entity.template del<TNotifiedTag>().template del<TNotification>();
        });
        world_.commit();

        using TQueueChunk = std::pair<float, NCecs::TEntityID>;
        eastl::queue<TQueueChunk, eastl::deque<TQueueChunk>> queue;

        world_.select<TSpot, TPosition>().run([&](auto spot_entity) {
            const auto& spot          = spot_entity.template get<TSpot>();
            const auto& spot_position = spot_entity.template get<TPosition>();
            for_nearest(spot_position.pos, kHearRadius, [&](auto bug_entity) {
                if constexpr (
                    bug_entity.template has<TPosition>() &&
                    bug_entity.template has<TBug>() &&
                    bug_entity.template has<TVelocity>()
                ) {
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
                            auto res = bug_entity.template add<TNotification>(
                                spot_entity.id(), dist
                            );
                            queue.emplace(std::make_pair(dist, res.id()));
                        }
                    }
                }
            });
        });
        world_.commit();

        while (!queue.empty()) {
            auto [curr_dist, curr_bug_id] = queue.front();
            queue.pop();

            world_.get(curr_bug_id).call([&](auto curr_bug) {
                if constexpr (
                    curr_bug.template has<TPosition>() &&
                    curr_bug.template has<TBug>() &&
                    !curr_bug.template has<TNotifiedTag>()
                ) {
                    const auto& curr_pos =
                        curr_bug.template get<TPosition>().pos;
                    const auto bug_type =
                        curr_bug.template get<TBug>().bug_type;
                    for_nearest(curr_pos, kHearRadius, [&](auto entity) {
                        if constexpr (
                            !entity.template has<TNotifiedTag>() &&
                            entity.template has<TBug>() &&
                            entity.template has<TPosition>()
                        ) {
                            const auto& bug = entity.template get<TBug>();
                            if (bug.bug_type != bug_type) {
                                return;
                            }
                            const auto& pos =
                                entity.template get<TPosition>().pos;
                            auto dist        = (pos - curr_pos).length();
                            float total_dist = curr_dist + dist;
                            if constexpr (
                                entity.template has<TNotification>()
                            ) {
                                if (entity.template get<TNotification>().dist <
                                    total_dist) {
                                    return;
                                }
                            }
                            if (dist <= kHearRadius) {
                                auto res = entity.template add<TNotification>(
                                    curr_bug_id, total_dist
                                );
                                queue.emplace(total_dist, res.id());
                            }
                        }
                    });
                    curr_bug.template add<TNotifiedTag>();
                }
            });
        }
        world_.commit();
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
                const auto& dst =
                    world_.get(notification.from).template get<TPosition>().pos;
                if ((dst - position.pos).length() < vel.vel * dt) {
                    entity.template del<TNotification>();
                } else {
                    vel.dir = (dst - position.pos).normalized();
                }
            }
        });
        world_.commit();
    }

    void draw() {
        world_.select<TPosition>().run([&](auto entity) {
            const auto& pos = entity.template get<TPosition>();
            if constexpr (
                entity.template has<TBug>() &&
                entity.template has<TNotifiedTag>() &&
                entity.template has<TNotification>()
            ) {
                const auto& bug          = entity.template get<TBug>();
                const auto& notification = entity.template get<TNotification>();
                const auto& dst =
                    world_.get(notification.from).template get<TPosition>().pos;
                sf::Vertex verts[] = {
                    sf::Vertex{pos.pos, kBugsColors[bug.bug_type]},
                    sf::Vertex{dst, kBugsColors[bug.bug_type]},
                };
                window_->draw(verts, 2, sf::PrimitiveType::Lines);
            }
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
    eastl::hash_map<sf::Vector2i, eastl::hash_set<NCecs::TEntityID>>
        grid_cache_;
    sf::RenderWindow* window_;
    TBugWorld world_;
    std::mt19937 gen_;
};

int main() {
    sf::RenderWindow window{sf::VideoMode{kModeSize}, "screaming bugs"};
    sf::Clock clock;
    TGame game(&window);
    game.init();

    std::atomic<std::size_t> runable = 1;
    std::mutex mut;
    auto fut = std::async(std::launch::async, [&]() {
        while (runable) {
            {
                std::lock_guard lock{mut};
                game.deinit();
                game.init();
            }
            std::this_thread::sleep_for(10s);
        }
    });

    clock.restart();
    while (runable) {
        {
            std::lock_guard lock{mut};
            while (const auto& event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window.close();
                    runable = 0;
                } else {
                    game.handle_event(*event);
                }
            }

            window.clear(kClearColor);
            game.update(clock.restart().asSeconds());
        }
        window.display();
    }
    fut.wait();
    game.deinit();
}

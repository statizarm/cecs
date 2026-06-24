#include "world.hpp"

#include <gtest/gtest.h>

#include <type_traits>

#include "type_list.hpp"

TEST(TWorld, TTestConstructDestructWorld) {
    NCecs::TWorld<NCecs::TTypeList<
        NCecs::TTypeList<int, char, double>,
        NCecs::TTypeList<std::string, float>,
        NCecs::TTypeList<int, double>,
        NCecs::TTypeList<char>>>
        world;
}

TEST(TWorld, TTestCreate) {
    static constexpr std::size_t kSize = 1 << 20;

    NCecs::TWorld<NCecs::TTypeList<
        NCecs::TTypeList<int, char, short>,
        NCecs::TTypeList<std::string, int>,
        NCecs::TTypeList<int, std::size_t>,
        NCecs::TTypeList<char, int>>>
        world;

    std::array<std::function<void(std::size_t)>, 4> assertions = {
        [&world](std::size_t i) {
            const auto& entity = world.create<int, char, short>(
                static_cast<int>(i), 4 - (i % 4), i % 4
            );
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<char>(), 4 - (i % 4));
            EXPECT_EQ(entity.get<short>(), i % 4);
        },
        [&world](std::size_t i) {
            const auto& entity = world.create<std::string, int>(
                std::to_string(i), static_cast<int>(i)
            );
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<std::string>(), std::to_string(i));
        },
        [&world](std::size_t i) {
            const auto& entity =
                world.create<std::size_t, int>(i, static_cast<int>(i));
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<std::size_t>(), i);
        },
        [&world](std::size_t i) {
            const auto& entity =
                world.create<int, char>(static_cast<int>(i), 4 - (i % 4));
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<char>(), 4 - (i % 4));
        },
    };

    for (std::size_t i = 0; i < kSize; ++i) {
        assertions[i % assertions.size()](i);
    }
    world.commit();
}

TEST(TWorld, TTestSelect) {
    static constexpr std::size_t kSize = 1 << 20;

    NCecs::TWorld<NCecs::TTypeList<
        NCecs::TTypeList<int, char, short>,
        NCecs::TTypeList<std::string, int>,
        NCecs::TTypeList<int, std::size_t>,
        NCecs::TTypeList<char, int>>>
        world;

    std::array<std::function<void(std::size_t)>, 4> assertions = {
        [&world](std::size_t i) {
            const auto& entity = world.create<int, char, short>(
                static_cast<int>(i), 4 - (i % 4), i % 4
            );
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<char>(), 4 - (i % 4));
            EXPECT_EQ(entity.get<short>(), i % 4);
        },
        [&world](std::size_t i) {
            const auto& entity = world.create<std::string, int>(
                std::to_string(i), static_cast<int>(i)
            );
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<std::string>(), std::to_string(i));
        },
        [&world](std::size_t i) {
            const auto& entity =
                world.create<std::size_t, int>(i, static_cast<int>(i));
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<std::size_t>(), i);
        },
        [&world](std::size_t i) {
            const auto& entity =
                world.create<int, char>(static_cast<int>(i), 4 - (i % 4));
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<char>(), 4 - (i % 4));
        },
    };

    for (std::size_t i = 0; i < kSize; ++i) {
        assertions[i % assertions.size()](i);
    }
    world.commit();

    std::size_t string_count = 0;
    std::size_t total_calls  = 0;
    auto func = [&string_count, &total_calls](const auto& entity) mutable {
        ++total_calls;
        if (entity.template has<std::string>()) {
            ++string_count;
        }
    };
    world.select<int>(func);

    EXPECT_EQ(string_count, kSize / 4);
    EXPECT_EQ(total_calls, kSize);

    string_count = 0;
    total_calls  = 0;
    world.select<char>(func);
    EXPECT_EQ(string_count, 0);
    EXPECT_EQ(total_calls, kSize / 2);

    string_count = 0;
    total_calls  = 0;
    world.select<int>(func);
    world.select<int>(func);

    EXPECT_EQ(string_count, kSize / 2);
    EXPECT_EQ(total_calls, kSize * 2);
}

TEST(TWorld, TAddComponent) {
    NCecs::TWorld<
        NCecs::TTypeList<NCecs::TTypeList<int>, NCecs::TTypeList<int, char>>>
        world;

    auto entity = world.create<int>(42);
    EXPECT_EQ(entity.get<int>(), 42);

    auto new_entity = entity.add<char>('0');
    EXPECT_FALSE(entity.valid());

    EXPECT_EQ(new_entity.get<int>(), 42);
    EXPECT_EQ(new_entity.get<char>(), '0');

    auto not_new_entity = new_entity.add<char>('1');
    static_assert(
        std::is_same_v<decltype(new_entity), decltype(not_new_entity)>
    );
    EXPECT_EQ(not_new_entity.get<char>(), '0');

    EXPECT_TRUE(new_entity.valid());
    EXPECT_TRUE(not_new_entity.valid());
}

TEST(TWorld, TDelComponent) {
    NCecs::TWorld<
        NCecs::TTypeList<NCecs::TTypeList<int>, NCecs::TTypeList<int, char>>>
        world;

    auto entity = world.create<int, char>(42, '0');
    EXPECT_EQ(entity.get<int>(), 42);

    auto new_entity = entity.del<char>();
    EXPECT_FALSE(entity.valid());

    EXPECT_EQ(new_entity.get<int>(), 42);

    auto not_new_entity = new_entity.del<std::string>();
    static_assert(
        std::is_same_v<decltype(new_entity), decltype(not_new_entity)>
    );
    EXPECT_TRUE(new_entity.valid());
    EXPECT_TRUE(not_new_entity.valid());
}

TEST(TWorld, TTestMovedEntititesUnseenUntilCommit) {
    NCecs::TWorld<
        NCecs::TTypeList<NCecs::TTypeList<int>, NCecs::TTypeList<int, char>>>
        world;

    auto entity = world.create<int, char>(42, '0');
    EXPECT_EQ(entity.get<int>(), 42);
    EXPECT_EQ(entity.get<char>(), '0');

    auto new_entity = entity.del<char>();
    EXPECT_FALSE(entity.valid());

    bool res       = false;
    auto assertion = [&res](auto entity) {
        res = res || entity.template has<int>();
    };
    world.select<int>(assertion);
    world.commit();

    EXPECT_FALSE(res);

    res = false;
    world.select<int>(assertion);
    EXPECT_TRUE(res);
}

TEST(TWorld, TTestSelectWithAddDeleteComponents) {
    static constexpr std::size_t kSize = 1 << 10;

    NCecs::TWorld<NCecs::TTypeList<
        NCecs::TTypeList<int, char>,
        NCecs::TTypeList<std::string, int>,
        NCecs::TTypeList<std::string, int, char>>>
        world;

    std::array<std::function<void(std::size_t)>, 2> assertions = {
        [&world](std::size_t i) {
            const auto& entity =
                world.create<int, char>(static_cast<int>(i), 4 - (i % 4));
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<char>(), 4 - (i % 4));
        },
        [&world](std::size_t i) {
            const auto& entity = world.create<std::string, int>(
                std::to_string(i), static_cast<int>(i)
            );
            EXPECT_EQ(entity.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity.get<std::string>(), std::to_string(i));
        },
    };

    for (std::size_t i = 0; i < kSize; ++i) {
        assertions[i % assertions.size()](i);
    }
    world.commit();

    std::size_t total_calls = 0;
    std::size_t moves_count = 0;
    auto func = [&moves_count, &total_calls](auto entity) mutable {
        ++total_calls;
        if constexpr (entity.template has<char>()) {
            auto res =
                entity.template add<std::string>(1, entity.template get<char>())
                    .template del<char>();
            ++moves_count;
        }
    };
    world.select<int>(func);

    EXPECT_EQ(total_calls, kSize);
    EXPECT_EQ(moves_count, kSize / 2);

    std::size_t moves_history = moves_count;
    total_calls               = 0;
    moves_count               = 0;

    world.select<int>(func);

    EXPECT_EQ(total_calls, kSize - moves_history);
    EXPECT_EQ(moves_count, 0);
}

TEST(TWorld, TTestSelectWithAddDeleteComponentsBetweenDifferentWorlds) {
    static constexpr std::size_t kSize = 1 << 10;

    using TWorld1 = NCecs::TWorld<NCecs::TTypeList<
        NCecs::TTypeList<int, char>,
        NCecs::TTypeList<std::string, int>,
        NCecs::TTypeList<std::string, int, char>>>;
    using TWorld2 = NCecs::TWorld<NCecs::TTypeList<
        NCecs::TTypeList<std::string, int, char>,
        NCecs::TTypeList<std::string, int>,
        NCecs::TTypeList<int, char>>>;

    TWorld1 world1;
    TWorld2 world2;

    std::array<std::function<void(std::size_t)>, 2> assertions = {
        [&world1, &world2](std::size_t i) {
            const auto& entity1 =
                world1.create<int, char>(static_cast<int>(i), 4 - (i % 4));
            EXPECT_EQ(entity1.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity1.get<char>(), 4 - (i % 4));

            const auto& entity2 =
                world2.create<int, char>(static_cast<int>(i), 4 - (i % 4));
            EXPECT_EQ(entity2.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity2.get<char>(), 4 - (i % 4));
        },
        [&world1, &world2](std::size_t i) {
            const auto& entity1 = world1.create<std::string, int>(
                std::to_string(i), static_cast<int>(i)
            );
            EXPECT_EQ(entity1.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity1.get<std::string>(), std::to_string(i));

            const auto& entity2 = world2.create<std::string, int>(
                std::to_string(i), static_cast<int>(i)
            );
            EXPECT_EQ(entity2.get<int>(), static_cast<int>(i));
            EXPECT_EQ(entity2.get<std::string>(), std::to_string(i));
        },
    };

    for (std::size_t i = 0; i < kSize; ++i) {
        assertions[i % assertions.size()](i);
    }
    world1.commit();
    world2.commit();

    std::size_t total_calls = 0;
    std::size_t moves_count = 0;
    auto func = [&moves_count, &total_calls](auto entity) mutable {
        ++total_calls;
        if constexpr (entity.template has<char>()) {
            auto res =
                entity.template add<std::string>(1, entity.template get<char>())
                    .template del<char>();
            ++moves_count;
        }
    };
    world1.select<int>(func);

    auto world1_total_calls = total_calls;
    auto world1_moves_count = moves_count;

    total_calls = 0;
    moves_count = 0;
    world2.select<int>(func);

    auto world2_total_calls = total_calls;
    auto world2_moves_count = moves_count;

    EXPECT_EQ(world1_total_calls, world2_total_calls);
    EXPECT_EQ(world1_moves_count, world2_moves_count);
}
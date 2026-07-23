#include "world.hpp"

#include <gtest/gtest.h>

#include <memory>
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

TEST(TWorld, TTestSubWorld) {
    using TW = NCecs::TWorld<NCecs::TTypeList<
        NCecs::TTypeList<int, char, double>,
        NCecs::TTypeList<std::string, float>,
        NCecs::TTypeList<int, double>,
        NCecs::TTypeList<char>>>;

    TW world;

    auto& subworld1      = world.select<int>();
    using TSubworldType1 = std::decay_t<decltype(subworld1)>;
    static_assert(std::same_as<
                  TSubworldType1::TKnownArchetypes,
                  NCecs::TTypeList<
                      NCecs::TTypeList<int, char, double>,
                      NCecs::TTypeList<int, double>>>);
    EXPECT_EQ(
        static_cast<void*>(std::addressof(world)),
        static_cast<void*>(std::addressof(subworld1))
    );

    auto& subworld2      = subworld1.select<char>();
    using TSubworldType2 = std::decay_t<decltype(subworld2)>;
    static_assert(std::same_as<
                  TSubworldType2::TKnownArchetypes,
                  NCecs::TTypeList<NCecs::TTypeList<int, char, double>>>);
    EXPECT_EQ(
        static_cast<void*>(std::addressof(world)),
        static_cast<void*>(std::addressof(subworld2))
    );

    auto& whole_world1 = subworld1.get_whole_world();
    auto& whole_world2 = subworld2.get_whole_world();
    using TWholeWorld1 = std::decay_t<decltype(whole_world1)>;
    using TWholeWorld2 = std::decay_t<decltype(whole_world2)>;
    static_assert(std::same_as<TWholeWorld1, TWholeWorld2>);
    static_assert(std::same_as<TWholeWorld1, TW>);

    EXPECT_EQ(
        static_cast<void*>(std::addressof(whole_world1)),
        static_cast<void*>(std::addressof(whole_world2))
    );
    EXPECT_EQ(
        static_cast<void*>(std::addressof(whole_world1)),
        static_cast<void*>(std::addressof(world))
    );
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
    world.select<int>().run(func);

    EXPECT_EQ(string_count, kSize / 4);
    EXPECT_EQ(total_calls, kSize);

    string_count = 0;
    total_calls  = 0;
    world.select<char>().run(func);
    EXPECT_EQ(string_count, 0);
    EXPECT_EQ(total_calls, kSize / 2);

    string_count = 0;
    total_calls  = 0;
    world.select<int>().run(func);
    world.select<int>().run(func);

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
    world.select<int>().run(assertion);
    world.commit();

    EXPECT_FALSE(res);

    res = false;
    world.select<int>().run(assertion);
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
    world.select<int>().run(func);

    EXPECT_EQ(total_calls, kSize);
    EXPECT_EQ(moves_count, kSize / 2);

    std::size_t moves_history = moves_count;
    total_calls               = 0;
    moves_count               = 0;

    world.select<int>().run(func);

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
    world1.select<int>().run(func);

    auto world1_total_calls = total_calls;
    auto world1_moves_count = moves_count;

    total_calls = 0;
    moves_count = 0;
    world2.select<int>().run(func);

    auto world2_total_calls = total_calls;
    auto world2_moves_count = moves_count;

    EXPECT_EQ(world1_total_calls, world2_total_calls);
    EXPECT_EQ(world1_moves_count, world2_moves_count);
}

TEST(TWorld, TTestVariantEntityGetHas) {
    NCecs::TWorld<
        NCecs::TTypeList<NCecs::TTypeList<int>, NCecs::TTypeList<int, char>>>
        world;

    auto entity1 = world.create<int, char>(1, '1');
    EXPECT_EQ(entity1.get<int>(), 1);
    EXPECT_EQ(entity1.get<char>(), '1');

    auto entity2 = world.create<int>(2);
    EXPECT_EQ(entity2.get<int>(), 2);

    using TVar = NCecs::TVariantEntity<decltype(entity1), decltype(entity2)>;
    TVar variant_entity{entity1};

    EXPECT_EQ(variant_entity.get<int>(), 1);
    EXPECT_TRUE(variant_entity.has<char>());
    EXPECT_EQ(variant_entity.get<char>(), '1');

    variant_entity = TVar(entity2);

    EXPECT_EQ(variant_entity.get<int>(), 2);
    EXPECT_FALSE(variant_entity.has<char>());

    const TVar const_variant_entity = entity2;

    static_assert(std::same_as<
                  decltype(const_variant_entity.template get<int>()),
                  const int&>);

    EXPECT_EQ(variant_entity.get<int>(), 2);
    EXPECT_FALSE(variant_entity.has<char>());
}

TEST(TWorld, TTestVariantEntityAddDel) {
    NCecs::TWorld<
        NCecs::TTypeList<NCecs::TTypeList<int>, NCecs::TTypeList<int, char>>>
        world;

    auto entity1 = world.create<int, char>(1, '1');
    auto entity2 = world.create<int>(2);

    using TVar = NCecs::TVariantEntity<decltype(entity1), decltype(entity2)>;
    TVar variant_entity{entity1};

    EXPECT_EQ(variant_entity.get<int>(), 1);
    EXPECT_TRUE(variant_entity.has<char>());
    EXPECT_EQ(variant_entity.get<char>(), '1');

    auto new_var = variant_entity.template del<char>();
    world.commit();

    EXPECT_EQ(new_var.get<int>(), 1);
    EXPECT_FALSE(new_var.has<char>());

    auto new_new_var = new_var.add<char>('9');
    world.commit();
    EXPECT_EQ(new_new_var.get<int>(), 1);
    EXPECT_TRUE(new_new_var.has<char>());
    EXPECT_EQ(new_new_var.get<char>(), '9');
}

TEST(TWorld, TTestBatchSelect) {
    static constexpr std::size_t kSize = 1 << 20;
    NCecs::TWorld<NCecs::TTypeList<
        NCecs::TTypeList<std::size_t>,
        NCecs::TTypeList<std::size_t, std::string>>>
        world;

    for (std::size_t i = 0; i < kSize; ++i) {
        if (i & 1) {
            world.create<std::size_t, std::string>(i, "hello");
        } else {
            world.create<std::size_t>(i);
        }
    }
    world.commit();

    auto [counter, sum] = world.select<std::size_t>().run_batched(
        [&](auto& subworld) -> std::pair<std::size_t, std::size_t> {
            std::size_t sum     = 0;
            std::size_t counter = 0;
            for (auto entity : subworld) {
                ++counter;
                sum += entity.template get<std::size_t>();
                if (entity.template has<std::string>()) {
                    EXPECT_EQ(entity.template get<std::string>(), "hello");
                }
            }
            return std::make_pair(counter, sum);
        }
    );

    EXPECT_EQ(counter, kSize);
    EXPECT_EQ(sum, (kSize - 1) * kSize / 2);
}

TEST(TWorld, TTestEmptyIteration) {
    using TWorld = NCecs::TWorld<
        NCecs::TTypeList<NCecs::TTypeList<int, char>, NCecs::TTypeList<char>>>;
    TWorld world;

    std::size_t size = 0;
    for (auto e : world) {
        ++size;
    }

    EXPECT_EQ(size, 0);
}

TEST(TWorld, TTestClear) {
    using TWorld = NCecs::TWorld<
        NCecs::TTypeList<NCecs::TTypeList<int, char>, NCecs::TTypeList<char>>>;
    TWorld world;

    world.create<int, char>(10, '0');
    world.create<char>('0');
    world.commit();

    std::size_t res = world.run_batched([](auto& subworld) -> std::size_t {
        return std::distance(subworld.begin(), subworld.end());
    });

    EXPECT_EQ(res, 2);

    world.clear();
    res = world.run_batched([](auto& subworld) -> std::size_t {
        return std::distance(subworld.begin(), subworld.end());
    });
    EXPECT_EQ(res, 0);
}

TEST(TWorld, TTestCommitInsideIteration) {
    static constexpr std::size_t kElementsCount = 1 << 10;
    using TWorld =
        NCecs::TWorld<NCecs::TTypeList<NCecs::TTypeList<std::size_t>>>;
    TWorld world;

    for (std::size_t i = 0; i < kElementsCount; ++i) {
        world.create<std::size_t>(i);
    }
    EXPECT_TRUE(world.commit());

    world.select<std::size_t>().run([&](auto entity) {
        auto c = entity.template get<std::size_t>();
        if (c > kElementsCount - 100 && c < kElementsCount - 10) {
            entity.destroy();
        } else if (c > kElementsCount - 10) {
            EXPECT_FALSE(world.commit());
        }
    });
    EXPECT_TRUE(world.commit());
}

TEST(TWorld, TTestGetEntityByID) {
    using TWorld =
        NCecs::TWorld<NCecs::TTypeList<NCecs::TTypeList<std::size_t>>>;
    TWorld world;

    auto e     = world.create<std::size_t>(10);
    auto var_e = world.get(e.id());

    EXPECT_EQ(e.template get<std::size_t>(), var_e.template get<std::size_t>());

    e.template get<std::size_t>() = 100500;
    EXPECT_EQ(var_e.template get<std::size_t>(), 100500);
}
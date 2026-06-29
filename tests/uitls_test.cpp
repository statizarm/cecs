#include <gtest/gtest.h>

#include <stdexcept>

#include "utils.hpp"

TEST(TUtils, TTestOneOfConcept) {
    static_assert(NCecs::COneOf<int, char, int, double>);
    static_assert(!NCecs::COneOf<std::string, char, int, double>);

    static_assert(!NCecs::COneOf<int>);
}

template <typename... T>
struct TPartialTest {};

TEST(TUtils, TTestPartialTemplate) {
    static_assert(
        std::same_as<
            NCecs::TPartialTemplate<TPartialTest, int, char>::template type<
                double>,
            TPartialTest<int, char, double>>
    );
}

TEST(TUtils, TTestOverloadedVisit) {
    using TValue = std::variant<int, char, double, std::string>;
    TValue var   = 0.1;

    auto overloaded = NCecs::TOverloaded{
        [](int&) -> int { return 0; },
        [](char&) -> int { return 1; },
        [](double&) -> int { return 2; },
        [](auto) -> int { return -1; },
    };
    EXPECT_EQ(std::visit(overloaded, var), 2);
    var = 1;
    EXPECT_EQ(std::visit(overloaded, var), 0);
    var = '0';
    EXPECT_EQ(std::visit(overloaded, var), 1);
    var = std::string{"hi"};
    EXPECT_EQ(std::visit(overloaded, var), -1);
}

template <typename T>
struct TGetter {
    using TType = T;
    template <typename TT>
    bool get()
        requires(std::same_as<T, TT>)
    {
        return true;
    }
};

template <typename TT>
auto get_overloaded() {
    return NCecs::TOverloaded{[]<typename TTT>(TTT g) -> bool {
        if constexpr (std::same_as<typename TTT::TType, TT>) {
            return g.template get<TT>();
        } else {
            throw std::runtime_error("");
        };
    }};
}

TEST(TUtils, TTestOverloadedVisitForArgs) {
    using TVal   = std::variant<TGetter<int>, TGetter<char>>;
    TVal variant = TGetter<int>();

    EXPECT_TRUE(std::visit(get_overloaded<int>(), variant));
    EXPECT_THROW(
        std::visit(get_overloaded<char>(), variant), std::runtime_error
    );
}
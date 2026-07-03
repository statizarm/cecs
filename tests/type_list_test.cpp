#include "type_list.hpp"

#include <gtest/gtest.h>

#include <type_traits>

#include "utils.hpp"

TEST(TTypeListTest, THasType) {
    using l = NCecs::TTypeList<int, bool>;

    static_assert(l::has<int>::value);
    static_assert(l::has<bool>::value);
    static_assert(!l::has<float>::value);
}

TEST(TTypeListTest, TAddType) {
    using l = NCecs::TTypeList<int, bool>;
    static_assert(std::same_as<l::add<int>::type, l>);
    static_assert(
        std::same_as<l::add<double>::type, NCecs::TTypeList<double, int, bool>>
    );
    static_assert(
        !std::same_as<l::add<double>::type, NCecs::TTypeList<int, double, bool>>
    );
}

TEST(TTypeListTest, TEqOperator) {
    NCecs::TTypeList<int, float> l1;
    NCecs::TTypeList<int, bool> l2;

    static_assert(l1 == l1);
    static_assert(l1 != l2);
}

TEST(TTypeListTest, TIsSub) {
    static_assert(NCecs::TTypeList<int, float>::is_sub<
                  NCecs::TTypeList<int, double, float>>());
    static_assert(
        NCecs::TTypeList<>::is_sub<NCecs::TTypeList<int, double, float>>()
    );
    static_assert(
        !NCecs::TTypeList<bool>::is_sub<NCecs::TTypeList<int, double, float>>()
    );

    using l1 = NCecs::TTypeList<int, float>;
    using l2 = NCecs::TTypeList<int, double>;

    using intersection = l1::intersect<l2>::type;

    static_assert(
        intersection::is_sub<l1>::value && intersection::is_sub<l2>::value
    );
}

TEST(TTypeListTest, TIntersection) {
    using l1          = NCecs::TTypeList<int, bool>;
    using l2          = NCecs::TTypeList<int, float>;
    using TResultType = decltype(l1::intersect<l2>::type());

    constexpr auto res = std::same_as<NCecs::TTypeList<int>, TResultType>;
    EXPECT_TRUE(res);
}

TEST(TTypeListTest, TCat) {
    using l1 = NCecs::TTypeList<int, char>;
    using l2 = NCecs::TTypeList<int, double>;

    static_assert(
        std::
            same_as<l1::cat<l2>::type, NCecs::TTypeList<int, char, int, double>>
    );
}

TEST(TTypeListTest, TAddDel) {
    using l1 = NCecs::TTypeList<int, char>;
    using l2 = l1::template add<double>::type;

    static_assert(std::same_as<l2, NCecs::TTypeList<double, int, char>>);
    static_assert(std::same_as<l1, l2::del<double>::type>);
}

template <std::size_t size>
struct TLessSize {
    template <typename TR>
    using op = std::bool_constant<std::less<std::size_t>{}(sizeof(TR), size)>;
};

template <typename TL, typename TR>
struct TSame : public std::bool_constant<std::same_as<TL, TR>> {};

TEST(TTypeListTest, TTestPredicate) {
    static_assert(TLessSize<8>::op<int>::value);
    static_assert(NCecs::TNeg<TLessSize<8>::template op>::op<double>::value);
    static_assert(NCecs::TPartial<TSame, double>::op<double>::value);
}

TEST(TTypeListTest, TFilterBySize) {
    static constexpr std::size_t kSize = sizeof(double);
    static_assert(std::same_as<
                  NCecs::TFilter<
                      TLessSize<kSize>::template op,
                      NCecs::TTypeList<int, char, double>>::type,
                  NCecs::TTypeList<int, char>>);

    static_assert(std::same_as<
                  NCecs::TFilter<
                      NCecs::TNeg<TLessSize<kSize>::template op>::template op,
                      NCecs::TTypeList<int, char, double>>::type,
                  NCecs::TTypeList<double>>);

    static_assert(std::same_as<
                  NCecs::TFilter<
                      NCecs::TPartial<TSame, double>::template op,
                      NCecs::TTypeList<int, char, double>>::type,
                  NCecs::TTypeList<double>>);
}

TEST(TTypeListTest, TAnyTest) {
    using TList = NCecs::TTypeList<int, char, double>;

    static_assert(
        NCecs::TAny<NCecs::TPartial<std::is_same, int>::template op, TList>::
            value
    );
    static_assert(
        NCecs::TAny<NCecs::TPartial<std::is_same, char>::template op, TList>::
            value
    );
    static_assert(
        NCecs::TAny<NCecs::TPartial<std::is_same, double>::template op, TList>::
            value
    );
    static_assert(
        !NCecs::TAny<NCecs::TPartial<std::is_same, float>::template op, TList>::
            value
    );
}

TEST(TTypeListTest, TAllTest) {
    using TList = NCecs::TTypeList<int, char, double>;

    static_assert(
        NCecs::TAll<TLessSize<2 * sizeof(double)>::template op, TList>::value
    );
    static_assert(
        !NCecs::TAll<TLessSize<sizeof(int)>::template op, TList>::value
    );
    static_assert(
        NCecs::TAll<
            NCecs::TNeg<TLessSize<sizeof(char)>::template op>::template op,
            TList>::value
    );
}

TEST(TTypeListTest, TMapTest) {
    using TList = NCecs::TTypeList<int, char, double>;
    static_assert(std::same_as<
                  typename TList::map<std::variant>::type,
                  NCecs::TTypeList<
                      std::variant<int>,
                      std::variant<char>,
                      std::variant<double>>>);
}

TEST(TTypeListTest, TAfterBeginTest) {
    using TList     = NCecs::TTypeList<int, char, double>;
    using TAfter    = typename TList::template after<int>::type;
    using TExpected = NCecs::TTypeList<char, double>;

    static_assert(std::same_as<TAfter, TExpected>);
}

TEST(TTypeListTest, TAfterMidTest) {
    using TList     = NCecs::TTypeList<int, char, double>;
    using TAfter    = typename TList::template after<char>::type;
    using TExpected = NCecs::TTypeList<double>;

    static_assert(std::same_as<TAfter, TExpected>);
}

TEST(TTypeListTest, TAfterEndTest) {
    using TList     = NCecs::TTypeList<int, char, double>;
    using TAfter    = typename TList::template after<double>::type;
    using TExpected = NCecs::TTypeList<>;

    static_assert(std::same_as<TAfter, TExpected>);
}

TEST(TTypeListTest, TEmptySizeTest) {
    using TList = NCecs::TTypeList<>;
    static_assert(TList::size == 0);
}

TEST(TTypeListTest, TNonEmptySizeTest) {
    using TList = NCecs::TTypeList<int, char, double>;
    static_assert(TList::size == 3);
}

TEST(TTypeListTest, TListOfListsTest) {
    using TListOfLists = NCecs::TTypeList<
        NCecs::TTypeList<int, char>,
        NCecs::TTypeList<double, std::string>>;
    using TMixList = NCecs::TTypeList<std::string, TListOfLists>;

    static_assert(TListOfLists::bind_functor([]<typename... T> {
        return (NCecs::CIsInstanceOf<T, NCecs::TTypeList> && ...);
    })());
    static_assert(!TMixList::bind_functor([]<typename... T> {
        return (NCecs::CIsInstanceOf<T, NCecs::TTypeList> && ...);
    })());
}

template <typename TL, typename TR>
struct TLess : public std::bool_constant<
                   std::less<std::size_t>{}(sizeof(TL), sizeof(TR))> {};

TEST(TTypeListTest, TTestQuickSort) {
    using l        = NCecs::TTypeList<int, char, std::string, double>;
    using expected = NCecs::TTypeList<std::string, double, int, char>;

    static_assert(std::same_as<NCecs::TQuickSort<TLess, l>::type, expected>);
}

TEST(TTypeListTest, TTestSimpleTraits) {
    using simple_traits = NCecs::TTypeListTraits<NCecs::TTypeList<int, char>>;

    static_assert(simple_traits::is_trivially_default_constructible::value);
    static_assert(simple_traits::is_trivially_destructible::value);
    static_assert(simple_traits::is_trivially_copy_assignable::value);
    static_assert(simple_traits::is_trivially_move_assignable::value);
    static_assert(simple_traits::is_trivially_copy_constructible::value);
    static_assert(simple_traits::is_trivially_move_constructible::value);

    static_assert(simple_traits::is_default_constructible::value);
    static_assert(simple_traits::is_copy_assignable::value);
    static_assert(simple_traits::is_move_assignable::value);
    static_assert(simple_traits::is_copy_constructible::value);
    static_assert(simple_traits::is_move_constructible::value);
}

TEST(TTypeListTest, TTestEmptyTraits) {
    using traits = NCecs::TTypeListTraits<NCecs::TTypeList<>>;

    static_assert(traits::is_trivially_default_constructible::value);
    static_assert(traits::is_trivially_destructible::value);
    static_assert(traits::is_trivially_copy_assignable::value);
    static_assert(traits::is_trivially_move_assignable::value);
    static_assert(traits::is_trivially_copy_constructible::value);
    static_assert(traits::is_trivially_move_constructible::value);

    static_assert(traits::is_default_constructible::value);
    static_assert(traits::is_copy_assignable::value);
    static_assert(traits::is_move_assignable::value);
    static_assert(traits::is_copy_constructible::value);
    static_assert(traits::is_move_constructible::value);
}

TEST(TTypeListTest, TTestNonTrivialTraits) {
    using traits = NCecs::TTypeListTraits<NCecs::TTypeList<int, std::string>>;

    static_assert(!traits::is_trivially_default_constructible::value);
    static_assert(!traits::is_trivially_destructible::value);
    static_assert(!traits::is_trivially_copy_assignable::value);
    static_assert(!traits::is_trivially_move_assignable::value);
    static_assert(!traits::is_trivially_copy_constructible::value);
    static_assert(!traits::is_trivially_move_constructible::value);

    static_assert(traits::is_default_constructible::value);
    static_assert(traits::is_copy_assignable::value);
    static_assert(traits::is_move_assignable::value);
    static_assert(traits::is_copy_constructible::value);
    static_assert(traits::is_move_constructible::value);
}

TEST(TTypeListTest, TTestNonDefaultConstructilbleTraits) {
    struct TNonDefaultConstructible {
        TNonDefaultConstructible() = delete;
    };

    using traits =
        NCecs::TTypeListTraits<NCecs::TTypeList<TNonDefaultConstructible>>;

    static_assert(!traits::is_trivially_default_constructible::value);
    static_assert(traits::is_trivially_destructible::value);
    static_assert(traits::is_trivially_copy_assignable::value);
    static_assert(traits::is_trivially_move_assignable::value);
    static_assert(traits::is_trivially_copy_constructible::value);
    static_assert(traits::is_trivially_move_constructible::value);

    static_assert(!traits::is_default_constructible::value);
    static_assert(traits::is_copy_assignable::value);
    static_assert(traits::is_move_assignable::value);
    static_assert(traits::is_copy_constructible::value);
    static_assert(traits::is_move_constructible::value);
}

TEST(TTypeListTest, TTestNonCopyAssignableTraits) {
    struct TNonCopyAssignable {
        TNonCopyAssignable& operator=(const TNonCopyAssignable&) = delete;
    };

    using traits = NCecs::TTypeListTraits<NCecs::TTypeList<TNonCopyAssignable>>;

    static_assert(traits::is_trivially_default_constructible::value);
    static_assert(traits::is_trivially_destructible::value);
    static_assert(!traits::is_trivially_copy_assignable::value);
    static_assert(!traits::is_trivially_move_assignable::value);
    static_assert(traits::is_trivially_copy_constructible::value);
    static_assert(traits::is_trivially_move_constructible::value);

    static_assert(traits::is_default_constructible::value);
    static_assert(!traits::is_copy_assignable::value);
    static_assert(!traits::is_move_assignable::value);
    static_assert(traits::is_copy_constructible::value);
    static_assert(traits::is_move_constructible::value);
}

TEST(TTypeListTest, TTestNonMoveAssignableTraits) {
    struct TNonMoveAssignable {
        TNonMoveAssignable& operator=(TNonMoveAssignable&&) = delete;
    };

    using traits = NCecs::TTypeListTraits<NCecs::TTypeList<TNonMoveAssignable>>;

    static_assert(traits::is_trivially_default_constructible::value);
    static_assert(traits::is_trivially_destructible::value);
    static_assert(!traits::is_trivially_copy_assignable::value);
    static_assert(!traits::is_trivially_move_assignable::value);
    static_assert(!traits::is_trivially_copy_constructible::value);
    static_assert(!traits::is_trivially_move_constructible::value);

    static_assert(traits::is_default_constructible::value);
    static_assert(!traits::is_copy_assignable::value);
    static_assert(!traits::is_move_assignable::value);
    static_assert(!traits::is_copy_constructible::value);
    static_assert(!traits::is_move_constructible::value);
}

TEST(TTypeListTest, TTestNonCopyConstructibleTraits) {
    struct TNonCopyConstructible {
        TNonCopyConstructible() = default;

        TNonCopyConstructible(const TNonCopyConstructible&) = delete;
    };

    using traits =
        NCecs::TTypeListTraits<NCecs::TTypeList<TNonCopyConstructible>>;

    static_assert(traits::is_trivially_default_constructible::value);
    static_assert(traits::is_trivially_destructible::value);
    static_assert(traits::is_trivially_copy_assignable::value);
    static_assert(traits::is_trivially_move_assignable::value);
    static_assert(!traits::is_trivially_copy_constructible::value);
    static_assert(!traits::is_trivially_move_constructible::value);

    static_assert(traits::is_default_constructible::value);
    static_assert(traits::is_copy_assignable::value);
    static_assert(traits::is_move_assignable::value);
    static_assert(!traits::is_copy_constructible::value);
    static_assert(!traits::is_move_constructible::value);
}

TEST(TTypeListTest, TTestNonMoveConstructibleTraits) {
    struct TNonMoveConstructible {
        TNonMoveConstructible(TNonMoveConstructible&&)      = delete;
        TNonMoveConstructible(const TNonMoveConstructible&) = default;
    };

    using traits =
        NCecs::TTypeListTraits<NCecs::TTypeList<TNonMoveConstructible>>;

    static_assert(!traits::is_trivially_default_constructible::value);
    static_assert(traits::is_trivially_destructible::value);
    static_assert(!traits::is_trivially_copy_assignable::value);
    static_assert(!traits::is_trivially_move_assignable::value);
    static_assert(traits::is_trivially_copy_constructible::value);
    static_assert(!traits::is_trivially_move_constructible::value);

    static_assert(!traits::is_default_constructible::value);
    static_assert(!traits::is_copy_assignable::value);
    static_assert(!traits::is_move_assignable::value);
    static_assert(traits::is_copy_constructible::value);
    static_assert(!traits::is_move_constructible::value);
}

TEST(TTypeListTest, TTestComplexTraits) {
    struct TNonDefaultConstructible {
        TNonDefaultConstructible() = delete;
    };
    struct TNonCopyAssignable {
        TNonCopyAssignable& operator=(const TNonCopyAssignable&) = delete;
    };
    struct TNonMoveAssignable {
        TNonMoveAssignable& operator=(TNonMoveAssignable&&) = delete;
    };
    struct TNonCopyConstructible {
        TNonCopyConstructible() = default;

        TNonCopyConstructible(const TNonCopyConstructible&) = delete;
    };
    struct TNonMoveConstructible {
        TNonMoveConstructible(TNonMoveConstructible&&)      = delete;
        TNonMoveConstructible(const TNonMoveConstructible&) = default;
    };

    using traits = NCecs::TTypeListTraits<NCecs::TTypeList<
        TNonDefaultConstructible,
        TNonCopyConstructible,
        TNonMoveAssignable,
        TNonCopyAssignable,
        TNonMoveAssignable>>;

    static_assert(!traits::is_trivially_default_constructible::value);
    static_assert(traits::is_trivially_destructible::value);
    static_assert(!traits::is_trivially_copy_assignable::value);
    static_assert(!traits::is_trivially_move_assignable::value);
    static_assert(!traits::is_trivially_copy_constructible::value);
    static_assert(!traits::is_trivially_move_constructible::value);

    static_assert(!traits::is_default_constructible::value);
    static_assert(!traits::is_copy_assignable::value);
    static_assert(!traits::is_move_assignable::value);
    static_assert(!traits::is_copy_constructible::value);
    static_assert(!traits::is_move_constructible::value);
}

template <typename... T>
struct TTestBindWrapper;

template <typename TFirst, typename... TRest>
struct TTestBindWrapper<TFirst, TRest...> {
    using type = TFirst;
};

template <>
struct TTestBindWrapper<> {
    using type = void;
};

TEST(TTypeListTest, TTestBindType) {
    using TList = NCecs::TTypeList<int, char, double>;

    static_assert(
        std::same_as<TList::template bind_type<TTestBindWrapper>::type, int>
    );
    static_assert(
        std::same_as<
            NCecs::TTypeList<>::template bind_type<TTestBindWrapper>::type,
            void>
    );
}

TEST(TTypeListTest, TTestBindConstexprLambda) {
    using TList = NCecs::TTypeList<int, char, double>;

    auto functor = []<typename... T>(std::size_t factor) {
        return factor * (sizeof(T) + ...);
    };
    constexpr auto res = TList::bind_functor(functor)(2);
    static_assert(res == (sizeof(int) + sizeof(char) + sizeof(double)) * 2);
}

TEST(TTypeListTest, TTestBindStatefullLambda) {
    using TList = NCecs::TTypeList<int, char, double>;

    std::size_t count = 0;

    auto binded = TList::bind_functor([&count]<typename... T>() {
        ((++count, (void)sizeof(T)), ...);
    });
    EXPECT_EQ(count, 0);
    binded();
    EXPECT_EQ(count, 3);
    binded();
    EXPECT_EQ(count, 6);
}
#include "type_list.hpp"

#include <gtest/gtest.h>

#include <type_traits>

#include "buffer_layout.hpp"
#include "soa.hpp"
#include "world.hpp"

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

TEST(TBufferLayoutTest, TTestConstantsSorted) {
    using layout =
        NCecs::TBufferLayout<32, NCecs::TTypeList<double, int, short>>;

    static_assert(layout::kMaxElements == 2);
    static_assert(layout::kAlign == 8);
    static_assert(layout::kChunkSize == 14);
    static_assert(layout::offset<double>::value == 0);
    static_assert(
        layout::offset<int>::value == sizeof(double) * layout::kMaxElements
    );
    static_assert(
        layout::offset<short>::value == sizeof(double) * layout::kMaxElements +
                                            sizeof(int) * layout::kMaxElements
    );
}

TEST(TBufferLayoutTest, TTestConstantsUnsorted) {
    using layout = NCecs::TBufferLayout<32, NCecs::TTypeList<char, int, short>>;

    static_assert(layout::kMaxElements == 4);
    static_assert(layout::kAlign == 4);
    static_assert(layout::kChunkSize == 7);
    static_assert(
        layout::offset<char>::value == sizeof(int) * layout::kMaxElements +
                                           sizeof(short) * layout::kMaxElements
    );
    static_assert(layout::offset<int>::value == 0);
    static_assert(
        layout::offset<short>::value == sizeof(int) * layout::kMaxElements
    );
}

TEST(TBufferLayoutTest, TTestSortedUnsortedEquality) {
    using sorted_layout =
        NCecs::TBufferLayout<32, NCecs::TTypeList<double, int, char>>;
    using unsorted_layout =
        NCecs::TBufferLayout<32, NCecs::TTypeList<char, double, int>>;

    static_assert(sorted_layout::kMaxElements == unsorted_layout::kMaxElements);
    static_assert(sorted_layout::kAlign == unsorted_layout::kAlign);
    static_assert(sorted_layout::kChunkSize == unsorted_layout::kChunkSize);
    static_assert(
        sorted_layout::offset<char>::value ==
        unsorted_layout::offset<char>::value
    );
    static_assert(
        sorted_layout::offset<int>::value == unsorted_layout::offset<int>::value
    );
    static_assert(
        sorted_layout::offset<double>::value ==
        unsorted_layout::offset<double>::value
    );
}

template <typename... T>
class TTestAllocator {
  public:
    static constexpr std::size_t kSize = 1 << 10;

  public:
    using TLayout = NCecs::TBufferLayout<kSize, NCecs::TTypeList<T...>>;
    using TVal    = NCecs::TSOAElementRef<TLayout, TTestAllocator>;

  public:
    TVal get() {
        return TVal(static_cast<void*>(&memory_[0]), count_++);
    }

    void init(TVal val) {
        val.init(typename TLayout::types{});
    }

    void deinit(TVal val) {
        val.deinit(typename TLayout::types{});
    }

  private:
    alignas(TLayout::kAlign) std::array<std::byte, kSize> memory_;
    std::size_t count_ = 0;
};

struct TExStruct {
    TExStruct() {
        ++constructed;
    }
    ~TExStruct() {
        ++destroyed;
    }

    int a         = 100500;
    std::string b = {"hello"};

    static std::size_t constructed;
    static std::size_t destroyed;
};

std::size_t TExStruct::constructed = 0;
std::size_t TExStruct::destroyed   = 0;

TEST(TSOAElementTest, TTestAccess) {
    TTestAllocator<int, double, TExStruct> allocator;
    auto val = allocator.get();

    EXPECT_EQ(TExStruct::constructed, 0);
    EXPECT_EQ(TExStruct::destroyed, 0);

    allocator.init(val);

    EXPECT_EQ(TExStruct::constructed, 1);
    EXPECT_EQ(TExStruct::destroyed, 0);

    EXPECT_EQ(val.get<TExStruct>().a, 100500);
    EXPECT_EQ(val.get<TExStruct>().b, std::string{"hello"});

    val.get<int>()    = 42;
    val.get<double>() = 0.1f;
    EXPECT_EQ(val.get<int>(), 42);
    EXPECT_EQ(val.get<double>(), 0.1f);

    allocator.deinit(val);

    EXPECT_EQ(TExStruct::constructed, 1);
    EXPECT_EQ(TExStruct::destroyed, 1);

    // NOTE: code below is normal for test, but in real cases is broken

    EXPECT_EQ(val.get<int>(), 42);
    EXPECT_EQ(val.get<double>(), 0.1f);

    allocator.init(val);
    allocator.init(val);
    allocator.init(val);

    EXPECT_EQ(TExStruct::constructed, 4);
    EXPECT_EQ(TExStruct::destroyed, 1);

    allocator.deinit(val);

    EXPECT_EQ(TExStruct::constructed, 4);
    EXPECT_EQ(TExStruct::destroyed, 2);
}

TEST(TSOAElementTest, TTestCopyFrom) {
    using TAllocator = TTestAllocator<int, short>;
    using TRef       = TAllocator::TVal;
    TAllocator allocator;
    TRef val[] = {
        allocator.get(),
        allocator.get(),
    };

    allocator.init(val[0]);
    allocator.init(val[1]);

    val[0].get<int>()   = 42;
    val[0].get<short>() = 16;
    EXPECT_EQ(val[0].get<int>(), 42);
    EXPECT_EQ(val[0].get<short>(), 16);

    val[1].get<int>()   = 0;
    val[1].get<short>() = 0;
    EXPECT_EQ(val[1].get<int>(), 0);
    EXPECT_EQ(val[1].get<short>(), 0);

    val[1].copy_from(val[0]);

    EXPECT_EQ(val[1].get<int>(), 42);
    EXPECT_EQ(val[1].get<short>(), 16);
    EXPECT_EQ(val[0].get<int>(), 42);
    EXPECT_EQ(val[0].get<short>(), 16);
}

TEST(TTestAllocator, TTestAlignedAlloc) {
    static constexpr std::size_t align = 1 << 10;
    static constexpr uint64_t mask     = align - 1;

    NCecs::TStdAlignedAllocator allocator;

    auto* ptr = allocator.aligned_alloc(align, 100);
    EXPECT_EQ(reinterpret_cast<uint64_t>(ptr) & mask, 0);
    allocator.free(ptr);
}

TEST(TSOAContainer, TTestConstructionDestruction) {
    static constexpr std::size_t size = 4 << 10;
    using types                       = NCecs::TTypeList<int, char, double>;
    NCecs::TSOAContainer<size, types> container;
}

TEST(TSOAContainer, TTestInsertion) {
    static constexpr std::size_t size = 4 << 10;
    using types                       = NCecs::TTypeList<int, char, double>;
    NCecs::TSOAContainer<size, types> container;
    auto ref = container.create();
    EXPECT_EQ(container.size(), 1);
}

TEST(TSOAContainer, TTestLazyDestroy) {
    static constexpr std::size_t size = 4 << 10;
    using types                       = NCecs::TTypeList<int, char, double>;
    NCecs::TSOAContainer<size, types> container;
    auto ref = container.create();
    container.destroy(ref);
    EXPECT_EQ(container.size(), 1);
    container.commit();
    EXPECT_EQ(container.size(), 0);
}

TEST(TSOAContainer, TTestSwapWithEnd) {
    // Not normal case
    static constexpr std::size_t size = 4 << 10;
    using types                       = NCecs::TTypeList<int, char, double>;
    NCecs::TSOAContainer<size, types> container;
    auto ref                      = container.create();
    auto ref_copy                 = ref;
    container.create().get<int>() = 10;
    container.create().get<int>() = 42;
    container.destroy(ref);
    EXPECT_FALSE(ref.valid());
    EXPECT_TRUE(ref_copy.valid());
    EXPECT_EQ(container.size(), 3);
    container.commit();
    EXPECT_EQ(container.size(), 2);
    EXPECT_EQ(ref_copy.get<int>(), 42);
}

TEST(TSOAContainer, TTestIteration) {
    static constexpr std::size_t size      = 4 << 10;
    static constexpr std::size_t nElements = 4 << 10;
    using types                            = NCecs::TTypeList<std::size_t>;
    NCecs::TSOAContainer<size, types> container;
    for (std::size_t i = 0; i < nElements; ++i) {
        container.create().get<std::size_t>() = i;
    }

    EXPECT_EQ(container.size(), nElements);

    std::size_t expected_idx = 0;
    for (auto ref : container) {
        EXPECT_EQ(ref.get<std::size_t>(), expected_idx);
        ++expected_idx;
        container.destroy(ref);
    }

    container.commit();
    EXPECT_EQ(container.size(), 0);
}

TEST(TSOAContainer, TTestSelectionDestroying) {
    static constexpr std::size_t size      = 4 << 10;
    static constexpr std::size_t nElements = 4 << 10;
    using types                            = NCecs::TTypeList<std::size_t>;
    NCecs::TSOAContainer<size, types> container;
    for (std::size_t i = 0; i < nElements; ++i) {
        container.create().get<std::size_t>() = i;
    }

    EXPECT_EQ(container.size(), nElements);

    std::size_t destroyed = 0;
    for (auto ref : container) {
        if (ref.get<std::size_t>() % 7 == 1) {
            container.destroy(ref);
            ++destroyed;
        }
    }
    container.commit();
    EXPECT_EQ(container.size(), nElements - destroyed);
}

TEST(TSOAContainer, TTestDestroyString) {
    static constexpr std::size_t size      = 4 << 10;
    static constexpr std::size_t nElements = 7;
    using types = NCecs::TTypeList<std::size_t, std::string>;
    NCecs::TSOAContainer<size, types> container;
    for (std::size_t i = 0; i < nElements; ++i) {
        auto ref = container.create();

        ref.get<std::size_t>() = i;
        ref.get<std::string>() = "first";
    }

    EXPECT_EQ(container.size(), nElements);

    std::size_t destroyed = 0;
    for (auto ref : container) {
        const auto& stored = ref.get<std::string>();
        EXPECT_EQ(stored, std::string{"first"});
        if (ref.get<std::size_t>() % 3 == 1) {
            container.destroy(ref);
            ++destroyed;
        }
    }

    container.commit();
    EXPECT_EQ(container.size(), nElements - destroyed);
}

TEST(TSOAContainer, TTestSequensionalAdd) {
    static constexpr std::size_t size      = 4 << 10;
    static constexpr std::size_t nElements = 1 << 20;
    using types = NCecs::TTypeList<std::size_t, std::string>;
    NCecs::TSOAContainer<size, types> container;
    for (std::size_t i = 0; i < nElements; ++i) {
        auto ref = container.create();

        ref.get<std::size_t>() = i;
        ref.get<std::string>() = "first";
    }

    EXPECT_EQ(container.size(), nElements);

    std::size_t destroyed = 0;
    for (auto ref : container) {
        if (ref.get<std::size_t>() % 3 == 1) {
            container.destroy(ref);
            ++destroyed;
        }
    }

    container.commit();
    EXPECT_EQ(container.size(), nElements - destroyed);

    for (std::size_t i = 0; i < nElements; ++i) {
        auto ref               = container.create();
        ref.get<std::size_t>() = i + nElements;
        ref.get<std::string>() = "second";
    }

    EXPECT_EQ(container.size(), 2 * nElements - destroyed);

    std::size_t idx = 0;
    for (const auto& elem : container) {
        if (idx < nElements - destroyed) {
            EXPECT_EQ(elem.get<std::string>(), "first");
        } else {
            EXPECT_EQ(elem.get<std::string>(), "second");
        }
        ++idx;
    }
}

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
    world.select<decltype(func), int>(func);

    EXPECT_EQ(string_count, kSize / 4);
    EXPECT_EQ(total_calls, kSize);

    string_count = 0;
    total_calls  = 0;
    world.select<decltype(func), char>(func);
    EXPECT_EQ(string_count, 0);
    EXPECT_EQ(total_calls, kSize / 2);

    string_count = 0;
    total_calls  = 0;
    world.select<decltype(func), int>(func);
    world.select<decltype(func), int>(func);

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
    world.select<decltype(assertion), int>(assertion);
    world.commit();

    EXPECT_FALSE(res);

    res = false;
    world.select<decltype(assertion), int>(assertion);
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
    world.select<decltype(func), int>(func);

    EXPECT_EQ(total_calls, kSize);
    EXPECT_EQ(moves_count, kSize / 2);

    std::size_t moves_history = moves_count;
    total_calls               = 0;
    moves_count               = 0;

    world.select<decltype(func), int>(func);

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
    world1.select<decltype(func), int>(func);

    auto world1_total_calls = total_calls;
    auto world1_moves_count = moves_count;

    total_calls = 0;
    moves_count = 0;
    world2.select<decltype(func), int>(func);

    auto world2_total_calls = total_calls;
    auto world2_moves_count = moves_count;

    EXPECT_EQ(world1_total_calls, world2_total_calls);
    EXPECT_EQ(world1_moves_count, world2_moves_count);
}
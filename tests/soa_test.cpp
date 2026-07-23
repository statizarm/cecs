#include "soa.hpp"

#include <gtest/gtest.h>

#include <type_traits>

#include "buffer_layout.hpp"

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

TEST(TSOAElementTest, TTestReplaceFrom) {
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

    val[1].replace_from(val[0]);

    EXPECT_EQ(val[1].get<int>(), 42);
    EXPECT_EQ(val[1].get<short>(), 16);
    EXPECT_EQ(val[0].get<int>(), 42);
    EXPECT_EQ(val[0].get<short>(), 16);
}

struct TCStruct {
    std::size_t val = 0;
    TCStruct(const TCStruct& other) {
        val = other.val;
        ++copy_constructed;
    }
    TCStruct(TCStruct&& other) {
        val = other.val;
        ++move_constructed;
    }

    static std::size_t copy_constructed;
    static std::size_t move_constructed;
};

std::size_t TCStruct::copy_constructed = 0;
std::size_t TCStruct::move_constructed = 0;

struct TAStruct {
    std::size_t val = 0;
    TAStruct& operator=(const TAStruct& other) {
        val = other.val;
        ++copy_assigned;
        return *this;
    }

    TAStruct& operator=(TAStruct&& other) {
        val = other.val;
        ++move_assigned;
        return *this;
    }

    static std::size_t copy_assigned;
    static std::size_t move_assigned;
};

std::size_t TAStruct::copy_assigned = 0;
std::size_t TAStruct::move_assigned = 0;

TEST(TSOAElementTest, TTestConstructionDestructionWithReplace) {
    using TAllocator = TTestAllocator<TCStruct, TAStruct>;
    using TRef       = TAllocator::TVal;
    TAllocator allocator;
    TRef val[] = {
        allocator.get(),
        allocator.get(),
    };

    val[1].get<TCStruct>().val = 100;
    val[1].get<TAStruct>().val = 200;

    static_assert(
        std::is_copy_constructible<TCStruct>::value &&
        std::is_move_constructible<TCStruct>::value
    );

    static_assert(
        std::is_copy_assignable<TAStruct>::value &&
        std::is_move_assignable<TAStruct>::value
    );
    val[0].replace_from(val[1]);

    EXPECT_EQ(TAStruct::copy_assigned, 1);
    EXPECT_EQ(TAStruct::move_assigned, 0);

    EXPECT_EQ(TCStruct::copy_constructed, 1);
    EXPECT_EQ(TCStruct::move_constructed, 0);

    EXPECT_EQ(val[0].get<TCStruct>().val, 100);
    EXPECT_EQ(val[0].get<TAStruct>().val, 200);
    EXPECT_EQ(val[1].get<TCStruct>().val, 100);
    EXPECT_EQ(val[1].get<TAStruct>().val, 200);

    val[0].get<TCStruct>().val = 10;
    val[0].get<TAStruct>().val = 20;

    val[1].replace_from(std::move(val[0]));

    EXPECT_EQ(TAStruct::copy_assigned, 1);
    EXPECT_EQ(TAStruct::move_assigned, 1);

    EXPECT_EQ(TCStruct::copy_constructed, 1);
    EXPECT_EQ(TCStruct::move_constructed, 1);

    EXPECT_EQ(val[0].get<TCStruct>().val, 10);
    EXPECT_EQ(val[0].get<TAStruct>().val, 20);
    EXPECT_EQ(val[1].get<TCStruct>().val, 10);
    EXPECT_EQ(val[1].get<TAStruct>().val, 20);
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
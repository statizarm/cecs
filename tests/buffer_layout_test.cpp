#include "buffer_layout.hpp"

#include <gtest/gtest.h>

#include "type_list.hpp"

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

TEST(TBufferLayoutTest, TTestStorage) {
    using sorted_layout =
        NCecs::TBufferLayout<32, NCecs::TTypeList<double, int, char>>;

    using TStorage = NCecs::TLayoutStorage<sorted_layout>;
    static_assert(sizeof(TStorage) == 32);
    TStorage storage;
    storage.get<double>(0) = 42.l;
    storage.get<int>(0)    = 100500;
    storage.get<char>(0)   = 'a';

    EXPECT_EQ(storage.get<double>(0), 42.l);
    EXPECT_EQ(storage.get<int>(0), 100500);
    EXPECT_EQ(storage.get<char>(0), 'a');
}

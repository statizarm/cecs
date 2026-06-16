#pragma once

#include <concepts>
#include <cstdlib>

namespace NCecs {

template <typename TAlloc, std::size_t align>
concept CAlignedAllocator = requires(std::size_t size) {
    { TAlloc::aligned_alloc(align, size) } -> std::same_as<void*>;
    { TAlloc::free(TAlloc::aligned_alloc(align, size)) } -> std::same_as<void>;
};

class TStdAlignedAllocator {
  private:
    using TThis = TStdAlignedAllocator;

  public:
    TStdAlignedAllocator() = default;

    TStdAlignedAllocator(const TThis& other) = delete;
    TStdAlignedAllocator(TThis&& other)      = delete;

    TThis& operator=(const TThis& other) = delete;
    TThis& operator=(TThis&& other)      = delete;

  public:
    static void* aligned_alloc(std::size_t align, std::size_t size) {
        return std::aligned_alloc(align, size);
    }

    static void free(void* ptr) {
        std::free(ptr);
    }
};

}  // namespace NCecs
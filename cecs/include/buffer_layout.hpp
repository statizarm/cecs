#pragma once

#include <algorithm>
#include <functional>

#include "type_list.hpp"
#include "utils.hpp"

namespace NCecs {

template <typename TL, typename TR>
struct TAlignLess : public std::bool_constant<
                        std::less<std::size_t>{}(alignof(TL), alignof(TR))> {};

template <CIsInstanceOf<TTypeList> T>
using type_aligned_t = typename TQuickSort<TAlignLess, T>::type;

template <std::size_t size, CIsInstanceOf<TTypeList> T>
struct TBufferLayoutImpl;

template <std::size_t size, typename... T>
    requires(
        size > (sizeof(T) + ...) &&
        std::same_as<TTypeList<T...>, type_aligned_t<TTypeList<T...>>>
    )
struct TBufferLayoutImpl<size, TTypeList<T...>> {
  private:
    template <COneOf<T...> TT, typename... TTypes>
    struct TOffset;

  public:
    static constexpr std::size_t kChunkSize   = (sizeof(T) + ...);
    static constexpr std::size_t kMaxElements = size / kChunkSize;
    static constexpr std::size_t kAlign       = std::max({alignof(T)...});

    template <COneOf<T...> TT>
    using offset = TOffset<TT, T...>;
    using types  = TTypeList<T...>;
    using TFirst = typename types::template head<types>::type;

  private:
    template <COneOf<T...> TT>
    struct TOffset<TT> {
        static constexpr std::size_t value = 0;
    };

    template <COneOf<T...> TT, typename TFirst, typename... TRest>
    struct TOffset<TT, TFirst, TRest...> {
        static constexpr std::size_t value = std::conditional_t<
            std::same_as<TT, TFirst>, TValueHolder<std::size_t, 0>,
            TValueHolder<
                std::size_t, sizeof(TFirst) * kMaxElements +
                                 TOffset<TT, TRest...>::value>>::value;
    };
};

template <std::size_t size, CIsInstanceOf<TTypeList> T>
struct TBufferLayout : public TBufferLayoutImpl<size, type_aligned_t<T>> {};

template <typename T>
struct TIsInstanceOfLayout : public std::false_type {};

template <std::size_t size, CIsInstanceOf<TTypeList> T>
struct TIsInstanceOfLayout<TBufferLayout<size, T>> : public std::true_type {};

template <typename T>
concept CLayout = TIsInstanceOfLayout<T>::value;

}  // namespace NCecs
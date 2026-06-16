#pragma once

#include <concepts>

namespace NCecs {

template <typename T, typename... TTypes>
concept COneOf = (std::same_as<T, TTypes> || ...);

template <typename T, template <typename...> class TTemplate>
struct TIsInstanceOf : public std::false_type {};

template <template <typename...> class TTemplate, typename... TArgs>
struct TIsInstanceOf<TTemplate<TArgs...>, TTemplate> : public std::true_type {};

template <typename T, template <typename...> class TTemplate>
concept CIsInstanceOf = TIsInstanceOf<T, TTemplate>::value;

template <typename T, T val>
struct TValueHolder {
    static constexpr T value = val;
};

}  // namespace NCecs
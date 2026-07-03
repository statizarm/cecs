#pragma once

#include <type_traits>
#include <utility>

#include "utils.hpp"

namespace NCecs {

template <typename... TType>
struct TTypeList;

template <template <typename> class TP, CIsInstanceOf<TTypeList> TList>
struct TFilter;

template <template <typename> class TP>
struct TNeg;

template <template <typename> class TLeft, template <typename> class TRight>
struct TAnd;

template <template <typename> class TLeft, template <typename> class TRight>
struct TOr;

template <template <typename...> class TP, typename T>
struct TPartial;

template <typename... TType>
struct TTypeList {
  private:
    template <typename... T>
    struct TIntersectHelper;

    template <typename... T>
    struct TCatHelper;

    template <typename... T>
    struct THeadHelper;

    template <template <typename...> class, typename...>
    struct TBindTypeHelper;

    template <typename TFunctor>
    struct TBindFunctorHelper;

    template <template <typename> class TMapper>
    struct TMapHelper;

    template <typename T, typename... TT>
        requires(COneOf<T, TT...>)
    struct TAfterHelper;

  private:
    using TThis = TTypeList<TType...>;

  public:
    template <typename T>
    using has = std::bool_constant<COneOf<T, TType...>>;
    template <CIsInstanceOf<TTypeList> T>
    using is_sub = std::bool_constant<(T::template has<TType>::value && ...)>;
    template <CIsInstanceOf<TTypeList> T>
    using eq = std::bool_constant<
        (T::template is_sub<TTypeList<TType...>>::value && is_sub<T>::value)>;
    template <CIsInstanceOf<TTypeList> T>
    using ne = std::bool_constant<!eq<T>::value>;

    template <template <typename...> class TWrapper>
    using bind_type = TWrapper<TType...>;
    template <typename T>
    using add = std::conditional<
        has<T>::value, TTypeList<TType...>, TTypeList<T, TType...>>;
    template <typename T>
    using del = TFilter<
        TNeg<TPartial<std::is_same, T>::template op>::template op, TThis>;
    template <CIsInstanceOf<TTypeList> T>
    using intersect = typename T::template bind_type<TIntersectHelper>;
    template <CIsInstanceOf<TTypeList> T>
    using cat = typename T::template bind_type<TCatHelper>;
    template <CIsInstanceOf<TTypeList> T = TThis>
    using head = typename T::template bind_type<THeadHelper>;
    template <template <typename> class T>
    using map = TMapHelper<T>;

    template <COneOf<TType...> T>
    using after = TAfterHelper<T, TType...>;

  public:
    static constexpr std::size_t size = sizeof...(TType);

  public:
    template <CIsInstanceOf<TTypeList> T>
    consteval bool operator==(const T&) const {
        return eq<T>::value;
    }

    template <CIsInstanceOf<TTypeList> T>
    consteval bool operator!=(const T&) const {
        return ne<TTypeList<T>>::value;
    }

    template <typename TFunctor>
    static constexpr auto bind_functor(TFunctor&& func) {
        return [&]<typename... TArgs>(TArgs&&... args) {
            return func.template operator()<TType...>(
                std::forward<TArgs>(args)...
            );
        };
    }

  private:
    template <>
    struct TIntersectHelper<> {
        using type = TTypeList<>;
    };

    template <typename TFirst, typename... T>
    struct TIntersectHelper<TFirst, T...> {
        using rest = typename TIntersectHelper<T...>::type;
        using type = std::conditional_t<
            has<TFirst>::value, typename rest::template add<TFirst>::type,
            rest>;
    };

    template <typename... T>
    struct TCatHelper {
        using type = TTypeList<TType..., T...>;
    };

    template <typename T, typename... TRest>
    struct THeadHelper<T, TRest...> {
        using type = T;
    };

    template <typename TFunctor>
    struct TBindFunctorHelper {
        constexpr TBindFunctorHelper(TFunctor f)
            : f_(f) {
        }
        template <typename... TArgs>
        constexpr inline auto operator()(TArgs... args) {
            return f_.template operator()<TType...>(
                std::forward<TArgs>(args)...
            );
        }

      private:
        TFunctor f_;
    };

    template <template <typename> class TTemplate>
    struct TMapHelper {
        using type = TTypeList<TTemplate<TType>...>;
    };

    template <typename T, typename TFirst, typename... TRest>
        requires(std::same_as<T, TFirst>)
    struct TAfterHelper<T, TFirst, TRest...> {
        using type = TTypeList<TRest...>;
    };
    template <typename T, typename TFirst, typename... TRest>
        requires(!std::same_as<T, TFirst>)
    struct TAfterHelper<T, TFirst, TRest...> {
        using type = typename TAfterHelper<T, TRest...>::type;
    };
};

template <typename... T>
struct TUnique;

template <>
struct TUnique<> {
    using type = TTypeList<>;
};

template <typename T, typename... TRest>
struct TUnique<T, TRest...> {
    using rest = TUnique<TRest...>::type;
    using type = typename std::conditional_t<
        COneOf<T, TRest...>, rest,
        typename TTypeList<T>::template cat<rest>::type>;
};

template <template <typename> class TP, CIsInstanceOf<TTypeList> TList>
struct TAll : public std::bool_constant<
                  std::same_as<typename TFilter<TP, TList>::type, TList>> {};

template <template <typename> class TP, CIsInstanceOf<TTypeList> TList>
struct TAny
    : public std::bool_constant<
          !std::same_as<typename TFilter<TP, TList>::type, TTypeList<>>> {};

template <template <typename> class TP, CIsInstanceOf<TTypeList> TList>
struct TFilter;

template <template <typename> class TP>
struct TFilter<TP, TTypeList<>> {
    using type = TTypeList<>;
};

template <template <typename> class TP, typename TFirst, typename... TRest>
struct TFilter<TP, TTypeList<TFirst, TRest...>> {
    using rest = typename TFilter<TP, TTypeList<TRest...>>::type;
    using type = std::conditional_t<
        TP<TFirst>::value, typename TTypeList<TFirst>::template cat<rest>::type,
        rest>;
};

template <template <typename> class TP>
struct TNeg {
  private:
    template <typename T>
    struct TOp : public std::bool_constant<!TP<T>::value> {};

  public:
    template <typename T>
    using op = TOp<T>;
};

template <template <typename> class TLeft, template <typename> class TRight>
struct TAnd {
  private:
    template <typename T>
    struct TOp
        : public std::bool_constant<TLeft<T>::value && TRight<T>::value> {};

  public:
    template <typename T>
    using op = TOp<T>;
};

template <template <typename> class TLeft, template <typename> class TRight>
struct TOr {
  private:
    template <typename T>
    struct TOp
        : public std::bool_constant<TLeft<T>::value || TRight<T>::value> {};

  public:
    template <typename T>
    using op = TOp<T>;
};

template <template <typename...> class TP, typename T>
struct TPartial {
  private:
    template <typename... TRest>
    struct TOp : public TP<T, TRest...> {};

  public:
    template <typename... TRest>
    using op = TOp<TRest...>;
};

template <template <typename, typename> class TP, CIsInstanceOf<TTypeList> T>
struct TQuickSort;

template <template <typename, typename> class TP>
struct TQuickSort<TP, TTypeList<>> {
    using type = TTypeList<>;
};

template <
    template <typename, typename> class TP, typename TFirst, typename... TRest>
struct TQuickSort<TP, TTypeList<TFirst, TRest...>> {
    using pivot = TFirst;
    template <typename T>
    using predicate = TPartial<TP, pivot>::template op<T>;
    using left      = TQuickSort<
        TP, typename TFilter<predicate, TTypeList<TRest...>>::type>::type;
    using right = TQuickSort<
        TP, typename TFilter<
                TNeg<predicate>::template op, TTypeList<TRest...>>::type>::type;

    using type = typename left::template cat<
        TTypeList<pivot>>::type::template cat<right>::type;
};

template <CIsInstanceOf<TTypeList> T>
struct TTypeListTraits;

template <typename... T>
struct TTypeListTraits<TTypeList<T...>> {
  private:
    template <template <typename...> class TP>
    struct TTraitWrapper : public std::bool_constant<(TP<T>::value && ...)> {};

  public:
    using is_trivially_default_constructible =
        TTraitWrapper<std::is_trivially_default_constructible>;
    using is_trivially_destructible =
        TTraitWrapper<std::is_trivially_destructible>;

    using is_trivially_copy_assignable =
        TTraitWrapper<std::is_trivially_copy_assignable>;
    using is_trivially_copy_constructible =
        TTraitWrapper<std::is_trivially_copy_constructible>;

    using is_trivially_move_assignable =
        TTraitWrapper<std::is_trivially_move_assignable>;
    using is_trivially_move_constructible =
        TTraitWrapper<std::is_trivially_move_constructible>;

    using is_default_constructible =
        TTraitWrapper<std::is_default_constructible>;

    using is_copy_assignable    = TTraitWrapper<std::is_copy_assignable>;
    using is_copy_constructible = TTraitWrapper<std::is_copy_constructible>;

    using is_move_assignable    = TTraitWrapper<std::is_move_assignable>;
    using is_move_constructible = TTraitWrapper<std::is_move_constructible>;
};

}  // namespace NCecs
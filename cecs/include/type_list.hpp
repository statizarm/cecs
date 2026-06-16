#pragma once

#include <type_traits>

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
    template <CIsInstanceOf<TTypeList> T>
    struct TIntersectHelper;

    template <CIsInstanceOf<TTypeList> T>
    struct TCatHelper;

    template <CIsInstanceOf<TTypeList> T>
    struct THeadHelper;

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

    template <typename T>
    using add = std::conditional<
        has<T>::value, TTypeList<TType...>, TTypeList<T, TType...>>;
    template <typename T>
    using del = TFilter<
        TNeg<TPartial<std::is_same, T>::template op>::template op, TThis>;
    template <CIsInstanceOf<TTypeList> T>
    using intersect = TIntersectHelper<T>;
    template <CIsInstanceOf<TTypeList> T>
    using cat = TCatHelper<T>;
    template <CIsInstanceOf<TTypeList> T = TThis>
    using head = THeadHelper<T>;

    template <typename... T>
    consteval bool operator==(const TTypeList<T...>&) const {
        return eq<TTypeList<T...>>::value;
    }

    template <typename... T>
    consteval bool operator!=(const TTypeList<T...>&) const {
        return ne<TTypeList<T...>>::value;
    }

  private:
    template <>
    struct TIntersectHelper<TTypeList<>> {
        using type = TTypeList<>;
    };

    template <typename TFirst, typename... T>
    struct TIntersectHelper<TTypeList<TFirst, T...>> {
        using rest = typename TIntersectHelper<TTypeList<T...>>::type;
        using type = std::conditional_t<
            has<TFirst>::value, typename rest::template add<TFirst>::type,
            rest>;
    };

    template <typename... T>
    struct TCatHelper<TTypeList<T...>> {
        using type = TTypeList<TType..., T...>;
    };

    template <typename T, typename... TRest>
    struct THeadHelper<TTypeList<T, TRest...>> {
        using type = T;
    };
};

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
#pragma once

#include <tuple>
#include <type_traits>

#include "allocator.hpp"
#include "soa.hpp"
#include "type_list.hpp"
#include "utils.hpp"

namespace NCecs {

template <std::size_t buffer_size, typename TAllocator, typename T>
concept CSOAAplicable =
    requires(TSOAContainer<buffer_size, T, TAllocator>& args) {
        (std::forward<TSOAContainer<buffer_size, T, TAllocator>>(args));
    };

template <typename TRef, typename TWorld>
class TEntity {
    friend TWorld;

  public:
    using TThis      = TEntity<TRef, TWorld>;
    using TArchetype = typename TRef::TAvailableTypes;

  public:
    TEntity()                   = delete;
    TEntity(const TThis& other) = default;
    TEntity(TThis&& other)      = default;

    TThis& operator=(const TThis& other) = default;
    TThis& operator=(TThis&& other)      = default;

  public:
    template <typename T>
    constexpr bool has() const {
        return TArchetype::template has<T>::value;
    }

    template <typename T>
        requires(TArchetype::template has<T>::value)
    T& get() {
        return ref_.template get<T>();
    }

    template <typename T>
        requires(TArchetype::template has<T>::value)
    const T& get() const {
        return ref_.template get<T>();
    }

    template <typename T, typename... TArgs>
        requires(std::is_constructible_v<T, TArgs...>)
    auto add(TArgs... args) {
        if constexpr (TArchetype::template has<T>::value) {
            return *this;
        } else {
            auto res = world_->template move_to<
                TThis,
                typename TArchetype::template add<T>::type>(*this);
            new (&res.template get<T>()) T(std::forward<TArgs>(args)...);
            return res;
        }
    }

    template <typename T>
    auto del() {
        if constexpr (!TArchetype::template has<T>::value) {
            return *this;
        } else {
            auto res = world_->template move_to<
                TThis,
                typename TArchetype::template del<T>::type>(*this);
            return res;
        }
    }

    bool valid() {
        return ref_.valid();
    }

  private:
    explicit TEntity(TRef ref, TWorld* world)
        : ref_(ref), world_(world) {
    }

  private:
    TRef ref_;
    TWorld* world_;
};

template <CIsInstanceOf<TEntity>... TEntities>
class TVariantEntity {};

template <
    CIsInstanceOf<TTypeList> T, std::size_t buffer_size, typename TAllocator>
class TWorldImpl;

template <
    std::size_t buffer_size, typename TAllocator, CIsInstanceOf<TTypeList>... T>
    requires(CSOAAplicable<buffer_size, TAllocator, T> && ...)
class TWorldImpl<TTypeList<T...>, buffer_size, TAllocator> {
  public:
    using TThis = TWorldImpl<TTypeList<T...>, buffer_size, TAllocator>;

  public:
    TWorldImpl()
        : containers_(), snapshots_() {
        update_snapshots();
    }

    template <typename... TT>
        requires(TTypeList<std::decay_t<TT>...>::template eq<T>::value || ...)
    auto create() {
        using TContainerType =
            TMatchedContainer<TTypeList<std::decay_t<TT>...>>;

        auto& container = std::get<TContainerType>(containers_);
        return TEntity<typename TContainerType::ref, TThis>(
            container.create(), this
        );
    }

    template <typename... TT>
        requires(TTypeList<std::decay_t<TT>...>::template eq<T>::value || ...)
    auto create(TT... args) {
        using TContainerType =
            TMatchedContainer<TTypeList<std::decay_t<TT>...>>;

        auto& container = std::get<TContainerType>(containers_);
        auto res        = TEntity<typename TContainerType::ref, TThis>(
            container.create(), this
        );
        ((res.template get<TT>() = std::forward<TT>(args)), ...);
        return res;
    }

    template <CIsInstanceOf<TEntity> TT>
    void destroy(TT& entity) {
        using TContainerType = TMatchedContainer<typename TT::TArchetype>;

        auto& container = std::get<TContainerType>(containers_);
        container.destroy(entity.ref_);
    }

    template <
        CIsInstanceOf<TEntity> TFrom,
        CIsInstanceOf<TTypeList> TDestinationArchetype>
    auto move_to(TFrom& entity) {
        using TSourceContainerType =
            TMatchedContainer<typename TFrom::TArchetype>;
        using TDestinationContainerType =
            TMatchedContainer<TDestinationArchetype>;

        if constexpr (
            std::same_as<TSourceContainerType, TDestinationContainerType>
        ) {
            return entity;
        } else {
            auto& container = std::get<TDestinationContainerType>(containers_);
            auto ref        = container.create();
            ref.replace_from(entity.ref_);
            destroy(entity);
            return TEntity<decltype(ref), TThis>(ref, this);
        }
    }

    void commit() {
        ((std::get<TContainer<T>>(containers_).commit()), ...);
        update_snapshots();
    }

    template <typename... TT>
        requires(TTypeList<TT...>::template is_sub<T>::value || ...)
    auto select(auto func) {
        using TSelected = typename TFilter<
            TTypeList<TT...>::template is_sub,
            TTypeList<T...>>::type;

        // return TSubworld?

        return TSelected::bind_functor([&]<typename... TArchetype>() {
            (
                [&](TContainerSnapshot<TArchetype>& snapshot) {
                    for (auto it = snapshot.first; it != snapshot.second;
                         ++it) {
                        if (!it.erased()) {
                            func(TEntity(*it, this));
                        }
                    }
                }(std::get<TContainerSnapshot<TArchetype>>(snapshots_)),
                ...);
        })();
    }

    template <typename TFunc, typename... TT>
        requires(TTypeList<TT...>::template is_sub<T>::value || ...)
    void select_batch(TFunc func) {
        call_batch_impl(
            func,
            typename TFilter<
                TTypeList<TT...>::template is_sub,
                TTypeList<T...>>::type{}
        );
    }

  private:
    inline void update_snapshots() {
        ((std::get<TContainerSnapshot<T>>(snapshots_) = TContainerSnapshot<T>(
              std::get<TContainer<T>>(containers_).begin(),
              std::get<TContainer<T>>(containers_).end()
          )),
         ...);
    }

    template <typename TFunc, typename... TArchetype>
    inline void call_impl(TFunc func, TTypeList<TArchetype...>) {
        (
            [this, &func](TContainerSnapshot<TArchetype> snapshot) {
                for (auto begin = snapshot.first; begin != snapshot.second;
                     ++begin) {
                    if (!begin.erased()) {
                        func(TEntity<decltype(*begin), TThis>(*begin, this));
                    }
                }
            }(std::get<TContainerSnapshot<TArchetype>>(snapshots_)),
            ...);
    }

    template <typename TFunc, typename... TArchetype>
    inline void call_batch_impl(TFunc, TTypeList<TArchetype...>) {
    }

  private:
    template <COneOf<T...> TT>
    using TContainer = TSOAContainer<buffer_size, TT, TAllocator>;

    template <COneOf<T...> TT>
    using TContainerSnapshot = std::pair<
        typename TContainer<TT>::iterator, typename TContainer<TT>::iterator>;

    using TContainers         = std::tuple<TContainer<T>...>;
    using TContainersSnapshot = std::tuple<TContainerSnapshot<T>...>;

    template <CIsInstanceOf<TTypeList> TArchetype>
        requires(TArchetype::template eq<T>::value || ...)
    using TMatchedArchetypes =
        typename TFilter<TArchetype::template eq, TTypeList<T...>>::type;

    template <CIsInstanceOf<TTypeList> TArchetype>
        requires(TArchetype::template eq<T>::value || ...)
    using TMatchedArchetype =
        typename TMatchedArchetypes<TArchetype>::template head<>::type;

    template <CIsInstanceOf<TTypeList> TArchetype>
        requires(TArchetype::template eq<T>::value || ...)
    using TMatchedContainer = TContainer<TMatchedArchetype<TArchetype>>;

    template <CIsInstanceOf<TTypeList> TArchetype>
        requires(TArchetype::template eq<T>::value || ...)
    using TMatchedContainerSnapshot =
        TContainerSnapshot<TMatchedArchetype<TArchetype>>;

  private:
    TContainers containers_;
    TContainersSnapshot snapshots_;
};

template <
    CIsInstanceOf<TTypeList> TArchetypes, std::size_t buffer_size = 4 << 10,
    typename TAllocator = TStdAlignedAllocator>
using TWorld = TWorldImpl<TArchetypes, buffer_size, TAllocator>;

}  // namespace NCecs
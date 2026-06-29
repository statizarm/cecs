#pragma once

#include <tuple>
#include <type_traits>
#include <variant>

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
class TVariantEntity {
  public:
    template <COneOf<TEntities...> T>
    TVariantEntity(T entity)
        : storage_(std::forward<T>(entity)) {
    }

  public:
    template <typename T>
    bool has() const {
        return std::visit(
            [](const auto& e) -> bool { return e.template has<T>(); }, storage_
        );
    }

    template <typename T>
    T& get() {
        return std::visit(
            TOverloaded{[]<COneOf<TEntities...> TE>(TE& e) -> T& {
                if constexpr (TE::TArchetype::template has<T>::value) {
                    return e.template get<T>();
                } else {
                    // Huff.
                    // Normaly code below never be executed
                    return *reinterpret_cast<T*>(~0ul);
                }
            }},
            storage_
        );
    }

    template <typename T>
    const T& get() const {
        return std::visit(
            TOverloaded{[]<COneOf<TEntities...> TE>(const TE& e) -> const T& {
                if constexpr (e.template has<T>()) {
                    return e.template get<T>();
                } else {
                    // Huff.
                    // Normaly code below never be executed
                    return *reinterpret_cast<T*>(~0ul);
                }
            }},
            storage_
        );
    }

    bool valid() {
        return std::visit(
            [](const auto& e) -> bool { return e.valid(); }, storage_
        );
    }

  private:
    std::variant<TEntities...> storage_;
};

template <
    CIsInstanceOf<TTypeList> TAll, CIsInstanceOf<TTypeList> TKnown,
    std::size_t buffer_size, typename TAllocator>
class TWorldImpl;

template <
    std::size_t buffer_size, typename TAllocator,
    CIsInstanceOf<TTypeList>... TAll, CIsInstanceOf<TTypeList>... TKnown>
    requires(CSOAAplicable<buffer_size, TAllocator, TAll> && ...)
class TWorldImpl<
    TTypeList<TAll...>, TTypeList<TKnown...>, buffer_size, TAllocator> {
  private:
    template <
        CIsInstanceOf<TTypeList>, CIsInstanceOf<TTypeList>, std::size_t,
        typename>
    friend class TWorldImpl;

  public:
    using TAllArchetypes   = TTypeList<TAll...>;
    using TKnownArchetypes = TTypeList<TKnown...>;
    using TThis =
        TWorldImpl<TAllArchetypes, TKnownArchetypes, buffer_size, TAllocator>;

  public:
    TWorldImpl()
        : containers_(), snapshots_() {
        update_snapshots();
    }

    auto& get_whole_world() {
        using TWholeWorld =
            TWorldImpl<TAllArchetypes, TAllArchetypes, buffer_size, TAllocator>;

        if constexpr (TAllArchetypes::template eq<TKnownArchetypes>::value) {
            return *this;
        } else {
            static_assert(alignof(TThis) == alignof(TWholeWorld));
            static_assert(
                offsetof(TThis, containers_) ==
                offsetof(TWholeWorld, containers_)
            );
            static_assert(
                offsetof(TThis, snapshots_) == offsetof(TWholeWorld, snapshots_)
            );

            return *reinterpret_cast<TWholeWorld*>(this);
        }
    }

    template <typename... TT>
        requires(TAny<
                 TTypeList<std::decay_t<TT>...>::template eq,
                 TAllArchetypes>::value)
    auto create() {
        using TContainerType =
            TMatchedContainer<TTypeList<std::decay_t<TT>...>>;

        auto& container = std::get<TContainerType>(containers_);
        return TEntity<typename TContainerType::ref, TThis>(
            container.create(), this
        );
    }

    template <typename... TT>
        requires(TAny<
                 TTypeList<std::decay_t<TT>...>::template eq,
                 TAllArchetypes>::value)
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
        ((std::get<TContainer<TAll>>(containers_).commit()), ...);
        update_snapshots();
    }

    template <typename... TT>
        requires(
            TAny<TTypeList<TT...>::template is_sub, TKnownArchetypes>::value
        )
    auto& select() {
        using TSelected = typename TFilter<
            TTypeList<TT...>::template is_sub,
            TKnownArchetypes>::type;

        using TSubWorld =
            TWorldImpl<TAllArchetypes, TSelected, buffer_size, TAllocator>;

        if constexpr (std::is_same_v<TThis, TSubWorld>) {
            return *this;
        } else {
            static_assert(alignof(TThis) == alignof(TSubWorld));
            static_assert(
                offsetof(TThis, containers_) == offsetof(TSubWorld, containers_)
            );
            static_assert(
                offsetof(TThis, snapshots_) == offsetof(TSubWorld, snapshots_)
            );

            return *reinterpret_cast<TSubWorld*>(this);
        }
    }

    template <typename TFunc>
    auto run(TFunc&& func) {
        return TKnownArchetypes::bind_functor([&]<typename... TArchetype>() {
            (
                [&](TContainerSnapshot<TArchetype>& snapshot) {
                    for (auto it = snapshot.first; it != snapshot.second;
                         ++it) {
                        if (!it.erased()) {
                            std::forward<TFunc>(func)(TEntity(*it, this));
                        }
                    }
                }(std::get<TContainerSnapshot<TArchetype>>(snapshots_)),
                ...);
        })();
    }

    template <typename TFunc>
    auto run_batched(TFunc&& func) {
    }

  private:
    inline void update_snapshots() {
        ((std::get<TContainerSnapshot<TAll>>(snapshots_) =
              TContainerSnapshot<TAll>(
                  std::get<TContainer<TAll>>(containers_).begin(),
                  std::get<TContainer<TAll>>(containers_).end()
              )),
         ...);
    }

  private:
    template <COneOf<TAll...> TT>
    using TContainer = TSOAContainer<buffer_size, TT, TAllocator>;

    template <COneOf<TAll...> TT>
    using TContainerSnapshot = std::pair<
        typename TContainer<TT>::iterator, typename TContainer<TT>::iterator>;

    using TContainers         = std::tuple<TContainer<TAll>...>;
    using TContainersSnapshot = std::tuple<TContainerSnapshot<TAll>...>;

    template <CIsInstanceOf<TTypeList> TArchetype>
        requires(TArchetype::template eq<TAll>::value || ...)
    using TMatchedArchetypes =
        typename TFilter<TArchetype::template eq, TTypeList<TAll...>>::type;

    template <CIsInstanceOf<TTypeList> TArchetype>
        requires(TArchetype::template eq<TAll>::value || ...)
    using TMatchedArchetype =
        typename TMatchedArchetypes<TArchetype>::template head<>::type;

    template <CIsInstanceOf<TTypeList> TArchetype>
        requires(TArchetype::template eq<TAll>::value || ...)
    using TMatchedContainer = TContainer<TMatchedArchetype<TArchetype>>;

    template <CIsInstanceOf<TTypeList> TArchetype>
        requires(TArchetype::template eq<TAll>::value || ...)
    using TMatchedContainerSnapshot =
        TContainerSnapshot<TMatchedArchetype<TArchetype>>;

  private:
    TContainers containers_;
    TContainersSnapshot snapshots_;
};

template <
    CIsInstanceOf<TTypeList> TAllArchetypes, std::size_t buffer_size = 4 << 10,
    typename TAllocator = TStdAlignedAllocator>
    requires(TAllArchetypes::bind_functor([]<typename... TArch>() -> bool {
                return (CIsInstanceOf<TArch, TTypeList> && ...);
            })())
using TWorld =
    TWorldImpl<TAllArchetypes, TAllArchetypes, buffer_size, TAllocator>;

}  // namespace NCecs
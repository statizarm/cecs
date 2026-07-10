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
template <typename TW>
class TWorldIterator;

template <typename TRef, typename TWorld>
class TEntity {
    friend TWorld;
    template <typename>
    friend class TWorldIterator;

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

    void destroy() {
        world_->destroy(*this);
    }

    bool operator==(const TThis& other) const {
        return world_ == other.world_ && ref_ == other.ref_;
    }

    bool operator!=(const TThis& other) const {
        return !(*this == other);
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
    requires(sizeof...(TEntities) > 0)
class TVariantEntity {
    using TThis = TVariantEntity<TEntities...>;

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
            []<COneOf<TEntities...> TE>(TE& e) -> T& {
                if constexpr (TE::TArchetype::template has<T>::value) {
                    return e.template get<T>();
                } else {
                    // Huff.
                    // Normaly code below never be executed
                    return *reinterpret_cast<T*>(~0ul);
                }
            },
            storage_
        );
    }

    template <typename T>
    const T& get() const {
        return std::visit(
            []<COneOf<TEntities...> TE>(const TE& e) -> const T& {
                if constexpr (e.template has<T>()) {
                    return e.template get<T>();
                } else {
                    // Huff.
                    // Normaly code below never be executed
                    return *reinterpret_cast<T*>(~0ul);
                }
            },
            storage_
        );
    }

    template <typename T, typename... TArgs>
        requires(std::is_constructible_v<T, TArgs...>)
    auto add(TArgs&&... args) {
        using TResultList    = TUnique<std::invoke_result_t<
            decltype(&TEntities::template add<T, TArgs...>),
            TEntities,
            TArgs...>...>::type;
        using TResultVariant = TResultList::template bind_type<TVariantEntity>;

        return std::visit(
            [&](auto& e) -> TResultVariant {
                return TResultVariant{
                    e.template add<T>(std::forward<TArgs>(args)...)
                };
            },
            storage_
        );
    }

    template <typename T>
    auto del() {
        using TResultList    = TUnique<std::invoke_result_t<
            decltype(&TEntities::template del<T>),
            TEntities>...>::type;
        using TResultVariant = TResultList::template bind_type<TVariantEntity>;

        return std::visit(
            [&](auto& e) -> TResultVariant {
                return TResultVariant{e.template del<T>()};
            },
            storage_
        );
    }

    void destroy() {
        std::visit([](auto& e) -> void { e.destroy(); });
    }

    bool valid() {
        return std::visit(
            [](const auto& e) -> bool { return e.valid(); }, storage_
        );
    }

    bool operator==(const TThis& other) const {
        return storage_ == other.storage_;
    }

    bool operator!=(const TThis& other) const {
        return !(*this == other);
    }

  private:
    std::variant<TEntities...> storage_;
};

template <
    CIsInstanceOf<TTypeList> TAll, CIsInstanceOf<TTypeList> TKnown,
    std::size_t buffer_size, typename TAllocator>
class TWorldImpl;

template <typename TW>
class TWorldIterator {
    friend TW;

  private:
    template <typename T>
        requires(TW::TKnownArchetypes::template has<T>::value)
    using TRefType = typename TW::template TContainer<T>::ref;
    template <typename T>
        requires(TW::TKnownArchetypes::template has<T>::value)
    using TEntityType   = TEntity<TRefType<T>, TW>;
    using TEntitiesList = TW::TKnownArchetypes::template map<TEntityType>::type;

    template <typename T>
        requires(TW::TKnownArchetypes::template has<T>::value)
    using TContainerIteratorType = TW::template TContainer<T>::iterator;
    using TContainerIteratorsList =
        TW::TKnownArchetypes::template map<TContainerIteratorType>::type;

  public:
    using TThis = TWorldIterator<TW>;
    using TCurrentIterator =
        TContainerIteratorsList::template bind_type<std::variant>;
    using TValue = TEntitiesList::template bind_type<TVariantEntity>;

    using iterator_category = std::forward_iterator_tag;
    using value_type        = TValue;
    using difference_type   = std::ptrdiff_t;

  public:
    TWorldIterator& operator++() {
        std::visit(
            [this](auto& it) { it_ = increment_and_skip_invalid(it); }, it_
        );
        return *this;
    }
    TWorldIterator operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    bool operator==(const TThis& other) const {
        return it_ == other.it_ && world_ == other.world_;
    }

    bool operator!=(const TThis& other) const {
        return !(*this == other);
    }

    TValue operator*() {
        return std::visit(
            [this](auto& it) {
                const auto& ref = *it;
                return TValue{
                    TEntity<std::decay_t<decltype(ref)>, TW>(ref, world_)
                };
            },
            it_
        );
    }

    TValue operator*() const {
        return std::visit(
            [this](const auto& it) {
                const auto& ref = *it;
                return TValue{
                    TEntity<std::decay_t<decltype(ref)>, TW>(ref, world_)
                };
            },
            it_
        );
    }

  private:
    TWorldIterator(TW* world)
        : it_(), world_(world) {
    }

    template <typename TI>
        requires(TContainerIteratorsList::template has<std::decay_t<TI>>::value)
    TWorldIterator(TW* world, TI&& it)
        : it_(), world_(world) {
        it_ = skip_invalid(it);
    }

    template <typename TI>
        requires(TContainerIteratorsList::template has<std::decay_t<TI>>::value)
    TCurrentIterator increment_and_skip_invalid(TI& it) {
        ++it;
        return skip_invalid(it);
    }

    template <typename TI>
        requires(TContainerIteratorsList::template has<std::decay_t<TI>>::value)
    TCurrentIterator skip_invalid(TI it) {
        auto end = get_end<TI>();
        while (it.erased() && it != end) {
            ++it;
        }
        if (it == end) {
            if constexpr (has_next<TI>()) {
                return skip_invalid(get_next<TI>());
            } else {
                return TCurrentIterator();
            }
        }
        return it;
    }

    template <typename TI>
        requires(TContainerIteratorsList::template has<std::decay_t<TI>>::value)
    static constexpr bool has_next() {
        return TContainerIteratorsList::template after<TI>::type::size > 0;
    }

    template <typename TI>
        requires(TContainerIteratorsList::template has<std::decay_t<TI>>::value)
    auto get_next() {
        using TTail = TContainerIteratorsList::template after<TI>::type;
        using TNextContainerIterator = typename TTail::template head<>::type;
        using TNextContainer = typename TNextContainerIterator::TContainer;
        using TNextContainerSnapshot =
            std::pair<TNextContainerIterator, TNextContainerIterator>;

        return std::get<TNextContainerSnapshot>(world_->snapshots_).first;
    }

    template <typename TI>
        requires(TContainerIteratorsList::template has<TI>::value)
    auto get_end() {
        using TContainerSnapshot = std::pair<TI, TI>;
        return std::get<TContainerSnapshot>(world_->snapshots_).second;
    }

  private:
    TCurrentIterator it_;
    TW* world_;
};

template <
    std::size_t buffer_size, typename TAllocator,
    CIsInstanceOf<TTypeList>... TAll, CIsInstanceOf<TTypeList>... TKnown>
    requires(CSOAAplicable<buffer_size, TAllocator, TAll> && ...)
class TWorldImpl<
    TTypeList<TAll...>, TTypeList<TKnown...>, buffer_size, TAllocator> {
  public:
    using TAllArchetypes   = TTypeList<TAll...>;
    using TKnownArchetypes = TTypeList<TKnown...>;
    using TThis =
        TWorldImpl<TAllArchetypes, TKnownArchetypes, buffer_size, TAllocator>;
    using TIterator = TWorldIterator<TThis>;

  private:
    template <
        CIsInstanceOf<TTypeList>, CIsInstanceOf<TTypeList>, std::size_t,
        typename>
    friend class TWorldImpl;
    friend TIterator;

  public:
    TWorldImpl()
        : containers_(), snapshots_() {
        update_snapshots();
    }

    TIterator begin() {
        return TIterator(
            this,
            std::get<TContainerSnapshot<
                typename TKnownArchetypes::template head<>::type>>(snapshots_)
                .first
        );
    }
    TIterator end() {
        return TIterator(this);
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

    bool commit() {
        // Really dumb shit. Can i replace this guard with consistency
        // snapshots. Or may be different transaction interface?
        if (iterating_) {
            return false;
        }

        ((std::get<TContainer<TAll>>(containers_).commit()), ...);
        update_snapshots();
        return true;
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
            iterating_ =
                true;  // replace with Lock, snapshots, DB like consistency?
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
            iterating_ =
                false;  // replace with Lock, snapshots, DB like consistency?
        })();
    }

    template <typename TFunc>
    auto run_batched(TFunc&& func) {
        iterating_      = true;
        const auto& res = std::forward<TFunc>(func)(*this);
        iterating_      = false;
        return res;
    }

    void clear() {
        run([](auto entity) { entity.destroy(); });
        commit();
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

    using TContainersList = TTypeList<TContainer<TAll>...>;
    using TContainers     = TContainersList::template bind_type<std::tuple>;

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
    bool iterating_ = false;
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
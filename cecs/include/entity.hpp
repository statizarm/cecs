#include <type_traits>
#include <variant>

#include "type_list.hpp"
#include "utils.hpp"

namespace NCecs {

using TEntityID                          = void*;
static constexpr TEntityID kNullEntityID = nullptr;

template <typename TRef, typename TWorld>
class TEntity;

template <typename TWorld>
class TEntityIDComponent {
    template <
        CIsInstanceOf<TTypeList> TAll, CIsInstanceOf<TTypeList> TKnown,
        std::size_t buffer_size, typename TAllocator>
    friend class TWorldImpl;

    friend TWorld;

    template <typename, typename>
    friend class TEntity;

  public:
    using TThis = TEntityIDComponent<TWorld>;

  public:
    TEntityIDComponent()
        : TEntityIDComponent(kNullEntityID, nullptr, nullptr) {
    }

    TEntityIDComponent(const TThis&) = delete;
    TEntityIDComponent(TThis&& other) {
        *this = std::move(other);
    }

    TThis& operator=(const TThis&) = delete;
    TThis& operator=(TThis&& other) {
        std::swap(entity_id_, other.entity_id_);

        relink();
        other.destroy();
        return *this;
    }

    ~TEntityIDComponent() {
        destroy();
    }

  private:
    TEntityIDComponent(TEntityID entity_id, TWorld* world, void* container)
        : entity_id_(entity_id), world_(world), container_(container) {
    }

    TEntityIDComponent(TWorld* world, void* container)
        : TEntityIDComponent(kNullEntityID, world, container) {
    }

  private:
    void relink() {
        if (entity_id_ != kNullEntityID) {
            world_->relink_entity(entity_id_, container_, this);
        }
    }

    void destroy() {
        if (entity_id_ != kNullEntityID) {
            world_->unlink_entity(entity_id_);
            entity_id_ = kNullEntityID;
        }
    }

  private:
    TEntityID entity_id_;
    TWorld* world_;
    void* container_;
};

template <typename TRef, typename TWorld>
class TEntity {
    template <
        CIsInstanceOf<TTypeList> TAll, CIsInstanceOf<TTypeList> TKnown,
        std::size_t buffer_size, typename TAllocator>
    friend class TWorldImpl;

    friend TWorld;
    template <typename>
    friend class TWorldIterator;

  public:
    using TThis        = TEntity<TRef, TWorld>;
    using TArchetype   = typename TRef::TAvailableTypes;
    using TIDComponent = TEntityIDComponent<TWorld>;

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

    TEntityID id() const
        requires(TArchetype::template has<TIDComponent>::value)
    {
        return ref_.template get<TIDComponent>().entity_id_;
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

    TEntityID id() const {
        return std::visit(
            [](const auto& e) -> TEntityID { return e.id(); }, storage_
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
        std::visit([](auto& e) -> void { e.destroy(); }, storage_);
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

}  // namespace NCecs
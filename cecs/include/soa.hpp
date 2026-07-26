#pragma once

#include <bit>
#include <bitset>
#include <iterator>
#include <type_traits>

#include "allocator.hpp"
#include "buffer_layout.hpp"
#include "gmock/gmock.h"
#include "type_list.hpp"
#include "utils.hpp"

namespace NCecs {

template <CLayout T, typename TSOAContainer>
class TSOAElementRef;

template <CLayout T>
struct alignas(T::kAlign) TSOA : public TBuffer<T> {
    static_assert(sizeof(TBuffer<T>) <= T::kBufferSize);

    using TBuffer<T>::get;
};

template <
    std::size_t buffer_size, CIsInstanceOf<TTypeList> T,
    CAlignedAllocator<std::bit_ceil(buffer_size)> TAllocator =
        TStdAlignedAllocator>
class TSOAContainer {
  private:
    static constexpr std::size_t kRoundedSize = std::bit_ceil(buffer_size);

  private:
    using max_buffer_layout = TBufferLayout<kRoundedSize, T>;

  private:
    struct TSOAChunk;

    struct THeader {
        std::bitset<max_buffer_layout::kMaxElements> erased{};
        TSOAChunk* next;
        TSOAChunk* prev;
    };

  public:
    static constexpr std::size_t kHeaderSize =
        ((sizeof(THeader) - 1) / max_buffer_layout::kAlign + 1) *
        max_buffer_layout::kAlign;
    static constexpr std::size_t kBufferWithHeaderSize = kRoundedSize;
    static constexpr std::size_t kBufferSize =
        kBufferWithHeaderSize - kHeaderSize;

  public:
    class TIterator;

  public:
    using TThis           = TSOAContainer<buffer_size, T, TAllocator>;
    using buffer_layout   = TBufferLayout<kBufferSize, T>;
    using ref             = TSOAElementRef<buffer_layout, TThis>;
    using iterator        = TIterator;
    using TAvailableTypes = typename buffer_layout::types;

  private:
    struct TSOAChunk : public THeader {
        TSOA<buffer_layout> data;
    };

  public:
    static constexpr TSOAElementRef<buffer_layout, TThis> kInvalidRef{
        nullptr,
        static_cast<std::size_t>(-1),
    };

  public:
    class TIterator {
        friend TThis;

      private:
        TIterator(THeader* header, std::size_t pos = 0)
            : header_(header), position_(pos) {
        }

      public:
        using TContainer = TThis;

        using difference_type   = std::ptrdiff_t;
        using value_type        = ref;
        using iterator_category = std::bidirectional_iterator_tag;

      public:
        TIterator()
            : header_(nullptr), position_(static_cast<std::size_t>(-1)) {
        }

      public:
        ref operator*() {
            return get_ref_from_header(
                header_, position_ % buffer_layout::kMaxElements
            );
        }

        TIterator& operator++() {
            ++position_;
            if (position_ % buffer_layout::kMaxElements == 0) {
                header_ = header_->next;
            }
            return *this;
        }

        TIterator operator++(int) {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        TIterator& operator--() {
            if (position_ % buffer_layout::kMaxElements == 0) {
                header_ = header_->prev;
            }
            --position_;
            return *this;
        }

        TIterator operator--(int) {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const TIterator& other) const {
            return other.position_ == position_;
        }

        bool operator!=(const TIterator& other) const {
            return !(*this == other);
        }

        bool operator<(const TIterator& other) const {
            return position_ < other.position_;
        }

        bool operator>(const TIterator& other) const {
            return position_ > other.position_;
        }

        bool erased() const {
            return header_->erased.test(
                position_ % buffer_layout::kMaxElements
            );
        }

        void set_erased(bool value) const {
            header_->erased.set(position_ % buffer_layout::kMaxElements, value);
        }

      private:
        THeader* header_      = nullptr;
        std::size_t position_ = 0;
    };

  public:
    TSOAContainer() {
        buffer_.next = static_cast<TSOAChunk*>(&buffer_);
        buffer_.prev = static_cast<TSOAChunk*>(&buffer_);
        first_free_  = &buffer_;
    }

    TSOAContainer(const TThis& other) = delete;
    TSOAContainer(TThis&& other) {
        *this = std::move(other);
    }

    TThis& operator=(const TThis& other) = delete;
    TThis& operator=(TThis&& other) {
        std::swap(buffer_, other.buffer_);
        std::swap(size_, other.size_);

        buffer_.next->prev = &buffer_;
        buffer_.prev->next = &buffer_;

        other.buffer_.next->prev = &other.buffer_;
        other.buffer_.prev->next = &other.buffer_;
        return *this;
    }

    ~TSOAContainer() {
        for (auto elem : *this) {
            elem.deinit(TAvailableTypes{});
        }
        for (auto* header = buffer_.next; header != &buffer_;) {
            auto* next = header->next;
            TAllocator::free(header);
            header = next;
        }
    }

  public:
    ref create() {
        THeader* header = get_or_create_free_header();

        std::size_t free_index = size_ % buffer_layout::kMaxElements;
        header->erased.reset(free_index);

        auto res = get_ref_from_header(header, free_index);

        res.init(TAvailableTypes{});

        ++size_;
        if (size_ % buffer_layout::kMaxElements == 0) {
            first_free_ = first_free_->next;
        }

        return res;
    }

    void destroy(ref& elem) {
        if (elem != kInvalidRef) {
            THeader* header = find_header_by_ptr(elem.ptr_);
            header->erased.set(elem.position_);
            elem = kInvalidRef;
        }
    }

    void destroy(ref&& elem) {
        if (elem != kInvalidRef) {
            THeader* header = find_header_by_ptr(elem.ptr_);
            header->erased.set(elem.position_);
        }
    }

    void commit() {
        auto first_bad = begin();
        auto last_good = end();
        for (; first_bad != last_good;) {
            for (; first_bad != last_good && !first_bad.erased(); ++first_bad) {
            }
            for (; last_good != first_bad && (--last_good).erased();) {
            }

            erase(first_bad, last_good);
        }
        first_free_ = first_bad.header_;
        size_       = first_bad.position_;
    }

    template <typename TT>
        requires(TAvailableTypes::template has<TT>::value)
    ref get_ref_by_ptr(TT* ptr) {
        auto header = reinterpret_cast<TSOAChunk*>(find_header_by_ptr(ptr));
        std::size_t position = ptr - &header->data.template get<TT>(0);
        return ref(&header->data, position);
    }

    TIterator begin() {
        return TIterator(buffer_.next, 0);
    }

    TIterator end() {
        return TIterator(first_free_, size_);
    }

    std::size_t size() {
        return size_;
    }

  private:
    void erase(TIterator first_bad, TIterator last_good) {
        if (first_bad != last_good) {
            auto b             = buffer_layout::kMaxElements;
            auto last_good_ref = *last_good;
            auto first_bad_ref = *first_bad;
            first_bad_ref.replace_from(std::move(last_good_ref));

            first_bad.set_erased(false);
            last_good.set_erased(true);
        }
    }

    THeader* find_header_by_ptr(void* ptr) {
        std::size_t number = reinterpret_cast<std::size_t>(ptr);
        return reinterpret_cast<THeader*>(
            number & ~(kBufferWithHeaderSize - 1)
        );
    }

    THeader* get_or_create_free_header() {
        if (first_free_ == &buffer_) {
            first_free_ = create_free_header();
        }
        return first_free_;
    }

    THeader* create_free_header() {
        THeader* free      = static_cast<THeader*>(TAllocator::aligned_alloc(
            kBufferWithHeaderSize, kBufferWithHeaderSize
        ));
        free->next         = static_cast<TSOAChunk*>(&buffer_);
        free->prev         = buffer_.prev;
        free->erased       = {};
        buffer_.prev->next = static_cast<TSOAChunk*>(free);
        buffer_.prev       = static_cast<TSOAChunk*>(free);
        return free;
    }

    static inline ref get_ref_from_header(THeader* header, std::size_t idx) {
        auto chunk = reinterpret_cast<TSOAChunk*>(header);
        return ref(&chunk->data, idx);
    }

  private:
    THeader buffer_;
    THeader* first_free_;
    std::size_t size_ = 0;
};

template <CLayout TLayout, typename TContainer>
class TSOAElementRef {
    friend TContainer;

  public:
    using TThis           = TSOAElementRef<TLayout, TContainer>;
    using buffer_layout   = TLayout;
    using TAvailableTypes = typename buffer_layout::types;

  public:
    bool valid() const {
        return ptr_ != nullptr;
    }

    template <typename T>
        requires(TAvailableTypes::template has<T>::value)
    T& get() {
        return ptr_->template get<T>(position_);
    }

    template <typename T>
        requires(TAvailableTypes::template has<T>::value)
    const T& get() const {
        return ptr_->template get<T>(position_);
    }

    template <typename TOther>
        requires(
            CIsInstanceOf<std::decay_t<TOther>, TSOAElementRef> &&
            (std::decay_t<TOther>::TAvailableTypes::template is_sub<
                 TAvailableTypes>::value ||
             TAvailableTypes::template is_sub<
                 typename std::decay_t<TOther>::TAvailableTypes>::value) &&
            (TTypeListTraits<typename std::decay_t<TOther>::TAvailableTypes>::
                 is_copy_assignable_constructible::value ||
             TTypeListTraits<typename std::decay_t<TOther>::TAvailableTypes>::
                 is_move_assignable_constructible::value)
        )
    void replace_from(TOther&& other) {
        using TReplaceTypes = std::conditional_t<
            std::decay_t<TOther>::TAvailableTypes::template is_sub<
                TAvailableTypes>::value,
            typename std::decay_t<TOther>::TAvailableTypes,
            TAvailableTypes>;

        TReplaceTypes::bind_functor([]<typename... T>(TThis* lhs, auto&& rhs) {
            (
                [](TThis* lhs, auto&& rhs) {
                    if constexpr (
                        std::is_rvalue_reference_v<decltype(rhs)> &&
                        (std::is_move_constructible_v<T> ||
                         std::is_move_assignable_v<T>)
                    ) {
                        if constexpr (std::is_move_assignable_v<T>) {
                            lhs->get<T>() = std::move(rhs.template get<T>());
                        } else {
                            new (&lhs->get<T>())
                                T(std::move(rhs.template get<T>()));
                        }
                    } else {
                        if constexpr (std::is_copy_assignable_v<T>) {
                            lhs->get<T>() = rhs.template get<T>();
                        } else if constexpr (std::is_copy_constructible_v<T>) {
                            new (&lhs->get<T>()) T(rhs.template get<T>());
                        }
                    }
                }(lhs, std::forward<TOther>(rhs)),
                ...);
        })(this, std::forward<TOther>(other));
    }

    bool operator==(const TThis& other) const {
        return ptr_ == other.ptr_ && position_ == other.position_;
    }

    bool operator!=(const TThis& other) const {
        return !(*this == other);
    }

  private:
    constexpr TSOAElementRef(TSOA<TLayout>* ptr, std::size_t position)
        : ptr_(ptr), position_(position) {
    }

  private:
    template <typename... T>
        requires(TTypeList<T...>::template is_sub<TAvailableTypes>::value)
    void init(TTypeList<T...>) {
        (sub_init<T>(), ...);
    }

    template <typename... T>
        requires(TTypeList<T...>::template is_sub<TAvailableTypes>::value)
    void deinit(TTypeList<T...>) {
        (get<T>().~T(), ...);
    }

    template <typename T>
        requires(TAvailableTypes::template has<T>::value)
    void sub_init() {
        // FIXME
        if constexpr (requires() { T(); }) {
            new (&get<T>()) T();
        }
    }

  private:
    TSOA<TLayout>* ptr_;
    std::size_t position_;
};

}  // namespace NCecs
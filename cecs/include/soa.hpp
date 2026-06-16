#pragma once

#include <bit>
#include <bitset>
#include <iterator>

#include "allocator.hpp"
#include "buffer_layout.hpp"
#include "type_list.hpp"
#include "utils.hpp"

namespace NCecs {

template <CLayout T, typename TSOAContainer>
class TSOAElementRef;

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
    struct THeader {
        std::bitset<max_buffer_layout::kMaxElements> erased{};
        THeader* next;
        THeader* prev;
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
    using TThis         = TSOAContainer<buffer_size, T, TAllocator>;
    using buffer_layout = TBufferLayout<kBufferSize, T>;
    using ref           = TSOAElementRef<buffer_layout, TThis>;
    using iterator      = TIterator;

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
        buffer_.next = &buffer_;
        buffer_.prev = &buffer_;
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
            elem.deinit(typename buffer_layout::types{});
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

        res.init(typename buffer_layout::types{});

        ++size_;
        if (size_ % buffer_layout::kMaxElements == 0) {
            first_free_ = first_free_->next;
        }

        return res;
    }

    void destroy(ref& elem) {
        THeader* header = find_header_by_ptr(elem.ptr_);
        header->erased.set(elem.position_);
        elem = kInvalidRef;
    }

    void commit() {
        if (!size()) {
            return;
        }

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
            if constexpr (
                TTypeListTraits<
                    typename buffer_layout::types>::is_move_assignable::value
            ) {
                (*first_bad).move_from(*last_good);
                (*last_good).deinit(typename buffer_layout::types{});
            } else if constexpr (
                TTypeListTraits<
                    typename buffer_layout::types>::is_copy_assignable::value
            ) {
                (*first_bad).copy_from(*last_good);
                (*last_good).deinit(typename buffer_layout::types{});
            } else {
                static_assert(
                    false, "Types must be move assignable or copy assignable"
                );
            }
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
        free->next         = &buffer_;
        free->prev         = buffer_.prev;
        free->erased       = {};
        buffer_.prev->next = free;
        buffer_.prev       = free;
        return free;
    }

    static void* calc_buffer_start(THeader* header) {
        return reinterpret_cast<void*>(
            reinterpret_cast<std::byte*>(header) + kHeaderSize +
            buffer_layout::template offset<
                typename buffer_layout::TFirst>::value
        );
    }

    static inline ref get_ref_from_header(THeader* header, std::size_t idx) {
        auto buffer =
            static_cast<buffer_layout::TFirst*>(calc_buffer_start(header));
        return ref(static_cast<void*>(buffer), idx);
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
        static constexpr std::size_t offset =
            buffer_layout::template offset<T>::value;
        return *reinterpret_cast<T*>(
            static_cast<std::byte*>(ptr_) + offset + position_ * sizeof(T)
        );
    }

    template <typename T>
        requires(TAvailableTypes::template has<T>::value)
    const T& get() const {
        static constexpr std::size_t offset =
            buffer_layout::template offset<T>::value;
        return *reinterpret_cast<T*>(
            static_cast<std::byte*>(ptr_) + offset + position_ * sizeof(T)
        );
    }

    template <CIsInstanceOf<TSOAElementRef> TOther>
        requires(
            (TOther::TAvailableTypes::template is_sub<TAvailableTypes>::value ||
             TAvailableTypes::template is_sub<
                 typename TOther::TAvailableTypes>::value) &&
            (TTypeListTraits<
                 typename TOther::TAvailableTypes>::is_move_assignable::value ||
             TTypeListTraits<
                 typename TOther::TAvailableTypes>::is_copy_assignable::value)
        )
    void replace_from(TOther& other) {
        using TReplaceTypes = std::conditional_t<
            TOther::TAvailableTypes::template is_sub<TAvailableTypes>::value,
            typename TOther::TAvailableTypes,
            TAvailableTypes>;

        if constexpr (
            TTypeListTraits<
                typename TOther::TAvailableTypes>::is_move_assignable::value
        ) {
            if constexpr (std::same_as<TOther, TThis>) {
                move_from(other);
            } else {
                move_from_impl(other, TReplaceTypes{});
            }
        } else if constexpr (
            TTypeListTraits<
                typename TOther::TAvailableTypes>::is_copy_assignable::value
        ) {
            if constexpr (std::same_as<TOther, TThis>) {
                copy_from(other);
            } else {
                copy_from_impl(other, TReplaceTypes{});
            }
        }
    }

    void copy_from(TThis other)
        requires(TTypeListTraits<TAvailableTypes>::is_copy_assignable::value)
    {
        copy_from_impl(other, TAvailableTypes{});
    }

    void move_from(TThis other)
        requires(TTypeListTraits<TAvailableTypes>::is_move_assignable::value)
    {
        move_from_impl(other, TAvailableTypes{});
    }

  private:
    constexpr TSOAElementRef(void* ptr, std::size_t position)
        : ptr_(ptr), position_(position) {
    }

  private:
    template <typename... T>
        requires(TTypeList<T...>::template is_sub<TAvailableTypes>::value)
    void init(TTypeList<T...>) {
        (new (&get<T>()) T(), ...);
    }

    template <typename... T>
        requires(TTypeList<T...>::template is_sub<TAvailableTypes>::value)
    void deinit(TTypeList<T...>) {
        (get<T>().~T(), ...);
    }

    template <CIsInstanceOf<TSOAElementRef> TOther, typename... T>
        requires(TTypeList<T...>::template is_sub<TAvailableTypes>::value)
    void copy_from_impl(TOther other, TTypeList<T...>) {
        ((get<T>() = other.template get<T>()), ...);
    }

    template <CIsInstanceOf<TSOAElementRef> TOther, typename... T>
        requires(TTypeList<T...>::template is_sub<TAvailableTypes>::value)
    void move_from_impl(TOther other, TTypeList<T...>) {
        ((get<T>() = std::move(other.template get<T>())), ...);
    }

  private:
    void* ptr_;
    std::size_t position_;
};

}  // namespace NCecs
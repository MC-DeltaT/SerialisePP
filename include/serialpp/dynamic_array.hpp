#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <new>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "buffers.hpp"
#include "common.hpp"
#include "scalar.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        dynamic_array:
            Fixed data is an element count (dynamic_array_size_t) and an offset (data_offset_t).
            If the element count is > 0, then that many elements are contained starting at the offset.
            If the element count is 0, then the offset is not significant and no variable data is present.
    */


    using dynamic_array_size_t = std::uint32_t;

    inline constexpr std::size_t max_dynamic_array_size = std::numeric_limits<dynamic_array_size_t>::max();


    namespace detail {

        // Safely casts to dynamic_array_size_t. Throws object_size_error if the value is too big to be stored in a
        // dynamic_array_size_t.
        [[nodiscard]]
        constexpr dynamic_array_size_t to_dynamic_array_size(std::size_t const size) {
            if (std::cmp_less_equal(size, max_dynamic_array_size)) {
                return static_cast<dynamic_array_size_t>(size);
            }
            else {
                throw object_size_error{
                    std::format("dynamic_array with {} elements is too big to be serialised", size)};
            }
        }

    }


    // Serialisable variable-length homogeneous array. Can hold up to max_dynamic_array_size elements.
    template<serialisable T>
    struct dynamic_array {
        using element_type = T;
    };


    template<serialisable T>
    struct fixed_data_size<dynamic_array<T>>
        : detail::size_t_constant<fixed_data_size_v<dynamic_array_size_t> + fixed_data_size_v<data_offset_t>> {};


    template<typename R, typename T>
    concept dynamic_array_serialise_source_range = std::ranges::forward_range<R> && std::ranges::sized_range<R>
        && std::convertible_to<std::ranges::range_reference_t<R>, serialise_source<T> const&>;


    template<serialisable T>
    class serialise_source<dynamic_array<T>> {
    public:
        // TODO: move assignable?

        // Constructs with zero elements.
        constexpr serialise_source() :
            _range{}
        {}

        // Constructs from the elements of a braced-init-list.
        template<std::size_t N> requires (N <= max_dynamic_array_size)
        constexpr serialise_source(serialise_source<T> (&& elements)[N]) :
            _range{}
        {
            using range_type = std::array<serialise_source<T>, N>;
            if constexpr (N > 0) {
                if (std::is_constant_evaluated()) {
                    // At compile time, always dynamically allocate.
                    [this, &elements] <std::size_t... Is> (std::index_sequence<Is...>) {
                        _range.set_constexpr_wrapper(new _alloc_range_wrapper<range_type>{std::move(elements[Is])...});
                    }(std::make_index_sequence<N>{});
                }
                else {
                    // If the array is small enough, put it in the inline buffer.
                    if constexpr (_inline_range_buffer::template can_contain<range_type>()) {
                        [this, &elements] <std::size_t... Is> (std::index_sequence<Is...>) {
                            _range.inline_buffer().emplace([&elements](void* const data) {
                                return new(data) range_type{{std::move(elements[Is])...}};
                            });
                        }(std::make_index_sequence<N>{});
                    }
                    // Otherwise have to dynamically allocate space to type-erase the range.
                    else {
                        [this, &elements] <std::size_t... Is> (std::index_sequence<Is...>) {
                            _range.inline_buffer().emplace<_alloc_range_wrapper_base_ptr>(
                                new _alloc_range_wrapper<range_type>{std::move(elements[Is])...});
                        }(std::make_index_sequence<N>{});
                    }
                }
            }
        }

        // Constructs from the elements of a range. The range must have <= max_dynamic_array_size elements.
        // The range is stored within an instance of std::ranges::views::all.
        template<std::ranges::viewable_range R>
            requires dynamic_array_serialise_source_range<std::ranges::views::all_t<R>, T>
        constexpr serialise_source(R&& range) :
            _range{}
        {
            assert(std::ranges::size(range) <= max_dynamic_array_size);

            using range_type = std::ranges::views::all_t<R>;
            // If we're owning the elements and the range is empty, don't bother storing it.
            if (!std::ranges::empty(range) || std::ranges::borrowed_range<R>) {
                if (std::is_constant_evaluated()) {
                    // At compile time, always dynamically allocate.
                    _range.set_constexpr_wrapper(new _alloc_range_wrapper<range_type>{std::forward<R>(range)});
                }
                // If the range object is small enough, put it in the inline buffer.
                else if constexpr (_inline_range_buffer::template can_contain<range_type>()) {
                    _range.inline_buffer().emplace<range_type>(std::forward<R>(range));
                }
                // Otherwise have to dynamically allocate space to type-erase the range.
                else {
                    _range.inline_buffer().emplace<_alloc_range_wrapper_base_ptr>(
                        new _alloc_range_wrapper<range_type>{std::forward<R>(range)});
                }
            }
        }

    private:
        struct _range_visitor {
            detail::devirtualised_virtual_buffer& buffer;
            std::size_t element_count = 0;

            constexpr void operator()(dynamic_array_serialise_source_range<T> auto& range) {
                std::unsigned_integral auto const count = std::ranges::size(range);
                element_count = count;
                push_variable_subobjects<T>(count, buffer, [this, &range](std::size_t const elements_fixed_offset_) {
                    with_buffer_for<T>(buffer, [elements_fixed_offset_, &range](auto&& buffer) {
                        // Local copy - mutating a lambda capture seems to inhibit optimisation.
                        auto elements_fixed_offset = elements_fixed_offset_;
                        for (serialise_source<T> const& element : range) {
                            elements_fixed_offset = push_fixed_subobject<T>(elements_fixed_offset,
                                detail::bind_serialise(element, buffer));
                        }
                    });
                });
            }
        };

        class _alloc_range_wrapper_base {
        public:
            virtual constexpr ~_alloc_range_wrapper_base() = default;

            virtual constexpr void visit(_range_visitor& visitor) const = 0;
        };

        template<typename R> requires std::is_object_v<R>
        class _alloc_range_wrapper final : public _alloc_range_wrapper_base {
        public:
            R range;

            template<typename... Args>
            constexpr _alloc_range_wrapper(Args&&... args) :
                range{std::forward<Args>(args)...}
            {}

            constexpr void visit(_range_visitor& visitor) const final {
                visitor(range);
            }
        };

        using _alloc_range_wrapper_base_ptr = detail::constexpr_unique_ptr<_alloc_range_wrapper_base const>;

        struct _range_wrapper_visitor : _range_visitor {
            using _range_visitor::operator();

            constexpr void operator()(_alloc_range_wrapper_base_ptr const& wrapper) {
                assert(wrapper);
                wrapper->visit(*this);
            }
        };

        static constexpr std::size_t _inline_buffer_size = std::max<std::size_t>(
            sizeof(_alloc_range_wrapper_base_ptr),      // Almost certainly <= 24 bytes.
            24      // Size of std::vector on GCC, Clang, and MSVC.
        );

        static constexpr std::size_t _inline_buffer_align = std::max<std::size_t>(
            alignof(_alloc_range_wrapper_base_ptr),     // Likely 8 on 64-bit platforms.
            8       // Alignment of std::vector on 64-bit GCC, Clang, and MSVC.
        );

        // The most common range types are small, so we can often avoid allocations by using a small inline buffer.
        using _inline_range_buffer =
            detail::small_any<_inline_buffer_size, _inline_buffer_align, _range_wrapper_visitor>;

        // Now here is the real voodoo.
        // We want to allow constexpr use of a serialise_source<dynamic_array<T>>, but small_any is not constexpr,
        // because you can't reinterpret_cast pointers nor use placement new at compile time.
        // The solution is to forget the small object optimisation and always type erase via dynamic allocation when in
        // compile time.
        // Hence this class, which wraps a union of a small_any and constexpr_unique_ptr. At compile time, the
        // constexpr_unique_ptr is active. At runtime, the small_any is active. There's no memory nor runtime overhead.
        // But what if the serialise_source is constructed at compile time and used at runtime? The union will have the
        // wronge type! Well, that's impossible, because compile time dynamic allocations cannot escape compile time.
        class _range_wrapper {
        public:
            constexpr _range_wrapper() :
                _inline_buffer{}
            {
                if (std::is_constant_evaluated()) {
                    _inline_buffer.~_inline_range_buffer();

                    // Need to do dynamic allocation to prevent constexpr default constructed instance from being used
                    // at runtime (because we wouldn't know which union member is active).
                    std::construct_at(&_constexpr_wrapper,
                        new _alloc_range_wrapper<std::ranges::empty_view<serialise_source<T>>>{});
                }
            }

            constexpr _range_wrapper(_range_wrapper&& other) :
                _inline_buffer{std::move(other._inline_buffer)}
            {
                if (std::is_constant_evaluated()) {
                    _inline_buffer.~_inline_range_buffer();
                    new(&_constexpr_wrapper) auto(std::move(other._constexpr_wrapper));
                }
            }

            _range_wrapper(_range_wrapper const&) = delete;

            constexpr ~_range_wrapper() {
                if (std::is_constant_evaluated()) {
                    _constexpr_wrapper.~_alloc_range_wrapper_base_ptr();
                }
                else {
                    _inline_buffer.~_inline_range_buffer();
                }
            }

            _range_wrapper& operator=(_range_wrapper&&) = delete;
            _range_wrapper& operator=(_range_wrapper const&) = delete;

            constexpr _inline_range_buffer const& inline_buffer() const {
                assert(!std::is_constant_evaluated());
                return _inline_buffer;
            }

            constexpr _inline_range_buffer& inline_buffer() {
                assert(!std::is_constant_evaluated());
                return _inline_buffer;
            }

            constexpr _alloc_range_wrapper_base const& constexpr_wrapper() const {
                assert(std::is_constant_evaluated());
                return *_constexpr_wrapper;
            }

            constexpr void set_constexpr_wrapper(_alloc_range_wrapper_base const* const ptr) {
                assert(std::is_constant_evaluated());
                _constexpr_wrapper.reset(ptr);
            }

        private:
            union {
                // Active if constructed at runtime.
                _inline_range_buffer _inline_buffer;
                // Active if constructed at compile time.
                _alloc_range_wrapper_base_ptr _constexpr_wrapper;
            };
        };

        _range_wrapper _range;

        // Returns the number of elements serialised.
        template<serialise_buffer Buffer>
        constexpr std::size_t _serialise_elements(Buffer& buffer) const {
            auto const visit = [this](detail::devirtualised_virtual_buffer& buffer) {
                _range_wrapper_visitor range_visitor{buffer};
                if (std::is_constant_evaluated()) {
                    _range.constexpr_wrapper().visit(range_visitor);
                }
                else {
                    _range.inline_buffer().visit(range_visitor);
                }
                return range_visitor.element_count;
            };

            if constexpr (std::same_as<std::remove_cvref_t<Buffer>, detail::devirtualised_virtual_buffer>) {
                return visit(buffer);
            }
            else {
                detail::virtual_buffer_impl virtual_buffer{buffer};
                detail::devirtualised_virtual_buffer devirtualised_buffer{virtual_buffer};
                return visit(devirtualised_buffer);
            }
        }

        friend struct serialiser<dynamic_array<T>>;
    };

    template<serialisable T>
    struct serialiser<dynamic_array<T>> {
        static constexpr void serialise(serialise_source<dynamic_array<T>> const& source, serialise_buffer auto& buffer,
                std::size_t fixed_offset) {
            // Must check this first in case elements use variable data.
            auto const variable_offset = buffer.span().size();

            auto const element_count = source._serialise_elements(buffer);

            auto const size = detail::to_dynamic_array_size(element_count);
            fixed_offset = push_fixed_subobject<dynamic_array_size_t>(fixed_offset,
                detail::bind_serialise(serialise_source<dynamic_array_size_t>{size}, buffer));

            auto const offset = element_count > 0 ? detail::to_data_offset(variable_offset) : data_offset_t{0};
            fixed_offset = push_fixed_subobject<data_offset_t>(fixed_offset,
                detail::bind_serialise(serialise_source<data_offset_t>{offset}, buffer));
        }
    };

    template<serialisable T>
    class deserialiser<dynamic_array<T>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        // Gets the number of elements in the dynamic_array.
        [[nodiscard]]
        constexpr std::size_t size() const {
            return deserialise<dynamic_array_size_t>(_buffer, _fixed_offset);
        }

        // Checks if the dynamic_array contains zero elements.
        [[nodiscard]]
        constexpr bool empty() const {
            return size() == 0;
        }

        // Gets the element at the specified index. index must be < size().
        [[nodiscard]]
        constexpr deserialise_t<T> operator[](std::size_t const index) const {
            assert(index < size());
            auto const element_offset = _offset() + fixed_data_size_v<T> * index;
            return deserialise<T>(_buffer, element_offset);
        }

        // Gets the element at the specified index. Throws std::out_of_range if index is out of bounds.
        [[nodiscard]]
        constexpr deserialise_t<T> at(std::size_t const index) const {
            auto const size = this->size();
            if (index < size) {
                return (*this)[index];
            }
            else {
                throw std::out_of_range{
                    std::format("index {} is out of bounds for dynamic_array with size {}", index, size)};
            }
        }

        // Gets a view of the elements.
        // The returned view references this deserialiser instance, so cannot outlive it.
        [[nodiscard]]
        constexpr std::ranges::random_access_range auto elements() const {
            return std::ranges::views::iota(std::size_t{0}, size())
                | std::ranges::views::transform([this](std::size_t const index) {
                    return (*this)[index];
                });
        }

    private:
        [[nodiscard]]
        constexpr data_offset_t _offset() const {
            return deserialise<data_offset_t>(_buffer, _fixed_offset + fixed_data_size_v<dynamic_array_size_t>);
        }
    };

}

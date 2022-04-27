#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "common.hpp"
#include "scalar.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        dynamic_array:
            Fixed data is an element count (dynamic_array_size_t) and an offset (data_offset_t).
            If the element count is > 0, then that many elements are contained starting at the offset in the variable
            data section.
            If the element count is 0, then the offset is unused and no variable data is present.
    */


    using dynamic_array_size_t = std::uint32_t;

    static inline constexpr std::size_t max_dynamic_array_size = std::numeric_limits<dynamic_array_size_t>::max();


    namespace detail {

        // Safely casts to dynamic_array_size_t. Throws object_size_error if the value is too big to be stored in a
        // dynamic_array_size_t.
        [[nodiscard]]
        inline dynamic_array_size_t to_dynamic_array_size(std::size_t size) {
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
        using size_type = dynamic_array_size_t;

        static constexpr std::size_t max_size = max_dynamic_array_size;
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
        // TODO: copyable?
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
                    if constexpr (_inline_range_buffer::template storage_compatible_with<range_type>()) {
                        [this, &elements] <std::size_t... Is> (std::index_sequence<Is...>) {
                            _range.inline_buffer().emplace([&elements](void* data) {
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
            assert(std::size(range) <= max_dynamic_array_size);

            using range_type = std::ranges::views::all_t<R>;
            // If we're owning the elements and the range is empty, don't bother storing it.
            if (!std::ranges::empty(range) || std::ranges::borrowed_range<R>) {
                if (std::is_constant_evaluated()) {
                    // At compile time, always dynamically allocate.
                    _range.set_constexpr_wrapper(new _alloc_range_wrapper<range_type>{std::forward<R>(range)});
                }
                // If the range object is small enough, put it in the inline buffer.
                else if constexpr (_inline_range_buffer::template storage_compatible_with<range_type>()) {
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
        class _element_visitor_base {
        public:
            // No virtual destructor because it's not destructed polymorphically.

            // Called before iterating the range with the reported size of the range.
            virtual constexpr void initialise(std::size_t element_count) = 0;

            // Called for each element in the range.
            virtual constexpr void visit_element(serialise_source<T> const& element) = 0;
        };

        template<serialise_buffer Buffer>
        class _element_visitor final : public _element_visitor_base {
        public:
            serialise_target<Buffer> target;
            std::optional<serialise_target<Buffer>> elements_target;
            serialise_target<Buffer>::push_variable_subobjects_context push_elements_context;

            explicit constexpr _element_visitor(serialise_target<Buffer> target) noexcept :
                target{target}, elements_target{std::nullopt}, push_elements_context{}
            {}

            constexpr void initialise(std::size_t element_count) final {
                auto const [elems_target, context] = target.enter_variable_subobjects<T>(element_count);
                elements_target = elems_target;
                push_elements_context = context;
            }

            constexpr void visit_element(serialise_source<T> const& element) final {
                assert(elements_target.has_value());
                elements_target = elements_target->push_fixed_subobject<T>(
                    [&element](serialise_target<Buffer> element_target) {
                        return serialiser<T>{}(element, element_target);
                    });
            }

            constexpr serialise_target<Buffer> finalise() const {
                // If the serialise_source is default constructed, then no range gets visited.
                if (elements_target.has_value()) {
                    return elements_target->exit_variable_subobjects(push_elements_context);
                }
                else {
                    return target;
                }
            }
        };

        struct _range_visitor {
            _element_visitor_base* elem_visitor;
            std::size_t element_count = 0;

            template<dynamic_array_serialise_source_range<T> R>
            constexpr void operator()(R& range) {
                std::unsigned_integral auto const count = std::ranges::size(range);
                // Keep track of real count if for some bizarre reason range size is wrong.
                std::size_t actual_count = 0;
                elem_visitor->initialise(count);
                std::forward_iterator auto element_it = std::ranges::begin(range);
                std::sentinel_for<decltype(element_it)> auto const end_it = std::ranges::end(range);
                while (actual_count < count && element_it != end_it) {
                    elem_visitor->visit_element(*element_it);
                    ++actual_count;
                    ++element_it;
                }
                element_count = actual_count;
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

        using _alloc_range_wrapper_base_ptr = std::unique_ptr<_alloc_range_wrapper_base const>;

        struct _range_wrapper_visitor : _range_visitor {
            using _range_visitor::operator();

            constexpr void operator()(_alloc_range_wrapper_base_ptr const& wrapper) {
                assert(wrapper);
                wrapper->visit(*this);
            }
        };

        static inline constexpr std::size_t _inline_buffer_size = std::max<std::size_t>(
            sizeof(_alloc_range_wrapper_base_ptr),      // Almost certainly <= 24 bytes.
            24      // Size of std::vector on GCC, Clang, and MSVC.
        );

        static inline constexpr std::size_t _inline_buffer_align = std::max<std::size_t>(
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
        // Hence this class, which wraps a union of a small_any and unique_ptr. At compile time, the unique_ptr is
        // active. At runtime, the small_any is active. There's no memory nor runtime overhead.
        // But what if the serialise_source is constructed at compile time and used at runtime? The union will have the
        // wronge type! Well, that's impossible, because compile time dynamic allocations cannot escape compile time.
        class _range_wrapper {
        public:
            constexpr _range_wrapper() :
                _inline_buffer{}
            {
                if (std::is_constant_evaluated()) {
                    _inline_buffer.~small_any();

                    // Need to do dynamic allocation to prevent constexpr default constructed instance from being used
                    // at runtime (because we wouldn't know which union member is active).
                    _constexpr_wrapper = new _alloc_range_wrapper<std::ranges::empty_view<serialise_source<T>>>{};
                }
            }

            constexpr _range_wrapper(_range_wrapper&& other) :
                _inline_buffer{std::move(other._inline_buffer)}
            {
                if (std::is_constant_evaluated()) {
                    _inline_buffer.~small_any();
                    _constexpr_wrapper = std::exchange(other._constexpr_wrapper, nullptr);
                }
            }

            _range_wrapper(_range_wrapper const&) = delete;

            constexpr ~_range_wrapper() {
                if (std::is_constant_evaluated()) {
                    if (_constexpr_wrapper) {
                        delete _constexpr_wrapper;
                    }
                }
                else {
                    _inline_buffer.~small_any();
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

            constexpr _alloc_range_wrapper_base const* constexpr_wrapper() const {
                assert(std::is_constant_evaluated());
                return _constexpr_wrapper;
            }

            constexpr void set_constexpr_wrapper(_alloc_range_wrapper_base const* ptr) {
                assert(std::is_constant_evaluated());
                delete _constexpr_wrapper;
                _constexpr_wrapper = ptr;
            }
        
        private:
            union {
                _inline_range_buffer _inline_buffer;                    // Active if constructed at runtime.
                _alloc_range_wrapper_base const* _constexpr_wrapper;    // Active if constructed at compile time
            };
        };

        _range_wrapper _range;

        template<serialise_buffer Buffer>
        constexpr std::pair<std::size_t, serialise_target<Buffer>> _serialise_elements(
                serialise_target<Buffer> target) const {
            // To serialise the elements, we need this double visitor process, because there are two type parameters:
            // the range type, and the serialise buffer type.
            // First we visit the range, and iterate its elements. Within the range visitor, the serialise buffer type
            // cannot be known, because the range visitor is instantiated before this function is called.
            // However we do know the range element type in advance, so we can virtual call each element into another
            // visitor that knows the serialise buffer type.

            _element_visitor elem_visitor{target};
            _range_wrapper_visitor rng_visitor{&elem_visitor};
            if (std::is_constant_evaluated()) {
                _range.constexpr_wrapper()->visit(rng_visitor);
            }
            else {
                _range.inline_buffer().visit(rng_visitor);
            }
            auto const new_target = elem_visitor.finalise();
            return {rng_visitor.element_count, new_target};
        }

        friend struct serialiser<dynamic_array<T>>;
    };

    template<serialisable T>
    struct serialiser<dynamic_array<T>> {
        template<serialise_buffer Buffer>
        constexpr serialise_target<Buffer> operator()(serialise_source<dynamic_array<T>> const& source,
                serialise_target<Buffer> target) const {
            auto const relative_variable_offset = target.relative_subobject_variable_offset();

            auto const [element_count, new_target] = source._serialise_elements(target);

            return new_target.push_fixed_subobject<dynamic_array_size_t>([element_count](auto size_target) {
                return serialiser<dynamic_array_size_t>{}(detail::to_dynamic_array_size(element_count), size_target);
            }).push_fixed_subobject<data_offset_t>([relative_variable_offset](auto offset_target) {
                return serialiser<data_offset_t>{}(detail::to_data_offset(relative_variable_offset), offset_target);
            });
        }
    };

    template<serialisable T>
    class deserialiser<dynamic_array<T>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr deserialiser(const_bytes_span fixed_data, const_bytes_span variable_data) :
            deserialiser_base{no_fixed_buffer_check, fixed_data, variable_data}
        {
            check_fixed_buffer_size<dynamic_array<T>>(_fixed_data);
        }

        // Gets the number of elements in the dynamic_array.
        [[nodiscard]]
        constexpr std::size_t size() const {
            return deserialiser<dynamic_array_size_t>{no_fixed_buffer_check, _fixed_data, _variable_data}.value();
        }

        // Checks if the dynamic_array contains zero elements.
        [[nodiscard]]
        constexpr bool empty() const {
            return size() == 0;
        }

        // Gets the element at the specified index. index must be < size().
        [[nodiscard]]
        constexpr auto_deserialise_t<T> operator[](std::size_t index) const {
            assert(index < size());
            auto const base_offset = _offset();
            auto const element_offset = base_offset + fixed_data_size_v<T> * index;
            _check_variable_offset(element_offset);
            deserialiser<T> const deser{
                _variable_data.subspan(element_offset, fixed_data_size_v<T>),
                _variable_data
            };
            return auto_deserialise(deser);
        }

        // Gets the element at the specified index. Throws std::out_of_range if index is out of bounds.
        [[nodiscard]]
        constexpr auto_deserialise_t<T> at(std::size_t index) const {
            auto const size = this->size();
            if (index < size) {
                return (*this)[index];
            }
            else {
                throw std::out_of_range{
                    std::format("dynamic_array index {} is out of bounds for size {}", index, size)};
            }
        }

        // Gets a view of the elements.
        // The returned view references this deserialiser instance cannot outlive it.
        [[nodiscard]]
        constexpr std::ranges::random_access_range auto elements() const {
            return std::ranges::views::iota(std::size_t{0}, size())
                | std::ranges::views::transform([this](std::size_t index) {
                    return (*this)[index];
                });
        }

    private:
        [[nodiscard]]
        constexpr data_offset_t _offset() const {
            return deserialiser<data_offset_t>{
                no_fixed_buffer_check,
                _fixed_data.subspan(fixed_data_size_v<dynamic_array_size_t>, fixed_data_size_v<data_offset_t>),
                _variable_data
            }.value();
        }
    };

}

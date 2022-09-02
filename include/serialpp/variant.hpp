#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

#include "common.hpp"
#include "scalar.hpp"
#include "utility.hpp"


namespace serialpp {

    // TODO: optimisation for 0 and 1 types?

    /*
        variant:
            Fixed data is a type index (variant_index_t) indicating which type is contained, and an offset to the
            variable data (data_offset_t).
            Variable data is the contained value.

            If the set of possible types is empty, then the type index and offset are not significant.
    */


    using variant_index_t = std::uint16_t;

    inline constexpr std::size_t max_variant_types = std::numeric_limits<variant_index_t>::max();


    // Serialisable type that holds exactly one instance of a type from a set of possible types.
    // Can have up to max_variant_types types (including zero).
    template<serialisable... Ts> requires (sizeof...(Ts) <= max_variant_types)
    struct variant {
        using index_type = variant_index_t;

        using types = type_list<Ts...>;
    };


    template<serialisable... Ts>
    struct fixed_data_size<variant<Ts...>>
        : detail::size_t_constant<fixed_data_size_v<variant_index_t> + fixed_data_size_v<data_offset_t>> {};


    template<>
    class serialise_source<variant<>> : public std::variant<std::monostate> {
    public:
        using std::variant<std::monostate>::variant;
    };

    template<serialisable... Ts>
    class serialise_source<variant<Ts...>> : public std::variant<serialise_source<Ts>...> {
    public:
        // TODO: is having the in_place_ constructors explicit good for us?
        using std::variant<serialise_source<Ts>...>::variant;
    };


    template<serialisable... Ts>
    struct serialiser<variant<Ts...>> {
        static constexpr void serialise(serialise_source<variant<Ts...>> const& source, serialise_buffer auto& buffer,
                std::size_t fixed_offset) {
            if (source.valueless_by_exception()) {
                throw std::bad_variant_access{};
            }

            auto const source_index = source.index();
            assert(source_index <= max_variant_types);
            auto const index = static_cast<variant_index_t>(source_index);
            fixed_offset = push_fixed_subobject<variant_index_t>(fixed_offset,
                detail::bind_serialise(serialise_source<variant_index_t>{index}, buffer));

            auto const variable_offset = buffer.span().size();
            auto const offset = sizeof...(Ts) > 0 ? detail::to_data_offset(variable_offset) : data_offset_t{0};
            fixed_offset = push_fixed_subobject<data_offset_t>(fixed_offset,
                detail::bind_serialise(serialise_source<data_offset_t>{offset}, buffer));

            if constexpr (sizeof...(Ts) > 0) {
                std::visit([&buffer] <serialisable T> (serialise_source<T> const& value_source) {
                    push_variable_subobjects<T>(1, buffer, detail::bind_serialise(value_source, buffer));
                }, source);
            }
        }

        static constexpr void serialise(serialise_source<variant<Ts...>> const&, mutable_bytes_span const buffer,
                std::size_t fixed_offset) requires (sizeof...(Ts) == 0) {
            fixed_offset = push_fixed_subobject<variant_index_t>(fixed_offset,
                detail::bind_serialise(serialise_source<variant_index_t>{0}, buffer));

            fixed_offset = push_fixed_subobject<data_offset_t>(fixed_offset,
                detail::bind_serialise(serialise_source<data_offset_t>{0}, buffer));
        }
    };


    template<serialisable... Ts>
    class deserialiser<variant<Ts...>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        // Gets the zero-based index of the contained type.
        [[nodiscard]]
        constexpr std::size_t index() const requires (sizeof...(Ts) > 0) {
            return deserialise<variant_index_t>(_buffer, _fixed_offset);
        }

        // If Index == index(), gets the contained value. Otherwise, throws std::bad_variant_access.
        template<std::size_t Index> requires (Index < sizeof...(Ts))
        [[nodiscard]]
        constexpr auto get() const {
            if (Index == index()) {
                return _get<Index>();
            }
            else {
                throw std::bad_variant_access{};
            }
        }

        // Invokes the specified function with the contained value as the argument.
        template<typename F> requires (std::invocable<F, deserialise_t<Ts>> && ...)
        constexpr decltype(auto) visit(F&& visitor) const
                noexcept((std::is_nothrow_invocable_v<F, deserialise_t<Ts>> && ...)) {
            if constexpr (sizeof...(Ts) > 0) {
                return _visit<0>(std::forward<F>(visitor), index());
            }
        }

    private:
        [[nodiscard]]
        constexpr data_offset_t _offset() const requires (sizeof...(Ts) > 0) {
            return deserialise<data_offset_t>(_buffer, _fixed_offset + fixed_data_size_v<variant_index_t>);
        }

        // Gets the contained value by index.
        template<std::size_t Index> requires (Index < sizeof...(Ts))
        constexpr auto _get() const {
            using element_type = detail::type_list_element<type_list<Ts...>, Index>::type;
            return deserialise<element_type>(_buffer, _offset());
        }

        template<std::size_t I, typename F>
            requires (sizeof...(Ts) > 0) && (std::invocable<F, deserialise_t<Ts>> && ...)
        constexpr decltype(auto) _visit(F&& visitor, std::size_t const index) const
                noexcept((std::is_nothrow_invocable_v<F, deserialise_t<Ts>> && ...)) {
            assert(index < sizeof...(Ts));
            if (I != index) {
                if constexpr (I + 1 < sizeof...(Ts)) {
                    return _visit<I + 1>(std::forward<F>(visitor), index);
                }
                else {
                    // Unreachable.
                    assert(false);
                }
            }
            return std::invoke(std::forward<F>(visitor), _get<I>());
        }
    };

}

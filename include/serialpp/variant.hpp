#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
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


    using variant_index_t = std::uint8_t;

    static inline constexpr std::size_t max_variant_types = std::numeric_limits<variant_index_t>::max();


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
        template<serialise_buffer Buffer>
        constexpr serialise_target<Buffer> operator()(serialise_source<variant<Ts...>> const& source,
                serialise_target<Buffer> target) const {
            if (source.valueless_by_exception()) {
                throw std::bad_variant_access{};
            }

            auto const index = source.index();
            auto const relative_variable_offset = target.relative_subobject_variable_offset();

            target = target.push_fixed_subobject<variant_index_t>([index](auto index_target) {
                assert(index <= max_variant_types);
                return serialiser<variant_index_t>{}(static_cast<variant_index_t>(index), index_target);
            }).push_fixed_subobject<data_offset_t>([relative_variable_offset](auto offset_target) {
                return serialiser<data_offset_t>{}(detail::to_data_offset(relative_variable_offset), offset_target);
            });

            if constexpr (sizeof...(Ts) > 0) {
                target = std::visit([&target] <serialisable T> (serialise_source<T> const& value_source) {
                    return target.push_variable_subobjects<T>(1, [&value_source](auto value_target) {
                        return serialiser<T>{}(value_source, value_target);
                    });
                }, source);
            }

            return target;
        }
    };


    template<serialisable... Ts>
    class deserialiser<variant<Ts...>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr deserialiser(const_bytes_span fixed_data, const_bytes_span variable_data) :
            deserialiser_base{no_fixed_buffer_check, fixed_data, variable_data}
        {
            check_fixed_buffer_size<variant<Ts...>>(_fixed_data);
        }

        // Gets the zero-based index of the contained type.
        [[nodiscard]]
        constexpr std::size_t index() const requires (sizeof...(Ts) > 0) {
            return deserialiser<variant_index_t>{no_fixed_buffer_check, _fixed_data, _variable_data}.value();
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
        template<typename F> requires (std::invocable<F, auto_deserialise_t<Ts>> && ...)
        constexpr decltype(auto) visit(F&& visitor) const {
            if constexpr (sizeof...(Ts) > 0) {
                return _visit<0>(std::forward<F>(visitor), index());
            }
        }

    private:
        [[nodiscard]]
        constexpr data_offset_t _offset() const requires (sizeof...(Ts) > 0) {
            return deserialiser<data_offset_t>{
                no_fixed_buffer_check,
                _fixed_data.subspan(fixed_data_size_v<variant_index_t>, fixed_data_size_v<data_offset_t>),
                _variable_data
            }.value();
        }

        // Gets the contained value by index.
        template<std::size_t Index> requires (Index < sizeof...(Ts))
        constexpr auto _get() const {
            using element_type = detail::type_list_element<type_list<Ts...>, Index>::type;
            auto const offset = _offset();
            _check_variable_offset(offset);
            deserialiser<element_type> const deser{
                _variable_data.subspan(offset, fixed_data_size_v<element_type>),
                _variable_data
            };
            return auto_deserialise(deser);
        }

        template<std::size_t I, typename F>
            requires (sizeof...(Ts) > 0) && (std::invocable<F, auto_deserialise_t<Ts>> && ...)
        constexpr decltype(auto) _visit(F&& visitor, std::size_t index) const {
            assert(index < sizeof...(Ts));
            if (I == index) {
                return std::invoke(std::forward<F>(visitor), _get<I>());
            }
            else if constexpr (I + 1 < sizeof...(Ts)) {
                return _visit<I + 1>(std::forward<F>(visitor), index);
            }
            // Unreachable.
            assert(false);
        }
    };

}

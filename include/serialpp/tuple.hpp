#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        tuple:
            Represented as the result of contiguously serialising each element of the tuple.
    */


    // Serialisable heterogeneous collection of any number of types.
    template<serialisable... Ts>
    struct tuple {
        using element_types = type_list<Ts...>;
    };


    template<serialisable... Ts>
    struct fixed_data_size<tuple<Ts...>> : detail::size_t_constant<(fixed_data_size_v<Ts> + ...)> {};

    template<>
    struct fixed_data_size<tuple<>> : detail::size_t_constant<0> {};    // Can't fold empty pack over +


    template<serialisable... Ts>
    class serialise_source<tuple<Ts...>> : public std::tuple<serialise_source<Ts>...> {
    public:
        using std::tuple<serialise_source<Ts>...>::tuple;

        // For some reason std::tuple doesn't have this constructor.
        constexpr serialise_source(serialise_source<Ts>&&... elements) :
            std::tuple<serialise_source<Ts>...>{std::move(elements)...}
        {}
    };


    template<serialisable... Ts>
    struct serialiser<tuple<Ts...>> {
        template<serialise_buffer Buffer>
        constexpr serialise_target<Buffer> operator()(serialise_source<tuple<Ts...>> const& source,
                serialise_target<Buffer> target) const {
            if constexpr (sizeof...(Ts) > 0) {
                return _serialise<0>(source, target);
            }
            else {
                return target;
            }
        }

    private:
        template<std::size_t Index, serialise_buffer Buffer> requires (Index < sizeof...(Ts))
        [[nodiscard]]
        static constexpr serialise_target<Buffer> _serialise(serialise_source<tuple<Ts...>> const& source,
                serialise_target<Buffer> target) {
            using element_type = detail::type_list_element<type_list<Ts...>, Index>::type;
            target = target.push_fixed_subobject<element_type>([&source](auto element_target) {
                return serialiser<element_type>{}(std::get<Index>(source), element_target);
            });

            if constexpr (Index + 1 < sizeof...(Ts)) {
                return _serialise<Index + 1>(source, target);
            }
            else {
                return target;
            }
        }
    };


    namespace detail {

        // Gets the offset of a tuple element from the beginning of the fixed data section.
        // If Index is out of bounds, a compile error occurs.
        template<class Elements, std::size_t Index> requires (Index < Elements::size)
        static inline constexpr std::size_t tuple_element_offset =
            fixed_data_size_v<typename Elements::head> + tuple_element_offset<typename Elements::tail, Index - 1>;

        template<class Elements>
        static inline constexpr std::size_t tuple_element_offset<Elements, 0> = 0;

    }


    template<serialisable... Ts>
    class deserialiser<tuple<Ts...>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr deserialiser(const_bytes_span fixed_data, const_bytes_span variable_data) :
            deserialiser_base{no_fixed_buffer_check, fixed_data, variable_data}
        {
            check_fixed_buffer_size<tuple<Ts...>>(_fixed_data);
        }

        template<std::size_t Index> requires (Index < sizeof...(Ts))
        [[nodiscard]]
        constexpr auto get() const {
            constexpr auto offset = detail::tuple_element_offset<type_list<Ts...>, Index>;
            using element_type = detail::type_list_element<type_list<Ts...>, Index>::type;
            deserialiser<element_type> const deser{
                no_fixed_buffer_check,
                _fixed_data.subspan(offset, fixed_data_size_v<element_type>),
                _variable_data
            };
            return auto_deserialise(deser);
        }
    };

}


namespace std {

    // Structured binding support.


    template<::serialpp::serialisable... Ts>
    struct tuple_size<::serialpp::deserialiser<::serialpp::tuple<Ts...>>> : integral_constant<size_t, sizeof...(Ts)> {};


    template<size_t I, ::serialpp::serialisable... Ts>
    struct tuple_element<I, ::serialpp::deserialiser<::serialpp::tuple<Ts...>>>
        : type_identity<
            ::serialpp::auto_deserialise_t<
                typename ::serialpp::detail::type_list_element<::serialpp::type_list<Ts...>, I>::type>> {};

}

#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

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
        template<typename B> requires (buffer_for<B, Ts> && ...)
        static constexpr void serialise(serialise_source<tuple<Ts...>> const& source, B&& buffer,
                std::size_t fixed_offset) {
            [&source, &buffer, &fixed_offset] <std::size_t... Is> (std::index_sequence<Is...>) {
                (_serialise<Is>(source, buffer, fixed_offset), ...);
            }(std::make_index_sequence<sizeof...(Ts)>{});
        }

    private:
        template<std::size_t Index, typename B> requires (Index < sizeof...(Ts)) && (buffer_for<B, Ts> && ...)
        static constexpr void _serialise(serialise_source<tuple<Ts...>> const& source, B&& buffer,
                std::size_t& fixed_offset) {
            using element_type = detail::type_list_element<type_list<Ts...>, Index>::type;
            fixed_offset = push_fixed_subobject<element_type>(fixed_offset,
                detail::bind_serialise(std::get<Index>(source), buffer));
        }
    };


    namespace detail {

        // Gets the offset of a tuple element from the beginning of the fixed data section.
        // If Index is out of bounds, the usage is ill-formed.
        template<class Elements, std::size_t Index> requires (Index < Elements::size)
        inline constexpr std::size_t tuple_element_offset =
            fixed_data_size_v<typename Elements::head> + tuple_element_offset<typename Elements::tail, Index - 1>;

        template<class Elements>
        inline constexpr std::size_t tuple_element_offset<Elements, 0> = 0;

    }


    template<serialisable... Ts>
    class deserialiser<tuple<Ts...>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        template<std::size_t Index> requires (Index < sizeof...(Ts))
        [[nodiscard]]
        constexpr auto get() const {
            constexpr auto offset = detail::tuple_element_offset<type_list<Ts...>, Index>;
            using element_type = detail::type_list_element<type_list<Ts...>, Index>::type;
            return deserialise<element_type>(_buffer, _fixed_offset + offset);
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
            ::serialpp::deserialise_t<
                typename ::serialpp::detail::type_list_element<::serialpp::type_list<Ts...>, I>::type>> {};

}

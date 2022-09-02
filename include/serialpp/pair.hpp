#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        pair:
            Represented as the result of contiguously serialising the first element, then the second element.
    */


    // Serialisable tuple of two (possibly distinct) types.
    template<serialisable T1, serialisable T2>
    struct pair {
        using first_type = T1;
        using second_type = T2;
    };


    template<serialisable T1, serialisable T2>
    struct fixed_data_size<pair<T1, T2>> : detail::size_t_constant<fixed_data_size_v<T1> + fixed_data_size_v<T2>> {};


    template<serialisable T1, serialisable T2>
    class serialise_source<pair<T1, T2>> : public std::pair<serialise_source<T1>, serialise_source<T2>> {
    public:
        using std::pair<serialise_source<T1>, serialise_source<T2>>::pair;
    };


    template<serialisable T1, serialisable T2>
    struct serialiser<pair<T1, T2>> {
        template<typename B> requires buffer_for<B, T1> && buffer_for<B, T2>
        static constexpr void serialise(serialise_source<pair<T1, T2>> const& source, B&& buffer,
                std::size_t fixed_offset) {
            fixed_offset = push_fixed_subobject<T1>(fixed_offset, detail::bind_serialise(source.first, buffer));
            fixed_offset = push_fixed_subobject<T2>(fixed_offset, detail::bind_serialise(source.second, buffer));
        }
    };


    template<serialisable T1, serialisable T2>
    class deserialiser<pair<T1, T2>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        // Gets the first element.
        [[nodiscard]]
        constexpr deserialise_t<T1> first() const {
            return deserialise<T1>(_buffer, _fixed_offset);
        }

        // Gets the second element.
        [[nodiscard]]
        constexpr deserialise_t<T2> second() const {
            return deserialise<T2>(_buffer, _fixed_offset + fixed_data_size_v<T1>);
        }

        // Gets an element by index.
        template<std::size_t Index> requires (Index <= 1)
        [[nodiscard]]
        constexpr auto get() const {
            if constexpr (Index == 0) {
                return first();
            }
            else if constexpr (Index == 1) {
                return second();
            }
            else {
                static_assert(Index <= 1);
            }
        }
    };

}


namespace std {

    // Structured binding support.


    template<::serialpp::serialisable T1, ::serialpp::serialisable T2>
    struct tuple_size<::serialpp::deserialiser<::serialpp::pair<T1, T2>>> : integral_constant<size_t, 2> {};


    template<::serialpp::serialisable T1, ::serialpp::serialisable T2>
    struct tuple_element<0, ::serialpp::deserialiser<::serialpp::pair<T1, T2>>>
        : type_identity<::serialpp::deserialise_t<T1>> {};

    template<::serialpp::serialisable T1, ::serialpp::serialisable T2>
    struct tuple_element<1, ::serialpp::deserialiser<::serialpp::pair<T1, T2>>>
        : type_identity<::serialpp::deserialise_t<T2>> {};

}

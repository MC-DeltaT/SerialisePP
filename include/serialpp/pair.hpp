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
        using first = T1;
        using second = T2;
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
        template<serialise_buffer Buffer>
        constexpr serialise_target<Buffer> operator()(serialise_source<pair<T1, T2>> const& source,
                serialise_target<Buffer> target) const {
            return target.push_fixed_subobject<T1>([&source](auto first_target) {
                return serialiser<T1>{}(source.first, first_target);
            }).push_fixed_subobject<T2>([&source](auto second_target) {
                return serialiser<T2>{}(source.second, second_target);
            });
        }
    };


    template<serialisable T1, serialisable T2>
    class deserialiser<pair<T1, T2>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr deserialiser(const_bytes_span fixed_data, const_bytes_span variable_data) :
            deserialiser_base{no_fixed_buffer_check, fixed_data, variable_data}
        {
            check_fixed_buffer_size<pair<T1, T2>>(_fixed_data);
        }

        // Gets the first element.
        [[nodiscard]]
        constexpr auto_deserialise_t<T1> first() const {
            deserialiser<T1> const deser{
                no_fixed_buffer_check,
                _fixed_data.first(fixed_data_size_v<T1>),
                _variable_data
            };
            return auto_deserialise(deser);
        }

        // Gets the second element.
        [[nodiscard]]
        constexpr auto_deserialise_t<T2> second() const {
            deserialiser<T2> const deser{
                no_fixed_buffer_check,
                _fixed_data.subspan(fixed_data_size_v<T1>, fixed_data_size_v<T2>),
                _variable_data
            };
            return auto_deserialise(deser);
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
        : type_identity<::serialpp::auto_deserialise_t<T1>> {};

    template<::serialpp::serialisable T1, ::serialpp::serialisable T2>
    struct tuple_element<1, ::serialpp::deserialiser<::serialpp::pair<T1, T2>>>
        : type_identity<::serialpp::auto_deserialise_t<T2>> {};

}

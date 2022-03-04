#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "common.hpp"
#include "scalar.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        Pair:
            Represented as the result of contiguously serialising the first element, then the second element.
    */


    // Serialisable tuple of two (possibly distinct) types.
    template<Serialisable T1, Serialisable T2>
    struct Pair {};


    template<Serialisable T1, Serialisable T2>
    struct FixedDataSize<Pair<T1, T2>> : SizeTConstant<FIXED_DATA_SIZE<T1> + FIXED_DATA_SIZE<T2>> {};


    template<Serialisable T1, Serialisable T2>
    struct SerialiseSource<Pair<T1, T2>> : std::pair<SerialiseSource<T1>, SerialiseSource<T2>> {
        using std::pair<SerialiseSource<T1>, SerialiseSource<T2>>::pair;
    };


    template<Serialisable T1, Serialisable T2>
    struct Serialiser<Pair<T1, T2>> {
        SerialiseTarget operator()(SerialiseSource<Pair<T1, T2>> const& source, SerialiseTarget target) const {
            return target.push_fixed_field<T1>([&source](SerialiseTarget first_target) {
                return Serialiser<T1>{}(source.first, first_target);
            }).push_fixed_field<T2>([&source](SerialiseTarget second_target) {
                return Serialiser<T2>{}(source.second, second_target);
            });
        }
    };


    template<Serialisable T1, Serialisable T2>
    class Deserialiser<Pair<T1, T2>> : public DeserialiserBase<Pair<T1, T2>> {
    public:
        using DeserialiserBase<Pair<T1, T2>>::DeserialiserBase;

        // Gets the first element.
        [[nodiscard]]
        auto first() const {
            Deserialiser<T1> const deserialiser{
                this->_fixed_data.first(FIXED_DATA_SIZE<T1>),
                this->_variable_data
            };
            return auto_deserialise_scalar(deserialiser);
        }

        // Gets the second element.
        [[nodiscard]]
        auto second() const {
            Deserialiser<T2> const deserialiser{
                this->_fixed_data.subspan(FIXED_DATA_SIZE<T1>, FIXED_DATA_SIZE<T2>),
                this->_variable_data
            };
            return auto_deserialise_scalar(deserialiser);
        }

        // Gets an element by index.
        template<std::size_t N> requires (N <= 1)
        [[nodiscard]]
        auto get() const {
            if constexpr (N == 0) {
                return first();
            }
            else if constexpr (N == 1) {
                return second();
            }
            else {
                static_assert(N <= 1);
            }
        }
    };

}


namespace std {

    template<serialpp::Serialisable T1, serialpp::Serialisable T2>
    struct tuple_size<serialpp::Deserialiser<serialpp::Pair<T1, T2>>> : integral_constant<size_t, 2> {};


    template<serialpp::Serialisable T1, serialpp::Serialisable T2>
    struct tuple_element<0, serialpp::Deserialiser<serialpp::Pair<T1, T2>>> : type_identity<T1> {};

    template<serialpp::Serialisable T1, serialpp::Serialisable T2>
    struct tuple_element<1, serialpp::Deserialiser<serialpp::Pair<T1, T2>>> : type_identity<T2> {};

}

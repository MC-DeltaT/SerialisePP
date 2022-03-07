#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        Tuple:
            Represented as the result of contiguously serialising each element of the tuple.
    */


    // Serialisable heterogeneous collection of any number of types.
    template<Serialisable... Ts>
    struct Tuple {
        using Elements = TypeList<Ts...>;
    };


    template<Serialisable... Ts>
    struct FixedDataSize<Tuple<Ts...>> : SizeTConstant<(FIXED_DATA_SIZE<Ts> + ...)> {};

    template<>
    struct FixedDataSize<Tuple<>> : SizeTConstant<0> {};    // Can't fold empty pack over +


    template<Serialisable... Ts>
    class SerialiseSource<Tuple<Ts...>> : public std::tuple<SerialiseSource<Ts>...> {
    public:
        using std::tuple<SerialiseSource<Ts>...>::tuple;

        constexpr SerialiseSource(SerialiseSource<Ts>&&... elements) :  // For some reason std::tuple doesn't have this
            std::tuple<SerialiseSource<Ts>...>{std::move(elements)...}
        {}
    };


    template<Serialisable... Ts>
    struct Serialiser<Tuple<Ts...>> {
        SerialiseTarget operator()(SerialiseSource<Tuple<Ts...>> const& source, SerialiseTarget target) const {
            if constexpr (sizeof...(Ts) > 0) {
                return _serialise<0>(source, target);
            }
            else {
                return target;
            }
        }
    
    private:
        template<std::size_t Index> requires (Index < sizeof...(Ts))
        [[nodiscard]]
        static SerialiseTarget _serialise(SerialiseSource<Tuple<Ts...>> const& source, SerialiseTarget target) {
            using ElementType = TypeListElement<TypeList<Ts...>, Index>;
            target = target.push_fixed_field<ElementType>([&source](SerialiseTarget element_target) {
                return Serialiser<ElementType>{}(std::get<Index>(source), element_target);
            });

            if constexpr (Index + 1 < sizeof...(Ts)) {
                return _serialise<Index + 1>(source, target);
            }
            else {
                return target;
            }
        }
    };


    namespace impl {

        template<class Elements, std::size_t Index> requires (Index < Elements::SIZE)
        static inline constexpr std::size_t ELEMENT_OFFSET =
            FIXED_DATA_SIZE<typename Elements::Head> + ELEMENT_OFFSET<typename Elements::Tail, Index - 1>;

        template<class Elements>
        static inline constexpr std::size_t ELEMENT_OFFSET<Elements, 0> = 0;

    }

    // Gets the offset of a Tuple element from the beginning of the fixed data section.
    // If Index is out of bounds, a compile error occurs.
    template<class Tuple, std::size_t Index> requires (Index < Tuple::Elements::SIZE)
    static inline constexpr auto ELEMENT_OFFSET = impl::ELEMENT_OFFSET<typename Tuple::Elements, Index>;


    template<Serialisable... Ts>
    class Deserialiser<Tuple<Ts...>> : public DeserialiserBase<Tuple<Ts...>> {
    public:
        using DeserialiserBase<Tuple<Ts...>>::DeserialiserBase;

        template<std::size_t Index> requires (Index < sizeof...(Ts))
        [[nodiscard]]
        auto get() const {
            constexpr auto offset = ELEMENT_OFFSET<Tuple<Ts...>, Index>;
            using ElementType = TypeListElement<TypeList<Ts...>, Index>;
            Deserialiser<ElementType> const deserialiser{
                this->_fixed_data.subspan(offset, FIXED_DATA_SIZE<ElementType>),
                this->_variable_data
            };
            return auto_deserialise(deserialiser);
        }
    };

}


namespace std {

    template<serialpp::Serialisable... Ts>
    struct tuple_size<serialpp::Deserialiser<serialpp::Tuple<Ts...>>> : integral_constant<size_t, sizeof...(Ts)> {};


    template<size_t I, serialpp::Serialisable... Ts>
    struct tuple_element<I, serialpp::Deserialiser<serialpp::Tuple<Ts...>>>
        : type_identity<serialpp::AutoDeserialiseResult<serialpp::TypeListElement<serialpp::TypeList<Ts...>, I>>> {};

}

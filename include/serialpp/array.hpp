#pragma once

#include <cassert>
#include <cstddef>
#include <format>
#include <ranges>
#include <tuple>
#include <type_traits>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        Array:
            Represented as the result of contiguously serialising each element of the array.
    */


    // Serialisable fixed-length homogeneous array.
    template<Serialisable T, std::size_t Size>
    struct Array {};


    template<Serialisable T, std::size_t Size>
    struct FixedDataSize<Array<T, Size>> : SizeTConstant<FIXED_DATA_SIZE<T> * Size> {};


    template<Serialisable T, std::size_t Size> requires (Size > 0)
    struct SerialiseSource<Array<T, Size>> {
        [[no_unique_address]]   // For Void
        SerialiseSource<T> elements[Size] = {};
    };

    template<Serialisable T>
    struct SerialiseSource<Array<T, 0>> {};


    template<Serialisable T, std::size_t Size>
    struct Serialiser<Array<T, Size>> {
        SerialiseTarget operator()(SerialiseSource<Array<T, Size>> const& source, SerialiseTarget target) const {
            if constexpr (Size > 0) {
                for (auto const& element : source.elements) {
                    target = target.push_fixed_field<T>([&element](SerialiseTarget element_target) {
                        return Serialiser<T>{}(element, element_target);
                    });
                }
            }
            return target;
        }
    };


    template<Serialisable T, std::size_t Size>
    class Deserialiser<Array<T, Size>> : public DeserialiserBase<Array<T, Size>> {
    public:
        using DeserialiserBase<Array<T, Size>>::DeserialiserBase;

        [[nodiscard]]
        static constexpr std::size_t size() noexcept {
            return Size;
        }

        // Gets the element at the specified index. index must be < size().
        [[nodiscard]]
        auto operator[](std::size_t index) const {
            assert(index < Size);
            Deserialiser<T> const deserialiser{
                this->_fixed_data.subspan(FIXED_DATA_SIZE<T> * index, FIXED_DATA_SIZE<T>),
                this->_variable_data
            };
            return auto_deserialise(deserialiser);
        }

        // Gets the element at the specified index. Throws std::out_of_range if index is out of bounds.
        [[nodiscard]]
        auto at(std::size_t index) const {
            if (index < Size) {
                return (*this)[index];
            }
            else {
                throw std::out_of_range{std::format("Array index {} is out of bounds for size {}", index, Size)};
            }
        }

        // Gets the element at the specified index. If Index is out of bounds, a compile error occurs.
        template<std::size_t Index> requires (Index < Size)
        [[nodiscard]]
        auto get() const {
            return (*this)[Index];
        }

        // Gets a view of the elements.
        [[nodiscard]]
        auto elements() const {
            return std::ranges::views::iota(std::size_t{0}, Size)
                | std::ranges::views::transform([this](std::size_t index) {
                    return (*this)[index];
                });
        }
    };

}


namespace std {

    template<serialpp::Serialisable T, std::size_t Size>
    struct tuple_size<serialpp::Deserialiser<serialpp::Array<T, Size>>> : integral_constant<size_t, Size> {};


    template<std::size_t I, serialpp::Serialisable T, std::size_t Size>
    struct tuple_element<I, serialpp::Deserialiser<serialpp::Array<T, Size>>>
        : type_identity<serialpp::AutoDeserialiseResult<T>> {};

}

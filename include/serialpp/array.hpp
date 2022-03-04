#pragma once

#include <cassert>
#include <cstddef>
#include <format>
#include <ranges>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        Array:
            Represented as the result of contiguously serialising each element of the array.
    */


    // Serialisable fixed-length homogeneous array.
    template<Serialisable T, std::size_t N>
    struct Array {};


    template<Serialisable T, std::size_t N>
    struct FixedDataSize<Array<T, N>> : SizeTConstant<FIXED_DATA_SIZE<T> * N> {};


    template<Serialisable T, std::size_t N> requires (N > 0)
    struct SerialiseSource<Array<T, N>> {
        SerialiseSource<T> elements[N] = {};
    };

    template<Serialisable T>
    struct SerialiseSource<Array<T, 0>> {};


    template<Serialisable T, std::size_t N>
    struct Serialiser<Array<T, N>> {
        SerialiseTarget operator()(SerialiseSource<Array<T, N>> const& source, SerialiseTarget target) const {
            if constexpr (N > 0) {
                for (auto const& element : source.elements) {
                    target = target.push_fixed_field<T>([&element](SerialiseTarget element_target) {
                        return Serialiser<T>{}(element, element_target);
                    });
                }
            }
            return target;
        }
    };


    // TODO: make this destructurable?
    template<Serialisable T, std::size_t N>
    class Deserialiser<Array<T, N>> : public DeserialiserBase<Array<T, N>> {
    public:
        using DeserialiserBase<Array<T, N>>::DeserialiserBase;

        [[nodiscard]]
        static constexpr std::size_t size() noexcept {
            return N;
        }

        // Gets the element at the specified index. index must be < size().
        [[nodiscard]]
        auto operator[](std::size_t index) const {
            assert(index < N);
            Deserialiser<T> const deserialiser{
                this->_fixed_data.subspan(FIXED_DATA_SIZE<T> * index, FIXED_DATA_SIZE<T>),
                this->_variable_data
            };
            return auto_deserialise(deserialiser);
        }

        // Gets the element at the specified index. Throws std::out_of_range if index is out of bounds.
        [[nodiscard]]
        auto at(std::size_t index) const {
            if (index < N) {
                return (*this)[index];
            }
            else {
                throw std::out_of_range{std::format("Array index {} is out of bounds for size {}", index, N)};
            }
        }

        // Gets the element at the specified index. If I is out of bounds, a compile error occurs.
        template<std::size_t I> requires (I < N)
        [[nodiscard]]
        auto get() const {
            return (*this)[I];
        }

        // Gets a view of the elements.
        [[nodiscard]]
        auto elements() const {
            return std::ranges::views::iota(std::size_t{0}, N)
                | std::ranges::views::transform([this](std::size_t index) {
                    return (*this)[index];
                });
        }
    };

}


#pragma once

#include <cassert>
#include <cstddef>
#include <format>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        static_array:
            Represented as the result of contiguously serialising each element of the array.
    */


    // Serialisable fixed-length homogeneous array.
    template<serialisable T, std::size_t Size>
    struct static_array {
        using element_type = T;

        static constexpr std::size_t size = Size;
    };


    template<serialisable T, std::size_t Size>
    struct fixed_data_size<static_array<T, Size>> : detail::size_t_constant<fixed_data_size_v<T> * Size> {};


    template<serialisable T, std::size_t Size> requires (Size > 0)
    struct serialise_source<static_array<T, Size>> {
        [[no_unique_address]]   // For null.
        serialise_source<T> elements[Size];
    };

    template<serialisable T>
    struct serialise_source<static_array<T, 0>> {
        [[no_unique_address]]
        std::ranges::empty_view<serialise_source<T>> elements;
    };


    template<serialisable T, std::size_t Size>
    struct serialiser<static_array<T, Size>> {
        static constexpr void serialise(serialise_source<static_array<T, Size>> const& source,
                serialise_buffer auto&& buffer, std::size_t const fixed_offset) {
            _serialise(source, buffer, fixed_offset);
        }

        static constexpr void serialise(serialise_source<static_array<T, Size>> const& source,
                mutable_bytes_span buffer, std::size_t const fixed_offset)
                requires fixed_size_serialisable<T> || (Size == 0) {
            _serialise(source, buffer, fixed_offset);
        }

    private:
        static constexpr void _serialise(serialise_source<static_array<T, Size>> const& source, auto&& buffer,
                std::size_t fixed_offset) {
            if constexpr (Size > 0) {
                for (auto const& element : source.elements) {
                    fixed_offset = push_fixed_subobject<T>(fixed_offset, detail::bind_serialise(element, buffer));
                }
            }
        }
    };


    template<serialisable T, std::size_t Size>
    class deserialiser<static_array<T, Size>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        [[nodiscard]]
        static constexpr std::size_t size() noexcept {
            return Size;
        }

        // Gets the element at the specified index. index must be < size().
        [[nodiscard]]
        constexpr deserialise_t<T> operator[](std::size_t const index) const {
            assert(index < Size);
            return deserialise<T>(_buffer, _fixed_offset + index * fixed_data_size_v<T>);
        }

        // Gets the element at the specified index. Throws std::out_of_range if index is out of bounds.
        [[nodiscard]]
        constexpr deserialise_t<T> at(std::size_t const index) const {
            if (index < Size) {
                return (*this)[index];
            }
            else {
                throw std::out_of_range{
                    std::format("index {} is out of bounds for static_array with size {}", index, Size)};
            }
        }

        // Gets the element at the specified index. If Index is out of bounds, the usage is ill-formed.
        template<std::size_t Index> requires (Index < Size)
        [[nodiscard]]
        constexpr deserialise_t<T> get() const {
            return (*this)[Index];
        }

        // Gets a view of the elements.
        // The returned view references this deserialiser instance cannot outlive it.
        [[nodiscard]]
        constexpr std::ranges::random_access_range auto elements() const {
            return std::ranges::views::iota(std::size_t{0}, Size)
                | std::ranges::views::transform([this](std::size_t const index) {
                    return (*this)[index];
                });
        }
    };

}


namespace std {

    // Structured binding support.


    template<::serialpp::serialisable T, size_t Size>
    struct tuple_size<::serialpp::deserialiser<::serialpp::static_array<T, Size>>> : integral_constant<size_t, Size> {};


    template<size_t I, ::serialpp::serialisable T, size_t Size>
    struct tuple_element<I, ::serialpp::deserialiser<::serialpp::static_array<T, Size>>>
        : type_identity<::serialpp::deserialise_t<T>> {};

}

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
    struct static_array {};


    template<serialisable T, std::size_t Size>
    struct fixed_data_size<static_array<T, Size>> : detail::size_t_constant<fixed_data_size_v<T> * Size> {};


    template<serialisable T, std::size_t Size> requires (Size > 0)
    struct serialise_source<static_array<T, Size>> {
        [[no_unique_address]]   // For null.
        serialise_source<T> elements[Size] = {};
    };

    template<serialisable T>
    struct serialise_source<static_array<T, 0>> {};


    template<serialisable T, std::size_t Size>
    struct serialiser<static_array<T, Size>> {
        template<serialise_buffer Buffer>
        constexpr serialise_target<Buffer> operator()(serialise_source<static_array<T, Size>> const& source,
                serialise_target<Buffer> target) const {
            if constexpr (Size > 0) {
                for (auto const& element : source.elements) {
                    target = target.push_fixed_subobject<T>([&element](auto element_target) {
                        return serialiser<T>{}(element, element_target);
                    });
                }
            }
            return target;
        }
    };


    template<serialisable T, std::size_t Size>
    class deserialiser<static_array<T, Size>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr deserialiser(const_bytes_span fixed_data, const_bytes_span variable_data) :
            deserialiser_base{no_fixed_buffer_check, fixed_data, variable_data}
        {
            check_fixed_buffer_size<static_array<T, Size>>(_fixed_data);
        }

        [[nodiscard]]
        static constexpr std::size_t size() noexcept {
            return Size;
        }

        // Gets the element at the specified index. index must be < size().
        [[nodiscard]]
        constexpr auto_deserialise_t<T> operator[](std::size_t index) const {
            assert(index < Size);
            deserialiser<T> const deser{
                no_fixed_buffer_check,
                _fixed_data.subspan(fixed_data_size_v<T> * index, fixed_data_size_v<T>),
                _variable_data
            };
            return auto_deserialise(deser);
        }

        // Gets the element at the specified index. Throws std::out_of_range if index is out of bounds.
        [[nodiscard]]
        constexpr auto_deserialise_t<T> at(std::size_t index) const {
            if (index < Size) {
                return (*this)[index];
            }
            else {
                throw std::out_of_range{std::format("static_array index {} is out of bounds for size {}", index, Size)};
            }
        }

        // Gets the element at the specified index. If Index is out of bounds, a compile error occurs.
        template<std::size_t Index> requires (Index < Size)
        [[nodiscard]]
        constexpr auto_deserialise_t<T> get() const {
            return (*this)[Index];
        }

        // Gets a view of the elements.
        // The returned view references this deserialiser instance cannot outlive it.
        [[nodiscard]]
        constexpr std::ranges::random_access_range auto elements() const {
            return std::ranges::views::iota(std::size_t{0}, Size)
                | std::ranges::views::transform([this](std::size_t index) {
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
        : type_identity<::serialpp::auto_deserialise_t<T>> {};

}

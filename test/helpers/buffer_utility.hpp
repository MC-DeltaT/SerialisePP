#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include <serialpp/common.hpp>


namespace serialpp::test {

    template<std::size_t N>
    [[nodiscard]]
    constexpr bool buffer_equal(serialise_buffer auto const& buffer, std::array<unsigned char, N> const& expected) {
        return std::ranges::equal(buffer.span(), std::span{expected}, {}, [](std::byte e) {
            return std::to_integer<unsigned char>(e);
        });
    }


    // Checks if two const_bytes_span view the same memory location and have the same size.
    [[nodiscard]]
    constexpr bool bytes_span_same(const_bytes_span v1, const_bytes_span v2) {
        return v1.data() == v2.data() && v1.size() == v2.size();
    }


    template<typename R>
    [[nodiscard]]
    const_bytes_span as_const_bytes_span(R const& range) {
        return std::as_bytes(std::span{range});
    }


    template<std::size_t N>
    [[nodiscard]]
    constexpr std::array<std::byte, N> uchar_array_to_bytes(std::array<unsigned char, N> const& array) {
        std::array<std::byte, N> bytes{};
        std::ranges::transform(array, bytes.begin(), [](unsigned char e) { return std::byte{e}; });
        return bytes;
    }

}

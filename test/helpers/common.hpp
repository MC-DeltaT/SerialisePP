#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>
#include <serialpp/utility.hpp>


namespace serialpp::test {

    template<std::size_t FixedSize>
    struct mock_serialisable {};


    template<std::size_t N>
    [[nodiscard]]
    constexpr bool buffer_equal(serialise_buffer auto const& buffer, std::array<unsigned char, N> const& expected) {
        return std::ranges::equal(buffer.span(), std::span{expected}, {}, [](std::byte e) {
            return std::to_integer<unsigned char>(e);
        });
    }


    // Checks if two const_bytes_span view the same memory location and have the same size.
    [[nodiscard]]
    constexpr bool bytes_view_same(const_bytes_span v1, const_bytes_span v2) {
        return v1.data() == v2.data() && v1.size() == v2.size();
    }


    template<class R>
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

namespace serialpp {

    template<std::size_t FixedSize>
    struct fixed_data_size<test::mock_serialisable<FixedSize>> : detail::size_t_constant<FixedSize> {};

    template<std::size_t FixedSize>
    struct serialise_source<test::mock_serialisable<FixedSize>> {
        mutable std::vector<serialise_target<basic_serialise_buffer<>>> targets;
    };

    template<std::size_t FixedSize>
    struct serialiser<test::mock_serialisable<FixedSize>> {
        template<serialise_buffer Buffer>
        serialise_target<Buffer> operator()(serialise_source<test::mock_serialisable<FixedSize>> const& source,
                serialise_target<Buffer> target) const {
            source.targets.push_back(target);
            return target;
        }
    };

    template<std::size_t FixedSize>
    class deserialiser<test::mock_serialisable<FixedSize>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr deserialiser(const_bytes_span fixed_data, const_bytes_span variable_data) :
            deserialiser_base{no_fixed_buffer_check, fixed_data, variable_data}
        {
            check_fixed_buffer_size<test::mock_serialisable<FixedSize>>(_fixed_data);
        }

        using deserialiser_base::_fixed_data;
        using deserialiser_base::_variable_data;
    };

    template<std::size_t FixedSize>
    [[nodiscard]]
    constexpr bool operator==(deserialiser<test::mock_serialisable<FixedSize>> const& lhs,
            deserialiser<test::mock_serialisable<FixedSize>> const& rhs) {
        return test::bytes_view_same(lhs._fixed_data, rhs._fixed_data)
            && test::bytes_view_same(lhs._variable_data, rhs._variable_data);
    }


    static_assert(serialisable<test::mock_serialisable<10>>);

}

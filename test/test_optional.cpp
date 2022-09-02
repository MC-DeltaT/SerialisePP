#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>
#include <serialpp/optional.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/mock_serialisable.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block optional_tests = [] {

    static_assert(variable_size_serialisable<optional<int>>);
    static_assert(fixed_data_size_v<optional<char>> == 4);
    static_assert(fixed_data_size_v<optional<std::uint64_t>> == 4);
    static_assert(fixed_data_size_v<optional<mock_serialisable<10000>>> == 4);

    static_assert(std::semiregular<serialise_source<optional<long>>>);

    test_case("serialiser optional empty") = [] {
        basic_buffer buffer;
        serialise_source<optional<std::int32_t>> const source;
        buffer.initialise(4);
        serialiser<optional<std::int32_t>>::serialise(source, buffer, 0);

        std::array<unsigned char, 4> const expected_buffer{0x00, 0x00, 0x00, 0x00};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser optional mock nonempty") = [] {
        using type = optional<mock_serialisable<25>>;
        basic_buffer buffer;
        buffer.initialise(14);
        for (unsigned i = 4; i < 14; ++i) {
            buffer.span()[i] = static_cast<std::byte>(i - 3);
        }
        serialise_source<type> const source{{}};
        serialiser<type>::serialise(source, buffer, 0);

        test_assert(source.value().buffers == std::vector{&buffer});
        test_assert(source.value().fixed_offsets == std::vector<std::size_t>{14});
    };

    test_case("serialiser optional scalar nonempty") = [] {
        using type = optional<std::uint32_t>;
        basic_buffer buffer;
        buffer.initialise(14);
        for (unsigned i = 4; i < 14; ++i) {
            buffer.span()[i] = static_cast<std::byte>(i - 3);
        }
        serialise_source<type> const source{3245678};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 18> const expected_buffer{
            0x0F, 0x00, 0x00, 0x00,     // optional value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00      // optional value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser optional empty") = [] {
        std::array<unsigned char, 4> const buffer{0x00, 0x00, 0x00, 0x00};
        using type = optional<std::uint64_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(!deser.has_value());
        test_assert_throws<std::bad_optional_access>([&deser] {
            (void)deser.value();
        });
    };

    test_case("deserialiser optional mock nonempty") = [] {
        std::array<unsigned char, 70> const buffer{
            0x13, 0x00, 0x00, 0x00      // optional value offset
        };
        auto const buffer_span = as_const_bytes_span(buffer);
        using type = optional<mock_serialisable<48>>;
        deserialiser<type> const deser{buffer_span, 0};
        test_assert(deser.has_value());
        test_assert(bytes_span_same(deser.value()._buffer, buffer_span));
        test_assert(deser.value()._fixed_offset == 18);
    };

    test_case("deserialiser optional scalar nonempty") = [] {
        std::array<unsigned char, 10> const buffer{
            0x09, 0x00, 0x00, 0x00,     // optional value offset
            0x11, 0x22, 0x33, 0x44,     // Dummy padding
            0xFE, 0xDC      // optional value
        };
        using type = optional<std::int16_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.has_value());
        test_assert(deser.value() == -8962);
        test_assert(*deser == -8962);
    };

    test_case("deserialiser optional offset out of bounds") = [] {
        std::array<unsigned char, 10> const buffer{
            0x0F, 0x00, 0x10, 0x00,     // optional value offset
            0x11, 0x22, 0x33, 0x44,     // Dummy padding
            0xFE, 0xDC      // optional value
        };
        using type = optional<std::int16_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert_throws<buffer_bounds_error>([&deser] {
            (void)deser.value();
        });
    };

    test_case("deserialiser optional value partially out of bounds") = [] {
        std::array<unsigned char, 12> const buffer{
            0x0A, 0x00, 0x10, 0x00,     // optional value offset
            0x11, 0x22, 0x33, 0x44,     // Dummy padding
            0xFE, 0xDC, 0x14, 0x86      // optional value
        };
        using type = optional<std::int32_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert_throws<buffer_bounds_error>([&deser] {
            (void)deser.value();
        });
    };

};
}

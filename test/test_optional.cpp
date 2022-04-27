#include <array>
#include <cstddef>
#include <cstdint>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>
#include <serialpp/optional.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block optional_tests = [] {

    static_assert(fixed_data_size_v<optional<char>> == 4);
    static_assert(fixed_data_size_v<optional<std::uint64_t>> == 4);
    static_assert(fixed_data_size_v<optional<mock_serialisable<10000>>> == 4);

    test_case("serialiser optional empty") = [] {
        basic_serialise_buffer buffer;
        serialise_source<optional<std::int32_t>> const source;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<optional<std::int32_t>>(buffer);
        serialiser<optional<std::int32_t>> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_target{buffer, 4, 4, 0, 4};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 4> const expected_buffer{0x00, 0x00, 0x00, 0x00};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser optional mock nonempty") = [] {
        using type = optional<mock_serialisable<25>>;
        basic_serialise_buffer buffer;
        buffer.initialise(14);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[4 + i] = static_cast<std::byte>(i + 1);
        }
        serialise_target const target{buffer, 4, 0, 4, 14};
        serialise_source<type> const source{{}};
        serialiser<type> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 4, 4, 0, 39};
        test_assert(new_target == expected_new_target);
        test_assert(source.value().targets.size() == 1);
        test_assert(source.value().targets.at(0) == serialise_target{buffer, 4, 14, 25, 39});
    };

    test_case("serialiser optional scalar nonempty") = [] {
        basic_serialise_buffer buffer;
        buffer.initialise(14);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[4 + i] = static_cast<std::byte>(i + 1);
        }
        serialise_source<optional<std::uint32_t>> const source{3245678};
        serialise_target const target{buffer, 4, 0, 4, 14};
        serialiser<optional<std::uint32_t>> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_target{buffer, 4, 4, 0, 18};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 18> const expected_buffer{
            0x0B, 0x00, 0x00, 0x00,     // optional value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00      // optional value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser optional empty") = [] {
        std::array<unsigned char, 4> const buffer{0x00, 0x00, 0x00, 0x00};
        using type = optional<std::uint64_t>;
        deserialiser<type> const deser = deserialise<type>(as_const_bytes_span(buffer));
        test_assert(!deser.has_value());
        test_assert_throws<std::bad_optional_access>([&deser] {
            (void)deser.value();
        });
    };

    test_case("deserialiser optional mock nonempty") = [] {
        std::array<unsigned char, 70> const buffer{
            0x09, 0x00, 0x00, 0x00      // optional value offset
        };
        auto const buffer_view = as_const_bytes_span(buffer);
        using type = optional<mock_serialisable<48>>;
        deserialiser<type> const deser = deserialise<type>(buffer_view);
        test_assert(deser.has_value());
        test_assert(bytes_view_same(deser.value()._fixed_data, const_bytes_span{buffer_view.data() + 12, 48}));
        test_assert(bytes_view_same(deser.value()._variable_data, const_bytes_span{buffer_view.data() + 4, 66}));
    };

    test_case("deserialiser optional scalar nonempty") = [] {
        std::array<unsigned char, 10> const buffer{
            0x05, 0x00, 0x00, 0x00,     // optional value offset
            0x11, 0x22, 0x33, 0x44,     // Dummy padding
            0xFE, 0xDC      // optional value
        };
        using type = optional<std::int16_t>;
        deserialiser<type> const deser = deserialise<type>(as_const_bytes_span(buffer));
        test_assert(deser.has_value());
        test_assert(deser.value() == -8962);
        test_assert(*deser == -8962);
    };

    test_case("deserialiser optional offset out of range") = [] {
        std::array<unsigned char, 8> const buffer{
            0x07, 0x00,     // optional value offset
            0x11, 0x22, 0x33, 0x44,     // Dummy padding
            0xFE, 0xDC      // optional value
        };
        using type = optional<std::int16_t>;
        deserialiser<type> const deser = deserialise<type>(as_const_bytes_span(buffer));
        test_assert_throws<variable_buffer_size_error>([&deser] {
            (void)deser.value();
        });
    };

};
}

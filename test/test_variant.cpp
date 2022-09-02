#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/variant.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/mock_serialisable.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block variant_tests = [] {

    static_assert(fixed_size_serialisable<variant<>>);
    static_assert(variable_size_serialisable<variant<char>>);
    static_assert(variable_size_serialisable<variant<char, mock_serialisable<57>>>);
    static_assert(fixed_data_size_v<variant<>> == 2 + 4);
    static_assert(fixed_data_size_v<variant<std::uint8_t, mock_serialisable<100>, std::int32_t>> == 2 + 4);

    test_case("serialiser variant empty serialise_buffer") = [] {
        basic_buffer buffer;
        buffer.initialise(6);
        serialise_source<variant<>> const source;
        serialiser<variant<>>::serialise(source, buffer, 0);

        std::array<unsigned char, 6> const expected_buffer{
            0x00, 0x00,             // Type index
            0x00, 0x00, 0x00, 0x00  // Value offset
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser variant empty span") = [] {
        basic_buffer buffer;
        buffer.initialise(6);
        serialise_source<variant<>> const source;
        serialiser<variant<>>::serialise(source, buffer.span(), 0);

        std::array<unsigned char, 6> const expected_buffer{
            0x00, 0x00,             // Type index
            0x00, 0x00, 0x00, 0x00  // Value offset
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser variant mocks") = [] {
        using type =
            variant<mock_serialisable<534>, mock_serialisable<87>, mock_serialisable<9066>, mock_serialisable<14>>;
        basic_buffer buffer;
        buffer.initialise(16);
        for (unsigned i = 6; i < 16; ++i) {
            buffer.span()[i] = static_cast<std::byte>(i - 5);
        }
        serialise_source<type> const source{std::in_place_index<3>};
        serialiser<type>::serialise(source, buffer, 0);

        test_assert(std::get<3>(source).buffers == std::vector{&buffer});
        test_assert(std::get<3>(source).fixed_offsets == std::vector<std::size_t>{16});
    };

    test_case("serialiser variant scalars") = [] {
        using type = variant<std::uint32_t, std::byte, std::int64_t>;
        basic_buffer buffer;
        buffer.initialise(16);
        for (unsigned i = 6; i < 16; ++i) {
            buffer.span()[i] = static_cast<std::byte>(i - 5);
        }
        serialise_source<type> const source{std::in_place_index<2>, 3'245'678};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 24> const expected_buffer{
            0x02, 0x00,                 // Type index
            0x10, 0x00, 0x00, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser variant empty") = [] {
        std::array<unsigned char, 6> const buffer{
            0x00, 0x00,                 // Type index
            0x00, 0x00, 0x00, 0x00      // Value offset
        };
        deserialiser<variant<>> const deser{as_const_bytes_span(buffer), 0};
        deser.visit([](auto) {
            fail_test();
        });
    };

    test_case("deserialiser variant mocks") = [] {
        std::array<unsigned char, 70> const buffer{
            0x01, 0x00,                 // Type index
            0x2A, 0x00, 0x00, 0x00      // Offset
        };
        auto const buffer_span = as_const_bytes_span(buffer);
        using type = variant<mock_serialisable<10>, mock_serialisable<15>, mock_serialisable<24>>;
        deserialiser<type> const deser{buffer_span, 0};
        test_assert(deser.index() == 1);
        test_assert(bytes_span_same(deser.get<1>()._buffer, buffer_span));
        test_assert(deser.get<1>()._fixed_offset == 42);
    };

    test_case("deserialiser variant scalars") = [] {
        std::array<unsigned char, 24> const buffer{
            0x02, 0x00,                 // Type index
            0x10, 0x00, 0x00, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        using type = variant<std::uint32_t, std::uint8_t, std::int64_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.index() == 2);
        test_assert(deser.get<2>() == 3'245'678);
        test_assert(42 == deser.visit([](auto value) {
            test_assert(std::same_as<decltype(value), std::int64_t>);
            test_assert(value == 3'245'678);
            return 42;
        }));
        test_assert_throws<std::bad_variant_access>([&deser] { (void)deser.get<0>(); });
        test_assert_throws<std::bad_variant_access>([&deser] { (void)deser.get<1>(); });
    };

    test_case("deserialiser variant offset out of bounds") = [] {
        std::array<unsigned char, 24> const buffer{
            0x02, 0x00,                 // Type index
            0x20, 0x00, 0x00, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        using type = variant<std::uint32_t, std::uint8_t, std::int64_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.index() == 2);
        test_assert_throws<buffer_bounds_error>([&deser] {
            (void)deser.get<2>();
        });
        test_assert_throws<buffer_bounds_error>([&deser] {
            deser.visit([](auto) {});
        });
    };

    test_case("deserialiser variant value partially out of bounds") = [] {
        std::array<unsigned char, 24> const buffer{
            0x02, 0x00,                 // Type index
            0x13, 0x00, 0x00, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        using type = variant<std::uint32_t, std::uint8_t, std::int64_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.index() == 2);
        test_assert_throws<buffer_bounds_error>([&deser] {
            (void)deser.get<2>();
        });
        test_assert_throws<buffer_bounds_error>([&deser] {
            deser.visit([](auto) {});
        });
    };

};
}
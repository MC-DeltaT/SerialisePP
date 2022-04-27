#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/variant.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block variant_tests = [] {

    static_assert(fixed_data_size_v<variant<>> == 1 + 4);
    static_assert(fixed_data_size_v<variant<std::uint8_t, mock_serialisable<100>, std::int32_t>> == 1 + 4);

    test_case("serialiser variant empty") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<variant<>>(buffer);
        serialise_source<variant<>> const source;
        serialiser<variant<>> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 5, 5, 0, 5};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 5> const expected_buffer{
            0x00,                   // Type index
            0x00, 0x00, 0x00, 0x00  // Value offset
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser variant mocks") = [] {
        using type =
            variant<mock_serialisable<534>, mock_serialisable<87>, mock_serialisable<9066>, mock_serialisable<14>>;
        basic_serialise_buffer buffer;
        buffer.initialise(15);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[5 + i] = static_cast<std::byte>(i + 1);
        }
        serialise_source<type> const source{std::in_place_index<3>};
        serialise_target const target{buffer, 5, 0, 5, 15};
        serialiser<type> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_target{buffer, 5, 5, 0, 29};
        test_assert(new_target == expected_target);
        test_assert(std::get<3>(source).targets.size() == 1);
        test_assert(std::get<3>(source).targets.at(0) == serialise_target{buffer, 5, 15, 14, 29});
    };

    test_case("serialiser variant scalars") = [] {
        using type = variant<std::uint32_t, std::byte, std::int64_t>;
        basic_serialise_buffer buffer;
        buffer.initialise(15);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[5 + i] = static_cast<std::byte>(i + 1);
        }
        serialise_source<type> const source{std::in_place_index<2>, 3'245'678};
        serialise_target const target{buffer, 5, 0, 5, 15};
        serialiser<type> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_target{buffer, 5, 5, 0, 23};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 23> const expected_buffer{
            0x02,                       // Type index
            0x0A, 0x00, 0x00, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser variant empty") = [] {
        std::array<unsigned char, 5> const buffer{
            0x00,       // Type index
            0x00, 0x00, 0x00, 0x00      // Value offset
        };
        deserialiser<variant<>> const deser = deserialise<variant<>>(as_const_bytes_span(buffer));
        deser.visit([](auto) {
            fail_test();
        });
    };

    test_case("deserialiser variant mocks") = [] {
        std::array<unsigned char, 70> const buffer{
            0x01,                       // Type index
            0x2A, 0x00, 0x00, 0x00      // Offset
        };
        auto const buffer_view = as_const_bytes_span(buffer);
        using type = variant<mock_serialisable<10>, mock_serialisable<15>, mock_serialisable<24>>;
        deserialiser<type> const deser = deserialise<type>(buffer_view);
        test_assert(deser.index() == 1);
        test_assert(bytes_view_same(deser.get<1>()._fixed_data, const_bytes_span{buffer_view.data() + 47, 15}));
        test_assert(bytes_view_same(deser.get<1>()._variable_data, const_bytes_span{buffer_view.data() + 5, 65}));
    };

    test_case("deserialiser variant scalars") = [] {
        std::array<unsigned char, 23> const buffer{
            0x02,           // Type index
            0x0A, 0x00, 0x00, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        using type = variant<std::uint32_t, std::uint8_t, std::int64_t>;
        deserialiser<type> const deser = deserialise<type>(as_const_bytes_span(buffer));
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

    test_case("deserialiser variant offset out of range") = [] {
        std::array<unsigned char, 23> const buffer{
            0x02,           // Type index
            0x1A, 0x00, 0x00, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        using type = variant<std::uint32_t, std::uint8_t, std::int64_t>;
        deserialiser<type> const deser = deserialise<type>(as_const_bytes_span(buffer));
        test_assert(deser.index() == 2);
        test_assert_throws<variable_buffer_size_error>([&deser] {
            (void)deser.get<2>();
        });
        test_assert_throws<variable_buffer_size_error>([&deser] {
            deser.visit([](auto) {});
        });
    };

};
}
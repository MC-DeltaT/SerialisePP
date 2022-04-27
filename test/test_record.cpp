#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>
#include <serialpp/record.hpp>

#include "helpers/common.hpp"
#include "helpers/record.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block record_tests = [] {

    static_assert(record_type<empty_test_record>);
    static_assert(record_type<basic_test_record>);
    static_assert(record_type<mock_test_record>);
    static_assert(record_type<derived_empty_test_record>);
    static_assert(record_type<derived_test_record>);
    static_assert(record_type<more_derived_test_record>);
    static_assert(!record_type<int>);

    static_assert(serialisable<empty_test_record>);
    static_assert(serialisable<basic_test_record>);
    static_assert(serialisable<mock_test_record>);
    static_assert(serialisable<derived_empty_test_record>);
    static_assert(serialisable<derived_test_record>);
    static_assert(serialisable<more_derived_test_record>);

    static_assert(fixed_data_size_v<empty_test_record> == 0);
    static_assert(fixed_data_size_v<basic_test_record> == 1 + 4 + 2 + 8);
    static_assert(fixed_data_size_v<derived_empty_test_record> == 0);
    static_assert(fixed_data_size_v<derived_test_record> == 1 + 4 + 2 + 8 + 2 + 1);
    static_assert(fixed_data_size_v<more_derived_test_record> == 1 + 4 + 2 + 8 + 2 + 1 + 4);

    static_assert(has_field<basic_test_record, "my field">);
    static_assert(has_field<derived_test_record, "my field">);
    static_assert(has_field<derived_test_record, "extra1">);
    static_assert(!has_field<empty_test_record, "nope">);
    static_assert(!has_field<derived_test_record, "still nope">);

    static_assert(record_derived_from<basic_test_record, basic_test_record>);
    static_assert(record_derived_from<derived_empty_test_record, empty_test_record>);
    static_assert(record_derived_from<derived_test_record, basic_test_record>);
    static_assert(record_derived_from<more_derived_test_record, derived_test_record>);
    static_assert(record_derived_from<more_derived_test_record, basic_test_record>);
    static_assert(record_derived_from<mock_derived_test_record, mock_test_record>);
    static_assert(!record_derived_from<basic_test_record, derived_test_record>);
    static_assert(!record_derived_from<derived_test_record, empty_test_record>);
    static_assert(!record_derived_from<derived_test_record, int>);
    static_assert(!record_derived_from<int, derived_test_record>);

    test_case("serialise_source record") = [] {
        serialise_source<basic_test_record> source{
            std::int8_t{1}, std::uint32_t{2u}, std::int16_t{3}, std::uint64_t{4u}};
        test_assert(source.get<"a">() == 1);
        test_assert(source.get<"foo">() == 2u);
        test_assert(source.get<"my field">() == 3);
        test_assert(source.get<"qux">() == 4u);

        source.get<"qux">() = 9'876'543'210ull;
        source.get<"a">() = -1;
        test_assert(source.get<"a">() == -1);
        test_assert(source.get<"foo">() == 2u);
        test_assert(source.get<"my field">() == 3);
        test_assert(source.get<"qux">() == 9'876'543'210ull);
    };

    test_case("serialiser record empty") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<empty_test_record>(buffer);
        serialise_source<empty_test_record> const source;
        serialiser<empty_test_record> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_target{buffer, 0, 0, 0, 0};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser record empty derived") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<derived_empty_test_record>(buffer);
        serialise_source<derived_empty_test_record> const source;
        serialiser<derived_empty_test_record> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_target{buffer, 0, 0, 0, 0};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser record mocks") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<mock_derived_test_record>(buffer);
        serialise_source<mock_derived_test_record> const source;
        serialiser<mock_derived_test_record> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 48, 48, 0, 48};
        test_assert(new_target == expected_new_target);
        test_assert(source.get<"magic">().targets.size() == 1);
        test_assert(source.get<"magic">().targets.at(0) == serialise_target{buffer, 48, 0, 20, 48});
        test_assert(source.get<"foo">().targets.size() == 1);
        test_assert(source.get<"foo">().targets.at(0) == serialise_target{buffer, 48, 20, 10, 48});
        test_assert(source.get<"field">().targets.size() == 1);
        test_assert(source.get<"field">().targets.at(0) == serialise_target{buffer, 48, 30, 5, 48});
        test_assert(source.get<"qux">().targets.size() == 1);
        test_assert(source.get<"qux">().targets.at(0) == serialise_target{buffer, 48, 35, 11, 48});
        test_assert(source.get<"anotherone">().targets.size() == 1);
        test_assert(source.get<"anotherone">().targets.at(0) == serialise_target{buffer, 48, 46, 2, 48});
    };

    test_case("serialiser record scalars") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<basic_test_record>(buffer);
        serialise_source<basic_test_record> const source{
            std::int8_t{-34},
            std::uint32_t{206'000u},
            std::int16_t{36},
            std::uint64_t{360'720u}
        };
        serialiser<basic_test_record> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_target{buffer, 15, 15, 0, 15};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 15> const expected_buffer{
            0xDE,                   // a
            0xB0, 0x24, 0x03, 0x00, // foo
            0x24, 0x00,             // my field
            0x10, 0x81, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00  // qux
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser record derived") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<derived_test_record>(buffer);
        serialise_source<derived_test_record> const source{
            std::int8_t{-34},
            std::uint32_t{206'000u},
            std::int16_t{36},
            std::uint64_t{360'720u},
            std::uint16_t{56543u},
            std::uint8_t{1u}
        };
        serialiser<derived_test_record> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_target{buffer, 18, 18, 0, 18};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 18> const expected_buffer{
            0xDE,                   // a
            0xB0, 0x24, 0x03, 0x00, // foo
            0x24, 0x00,             // my field
            0x10, 0x81, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, // qux
            0xDF, 0xDC,             // extra1
            0x01                    // extra2
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser record empty") = [] {
        std::array<unsigned char, 0> const buffer{};
        deserialiser<empty_test_record> const deser = deserialise<empty_test_record>(as_const_bytes_span(buffer));
    };

    test_case("deserialiser record mocks") = [] {
        std::array<std::byte, 100> const buffer{};

        auto const test_base_fields = [&buffer] <record_derived_from<mock_test_record> T> (deserialiser<T> const& deser) {
            test_assert(bytes_view_same(deser.get<"magic">()._fixed_data, const_bytes_span{buffer.data(), 20}));
            test_assert(bytes_view_same(deser.get<"magic">()._variable_data, const_bytes_span{buffer.data() + 48, 52}));
            test_assert(bytes_view_same(deser.get<"foo">()._fixed_data, const_bytes_span{buffer.data() + 20, 10}));
            test_assert(bytes_view_same(deser.get<"foo">()._variable_data, const_bytes_span{buffer.data() + 48, 52}));
            test_assert(bytes_view_same(deser.get<"field">()._fixed_data, const_bytes_span{buffer.data() + 30, 5}));
            test_assert(bytes_view_same(deser.get<"field">()._variable_data, const_bytes_span{buffer.data() + 48, 52}));
        };

        deserialiser<mock_derived_test_record> const deser = deserialise<mock_derived_test_record>(buffer);
        test_base_fields.operator()<mock_test_record>(deser);
        test_base_fields(deser);
        test_assert(bytes_view_same(deser.get<"qux">()._fixed_data, const_bytes_span{buffer.data() + 35, 11}));
        test_assert(bytes_view_same(deser.get<"qux">()._variable_data, const_bytes_span{buffer.data() + 48, 52}));
        test_assert(bytes_view_same(deser.get<"anotherone">()._fixed_data, const_bytes_span{buffer.data() + 46, 2}));
        test_assert(bytes_view_same(deser.get<"anotherone">()._variable_data, const_bytes_span{buffer.data() + 48, 52}));
    };

    test_case("deserialiser record scalars") = [] {
        std::array<unsigned char, 15> const buffer{
            0x9C,                   // a
            0x15, 0xCD, 0x5B, 0x07, // foo
            0x30, 0x75,             // my field
            0xFF, 0xE7, 0x76, 0x48, 0x17, 0x00, 0x00, 0x00  // qux
        };
        deserialiser<basic_test_record> const deser = deserialise<basic_test_record>(as_const_bytes_span(buffer));
        test_assert(deser.get<"a">() == -100);
        test_assert(deser.get<"foo">() == 123'456'789u);
        test_assert(deser.get<"my field">() == 30000);
        test_assert(deser.get<"qux">() == 99'999'999'999ull);
        auto const [a, foo, my_field, qux] = deser;
        test_assert(a == -100);
        test_assert(foo == 123'456'789u);
        test_assert(my_field == 30000);
        test_assert(qux == 99'999'999'999ull);
    };

    test_case("deserialiser record derived") = [] {
        std::array<unsigned char, 18> const buffer{
            0x9C,                   // a
            0x15, 0xCD, 0x5B, 0x07, // foo
            0x30, 0x75,             // my field
            0xFF, 0xE7, 0x76, 0x48, 0x17, 0x00, 0x00, 0x00, // qux
            0x5E, 0x7A,             // extra1
            0x65                    // extra2
        };

        auto const test_base_fields = [] <record_derived_from<basic_test_record> T> (deserialiser<T> const& deser) {
            test_assert(deser.get<"a">() == -100);
            test_assert(deser.get<"foo">() == 123'456'789u);
            test_assert(deser.get<"my field">() == 30000);
            test_assert(deser.get<"qux">() == 99'999'999'999ull);
        };

        deserialiser<derived_test_record> const deser = deserialise<derived_test_record>(as_const_bytes_span(buffer));
        test_base_fields.operator()<basic_test_record>(deser);
        test_base_fields(deser);
        test_assert(deser.get<"extra1">() == 31326u);
        test_assert(deser.get<"extra2">() == 101u);
        auto const [a, foo, my_field, qux, extra1, extra2] = deser;
        test_assert(a == -100);
        test_assert(foo == 123'456'789u);
        test_assert(my_field == 30000);
        test_assert(qux == 99'999'999'999ull);
        test_assert(extra1 == 31326u);
        test_assert(extra2 == 101u);
    };

    test_case("deserialiser record more derived") = [] {
        std::array<unsigned char, 22> const buffer{
            0x9C,                   // a
            0x15, 0xCD, 0x5B, 0x07, // foo
            0x30, 0x75,             // my field
            0xFF, 0xE7, 0x76, 0x48, 0x17, 0x00, 0x00, 0x00, // qux
            0x5E, 0x7A,             // extra1
            0x65,                   // extra2
            0x56, 0x83, 0xAE, 0x6B  // really extra
        };

        auto const test_base1_fields = [] <record_derived_from<basic_test_record> T> (deserialiser<T> const& deser) {
            test_assert(deser.get<"a">() == -100);
            test_assert(deser.get<"foo">() == 123'456'789u);
            test_assert(deser.get<"my field">() == 30000);
            test_assert(deser.get<"qux">() == 99'999'999'999ull);
        };

        auto const test_base2_fields = [] <record_derived_from<derived_test_record> T> (deserialiser<T> const& deser) {
            test_assert(deser.get<"extra1">() == 31326u);
            test_assert(deser.get<"extra2">() == 101u);
        };

        deserialiser<more_derived_test_record> const deser =
            deserialise<more_derived_test_record>(as_const_bytes_span(buffer));
        test_base1_fields.operator()<basic_test_record>(deser);
        test_base1_fields.operator()<derived_test_record>(deser);
        test_base1_fields(deser);
        test_base2_fields.operator()<derived_test_record>(deser);
        test_base2_fields(deser);
        test_assert(deser.get<"really extra">() == 1'806'598'998ul);
        auto const [a, foo, my_field, qux, extra1, extra2, really_extra] = deser;
        test_assert(a == -100);
        test_assert(foo == 123'456'789u);
        test_assert(my_field == 30000);
        test_assert(qux == 99'999'999'999ull);
        test_assert(extra1 == 31326u);
        test_assert(extra2 == 101u);
        test_assert(really_extra == 1'806'598'998ul);
    };

};
}

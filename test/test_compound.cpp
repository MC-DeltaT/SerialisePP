#include <algorithm>
#include <array>
#include <cstdint>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>
#include <serialpp/dynamic_array.hpp>
#include <serialpp/optional.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/variant.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/compound_types.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block compound_tests = [] {

    static_assert(fixed_data_size_v<optional<optional<std::int32_t>>> == 4);

    test_case("serialiser optional of optional") = [] {
        using type = optional<optional<std::int16_t>>;
        basic_buffer buffer;
        buffer.initialise(4);
        serialise_source<type> const source{{{-1654}}};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 10> const expected_buffer{
            0x05, 0x00, 0x00, 0x00,     // Outer optional value offset
            0x09, 0x00, 0x00, 0x00,     // Inner optional value offset
            0x8A, 0xF9      // Inner optional value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser optional of optional") = [] {
        using type = optional<optional<std::int32_t>>;
        std::array<unsigned char, 12> const buffer{
            0x05, 0x00, 0x00, 0x00,     // Outer optional value offset
            0x09, 0x00, 0x00, 0x00,     // Inner optional value offset
            0xC2, 0x5F, 0x02, 0x8E      // Inner optional value
        };
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.has_value());
        deserialiser<optional<std::int32_t>> const inner = deser.value();
        test_assert(inner.has_value());
        test_assert(inner.value() == -1'912'447'038);
    };


    static_assert(fixed_data_size_v<dynamic_array<dynamic_array<std::uint64_t>>> == 4 + 4);

    test_case("serialiser dynamic_array of dynamic_array") = [] {
        using type = dynamic_array<dynamic_array<std::uint32_t>>;
        basic_buffer buffer;
        buffer.initialise(8);
        serialise_source<type> const source{{
            {{11'223'344, 1'566'778'899, 123'456'789}},
            {{10'203'040}}
        }};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 40> const expected_buffer{
            0x02, 0x00, 0x00, 0x00,     // Outer array size
            0x08, 0x00, 0x00, 0x00,     // Outer array offset
            0x03, 0x00, 0x00, 0x00,     // Inner array 0 size
            0x18, 0x00, 0x00, 0x00,     // Inner array 0 offset
            0x01, 0x00, 0x00, 0x00,     // Inner array 1 size
            0x24, 0x00, 0x00, 0x00,     // Inner array 1 offset
            0x30, 0x41, 0xAB, 0x00,     // Inner array 0 elements
            0x13, 0x26, 0x63, 0x5D,
            0x15, 0xCD, 0x5B, 0x07,
            0xA0, 0xAF, 0x9B, 0x00      // Inner array 1 elements
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser dynamic_array of dynamic_array") = [] {
        std::array<unsigned char, 32> const buffer{
            0x02, 0x00, 0x00, 0x00,     // Outer array size
            0x08, 0x00, 0x00, 0x00,     // Outer array offset
            0x03, 0x00, 0x00, 0x00,     // Inner array 0 size
            0x18, 0x00, 0x00, 0x00,     // Inner array 0 offset
            0x01, 0x00, 0x00, 0x00,     // Inner array 1 size
            0x1E, 0x00, 0x00, 0x00,     // Inner array 1 offset
            0x12, 0x34,     // Inner array 0 elements
            0x4F, 0x7A,
            0x31, 0x12,
            0x11, 0x33,     // Inner array 1 elements
        };

        using type = dynamic_array<dynamic_array<std::uint16_t>>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.size() == 2);

        deserialiser<dynamic_array<std::uint16_t>> const inner0 = deser[0];
        test_assert(inner0.size() == 3);
        std::array<std::uint16_t, 3> const expected_elements0{13330, 31311, 4657};
        test_assert(std::ranges::equal(inner0.elements(), expected_elements0));

        deserialiser<dynamic_array<std::uint16_t>> const inner1 = deser[1];
        test_assert(inner1.size() == 1);
        std::array<std::uint16_t, 1> const expected_elements1{13073};
        test_assert(std::ranges::equal(inner1.elements(), expected_elements1));
    };


    static_assert(fixed_data_size_v<variant<variant<std::uint64_t, std::int16_t>>> == 2 + 4);

    test_case("serialiser variant of variant") = [] {
        using V1 = variant<std::uint32_t, std::uint16_t>;
        using V2 = variant<std::uint8_t, std::int16_t, std::int32_t>;
        using V3 = variant<V1, V2>;
        basic_buffer buffer;
        buffer.initialise(6);
        serialise_source<V3> const source{serialise_source<V2>{serialise_source<std::int32_t>{-123'456'789ll}}};
        serialiser<V3>::serialise(source, buffer, 0);

        std::array<unsigned char, 16> const expected_buffer{
            0x01, 0x00,             // Outer variant type index
            0x06, 0x00, 0x00, 0x00, // Outer variant value offset
            0x02, 0x00,             // Inner variant type index
            0x0C, 0x00, 0x00, 0x00, // Inner variant value offset
            0xEB, 0x32, 0xA4, 0xF8  // Inner variant value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser variant of variant") = [] {
        using type = variant<variant<std::uint32_t, std::uint16_t>, variant<std::uint8_t, std::int16_t, std::int32_t>>;
        std::array<unsigned char, 16> const buffer{
            0x01, 0x00,             // Outer variant type index
            0x06, 0x00, 0x00, 0x00, // Outer variant value offset
            0x02, 0x00,             // Inner variant type index
            0x0C, 0x00, 0x00, 0x00, // Inner variant value offset
            0xEB, 0x32, 0xA4, 0xF8  // Inner variant value
        };
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.index() == 1);
        deserialiser<variant<std::uint8_t, std::int16_t, std::int32_t>> const inner = deser.get<1>();
        test_assert(inner.index() == 2);
        test_assert(inner.get<2>() == -123'456'789ll);
    };


    static_assert(fixed_data_size_v<compound_test_record3> == 70);

    test_case("serialiser compound record") = [] {
        basic_buffer buffer;
        buffer.initialise(70);
        serialise_source<compound_test_record3> const source{
            {
                {
                    {
                        {
                            {{48815, 28759, 46627}},
                            123,
                            {}
                        },
                        {{
                            {{26676, 53866, 58316}},
                            148,
                            {}
                        }},
                        {{
                            {
                                {{19768}},
                                61,
                                {}
                            },
                            {
                                {},
                                20,
                                -34'562'034'598'108'927ll
                            }
                        }},
                        {{
                            {
                                {{20}},
                                1,
                                {-857923}
                            },
                            {
                                {},
                                16,
                                {}
                            }
                        }}
                    },
                    {
                        {{10314, 5267, 56351, 11437, 38287}},
                        44,
                        1'685'439'465'438'748ll
                    }
                },
                {
                    serialise_source<compound_test_record1>{
                        std::array<std::uint16_t, 4>{1000, 2000, 3000, 4000},
                        67,
                        -65'473'985'456ll
                    }
                }
            }
        };
        serialiser<compound_test_record3>::serialise(source, buffer, 0);

        std::array<unsigned char, 188> const expected_buffer{
            // Fixed data
            0x03, 0x00, 0x00, 0x00,     // pair_record2_record1.first.record1.darray_u16 size
            0x46, 0x00, 0x00, 0x00,     // pair_record2_record1.first.record1.darray_u16 offset
            0x7B,                       // pair_record2_record1.first.record1.u8
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.record1.opt_i64 value offset
            0x4D, 0x00, 0x00, 0x00,     // pair_record2_record1.first.opt_record1 value offset
            0x02, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1 size
            0x5F, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1 offset
            0x01, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[0].darray_u16 size
            0x83, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[0].darray_u16 offset
            0x01,                       // pair_record2_record1.first.sarray_record1[0].u8
            0x86, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[0].opt_i64 value offset
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[1].darray_u16 size
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[1].darray_u16 offset
            0x10,                       // pair_record2_record1.first.sarray_record1[1].u8
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[1].opt_i64 value offset
            0x05, 0x00, 0x00, 0x00,     // pair_record2_record1.second.darray_u16 size
            0x8D, 0x00, 0x00, 0x00,     // pair_record2_record1.second.darray_u16 offset
            0x2C,                       // pair_record2_record1.second.u8
            0x98, 0x00, 0x00, 0x00,     // pair_record2_record1.second.opt_i64 value offset
            0x01, 0x00,                 // var_record1_record2 type index
            0x9F, 0x00, 0x00, 0x00,     // var_record1_record2 value offset
            // Variable data
            0xAF, 0xBE,                 // pair_record2_record1.first.record1.darray_u16 elements
            0x57, 0x70,
            0x23, 0xB6,
            0x03, 0x00, 0x00, 0x00,     // pair_record2_record1.first.opt_record1.darray_u16 size
            0x59, 0x00, 0x00, 0x00,     // pair_record2_record1.first.opt_record1.darray_u16 offset
            0x94,                       // pair_record2_record1.first.opt_record1.u8
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.opt_record1.opt_i64 value offset
            0x34, 0x68,                 // pair_record2_record1.first.opt_record1.darray_u16 elements
            0x6A, 0xD2,
            0xCC, 0xE3,
            0x01, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[0].darray_u16 size
            0x79, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[0].darray_u16 offset
            0x3D,                       // pair_record2_record1.first.darray_record1[0].u8
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[0].opt_i64 value offset
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[1].darray_u16 size
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[1].darray_u16 offset
            0x14,                       // pair_record2_record1.first.darray_record1[1].u8
            0x7C, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[1].opt_i64 value offset
            0x38, 0x4D,                 // pair_record2_record1.first.darray_record1[0].darray_u16 elements
            0x01, 0xA1, 0x10, 0x3D, 0x03, 0x36, 0x85, 0xFF, // pair_record2_record1.first.darray_record1[1].opt_i64 value
            0x14, 0x00,                 // pair_record2_record1.first.sarray_record1[0].darray_u16 elements
            0xBD, 0xE8, 0xF2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // pair_record2_record1.first.sarray_record1[0].opt_i64 value
            0x4A, 0x28,                 // pair_record2_record1.second.darray_u16 elements
            0x93, 0x14,
            0x1F, 0xDC,
            0xAD, 0x2C,
            0x8F, 0x95,
            0x1C, 0xBE, 0xA0, 0xF4, 0xE5, 0xFC, 0x05, 0x00, // pair_record2_record1.second.opt_i64 value
            0x04, 0x00, 0x00, 0x00,     // var_record1_record2.record1.darray_u16 size
            0xAC, 0x00, 0x00, 0x00,     // var_record1_record2.record1.darray_u16 offset
            0x43,                       // var_record1_record2.record1.u8
            0xB5, 0x00, 0x00, 0x00,     // var_record1_record2.record1.opt_i64 value offset
            0xE8, 0x03,                 // var_record1_record2.record1.darray_u16 elements
            0xD0, 0x07,
            0xB8, 0x0B,
            0xA0, 0x0F,
            0x50, 0x44, 0x72, 0xC1, 0xF0, 0xFF, 0xFF, 0xFF  // var_record1_record2.record1.opt_i64 value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser compound record") = [] {
        std::array<unsigned char, 188> const buffer{
            // Fixed data
            0x03, 0x00, 0x00, 0x00,     // pair_record2_record1.first.record1.darray_u16 size
            0x46, 0x00, 0x00, 0x00,     // pair_record2_record1.first.record1.darray_u16 offset
            0x7B,                       // pair_record2_record1.first.record1.u8
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.record1.opt_i64 value offset
            0x4D, 0x00, 0x00, 0x00,     // pair_record2_record1.first.opt_record1 value offset
            0x02, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1 size
            0x5F, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1 offset
            0x01, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[0].darray_u16 size
            0x83, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[0].darray_u16 offset
            0x01,                       // pair_record2_record1.first.sarray_record1[0].u8
            0x86, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[0].opt_i64 value offset
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[1].darray_u16 size
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[1].darray_u16 offset
            0x10,                       // pair_record2_record1.first.sarray_record1[1].u8
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.sarray_record1[1].opt_i64 value offset
            0x05, 0x00, 0x00, 0x00,     // pair_record2_record1.second.darray_u16 size
            0x8D, 0x00, 0x00, 0x00,     // pair_record2_record1.second.darray_u16 offset
            0x2C,                       // pair_record2_record1.second.u8
            0x98, 0x00, 0x00, 0x00,     // pair_record2_record1.second.opt_i64 value offset
            0x01, 0x00,                 // var_record1_record2 type index
            0x9F, 0x00, 0x00, 0x00,     // var_record1_record2 value offset
            // Variable data
            0xAF, 0xBE,                 // pair_record2_record1.first.record1.darray_u16 elements
            0x57, 0x70,
            0x23, 0xB6,
            0x03, 0x00, 0x00, 0x00,     // pair_record2_record1.first.opt_record1.darray_u16 size
            0x59, 0x00, 0x00, 0x00,     // pair_record2_record1.first.opt_record1.darray_u16 offset
            0x94,                       // pair_record2_record1.first.opt_record1.u8
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.opt_record1.opt_i64 value offset
            0x34, 0x68,                 // pair_record2_record1.first.opt_record1.darray_u16 elements
            0x6A, 0xD2,
            0xCC, 0xE3,
            0x01, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[0].darray_u16 size
            0x79, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[0].darray_u16 offset
            0x3D,                       // pair_record2_record1.first.darray_record1[0].u8
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[0].opt_i64 value offset
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[1].darray_u16 size
            0x00, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[1].darray_u16 offset
            0x14,                       // pair_record2_record1.first.darray_record1[1].u8
            0x7C, 0x00, 0x00, 0x00,     // pair_record2_record1.first.darray_record1[1].opt_i64 value offset
            0x38, 0x4D,                 // pair_record2_record1.first.darray_record1[0].darray_u16 elements
            0x01, 0xA1, 0x10, 0x3D, 0x03, 0x36, 0x85, 0xFF, // pair_record2_record1.first.darray_record1[1].opt_i64 value
            0x14, 0x00,                 // pair_record2_record1.first.sarray_record1[0].darray_u16 elements
            0xBD, 0xE8, 0xF2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // pair_record2_record1.first.sarray_record1[0].opt_i64 value
            0x4A, 0x28,                 // pair_record2_record1.second.darray_u16 elements
            0x93, 0x14,
            0x1F, 0xDC,
            0xAD, 0x2C,
            0x8F, 0x95,
            0x1C, 0xBE, 0xA0, 0xF4, 0xE5, 0xFC, 0x05, 0x00, // pair_record2_record1.second.opt_i64 value
            0x04, 0x00, 0x00, 0x00,     // var_record1_record2.record1.darray_u16 size
            0xAC, 0x00, 0x00, 0x00,     // var_record1_record2.record1.darray_u16 offset
            0x43,                       // var_record1_record2.record1.u8
            0xB5, 0x00, 0x00, 0x00,     // var_record1_record2.record1.opt_i64 value offset
            0xE8, 0x03,                 // var_record1_record2.record1.darray_u16 elements
            0xD0, 0x07,
            0xB8, 0x0B,
            0xA0, 0x0F,
            0x50, 0x44, 0x72, 0xC1, 0xF0, 0xFF, 0xFF, 0xFF  // var_record1_record2.record1.opt_i64 value
        };
        deserialiser<compound_test_record3> const deser{as_const_bytes_span(buffer), 0};

        auto const tuple = deser.get<"tuple">();

        auto const [pair, var] = tuple;

        auto const [record2, record1] = pair;

        auto const record2_record1 = record2.get<"record1">();
        test_assert(std::ranges::equal(record2_record1.get<"darray_u16">().elements(), std::array{48815, 28759, 46627}));
        test_assert(record2_record1.get<"u8">() == 123);
        test_assert(!record2_record1.get<"opt_i64">().has_value());

        auto const record2_opt_record1 = record2.get<"opt_record1">();
        test_assert(record2_opt_record1.has_value());
        auto const record2_opt_record1_v = record2_opt_record1.value();
        test_assert(
            std::ranges::equal(record2_opt_record1_v.get<"darray_u16">().elements(), std::array{26676, 53866, 58316}));
        test_assert(record2_opt_record1_v.get<"u8">() == 148);
        test_assert(!record2_opt_record1_v.get<"opt_i64">().has_value());

        auto const record2_darray_record1 = record2.get<"darray_record1">();
        test_assert(record2_darray_record1.size() == 2);
        test_assert(std::ranges::equal(record2_darray_record1.at(0).get<"darray_u16">().elements(), std::array{19768}));
        test_assert(record2_darray_record1.at(0).get<"u8">() == 61);
        test_assert(!record2_darray_record1.at(0).get<"opt_i64">().has_value());
        test_assert(record2_darray_record1.at(1).get<"darray_u16">().empty());
        test_assert(record2_darray_record1.at(1).get<"u8">() == 20);
        test_assert(record2_darray_record1.at(1).get<"opt_i64">().has_value());
        test_assert(record2_darray_record1.at(1).get<"opt_i64">().value() == -34'562'034'598'108'927ll);

        auto const record2_sarray_record1 = record2.get<"sarray_record1">();
        test_assert(std::ranges::equal(record2_sarray_record1.at(0).get<"darray_u16">().elements(), std::array{20}));
        test_assert(record2_sarray_record1.at(0).get<"u8">() == 1);
        test_assert(record2_sarray_record1.at(0).get<"opt_i64">().has_value());
        test_assert(record2_sarray_record1.at(0).get<"opt_i64">().value() == -857923);
        test_assert(record2_sarray_record1.at(1).get<"darray_u16">().empty());
        test_assert(record2_sarray_record1.at(1).get<"u8">() == 16);
        test_assert(!record2_sarray_record1.at(1).get<"opt_i64">().has_value());

        test_assert(
            std::ranges::equal(record1.get<"darray_u16">().elements(), std::array{10314, 5267, 56351, 11437, 38287}));
        test_assert(record1.get<"u8">() == 44);
        test_assert(record1.get<"opt_i64">().has_value());
        test_assert(record1.get<"opt_i64">().value() == 1'685'439'465'438'748ll);

        test_assert(var.index() == 1);
        auto const var_record1 = var.get<1>();
        test_assert(std::ranges::equal(var_record1.get<"darray_u16">().elements(), std::array{1000, 2000, 3000, 4000}));
        test_assert(var_record1.get<"u8">() == 67);
        test_assert(var_record1.get<"opt_i64">().has_value());
        test_assert(var_record1.get<"opt_i64">().value() == -65'473'985'456ll);
    };


    static_assert(fixed_data_size_v<constexpr_test_record> == 28);

    test_case("serialise()/2 constexpr") = [] {
        []() consteval {
            basic_buffer buffer;
            serialise_source<constexpr_test_record> source{
                9'214'357,
                {765, -50},
                {71, 266, 76589},
                {{11, 22, 33, 44}},
                -685249,
                serialise_source<std::int8_t>{48}
            };
            serialise(source, buffer);

            std::array<unsigned char, 33> const expected_buffer{
                0x95, 0x99, 0x8C, 0x00,     // i32
                0xFD, 0x02,                 // pair_u16_i8.first
                0xCE,                       // pair_u16_i8.second
                0x47,                       // tuple_u8_u16_u32[0]
                0x0A, 0x01,                 // tuple_u8_u16_u32[1]
                0x2D, 0x2B, 0x01, 0x00,     // tuple_u8_u16_u32[2]
                0x0B,                       // sarray_u8 elements
                0x16,
                0x21,
                0x2C,
                0x1D, 0x00, 0x00, 0x00,     // opt_i32 value offset
                0x01, 0x00,                 // var_u64_i8 type index
                0x20, 0x00, 0x00, 0x00,     // var_u64_i8 value offset
                0x3F, 0x8B, 0xF5, 0xFF,     // opt_i32 value
                0x30                        // var_u64_i8 value
            };
            test_assert(buffer_equal(buffer, expected_buffer));
        }();
    };

    test_case("deserialise()/1 constexpr") = [] {
        []() consteval {
            std::array<unsigned char, 33> const buffer{
                0x95, 0x99, 0x8C, 0x00,     // i32
                0xFD, 0x02,                 // pair_u16_i8.first
                0xCE,                       // pair_u16_i8.second
                0x47,                       // tuple_u8_u16_u32[0]
                0x0A, 0x01,                 // tuple_u8_u16_u32[1]
                0x2D, 0x2B, 0x01, 0x00,     // tuple_u8_u16_u32[2]
                0x0B,                       // sarray_u8 elements
                0x16,
                0x21,
                0x2C,
                0x1D, 0x00, 0x00, 0x00,     // opt_i32 value offset
                0x01, 0x00,                 // var_u64_i8 type index
                0x20, 0x00, 0x00, 0x00,     // var_u64_i8 value offset
                0x3F, 0x8B, 0xF5, 0xFF,     // opt_i32 value
                0x30                        // var_u64_i8 value
            };
            auto const buffer_bytes = uchar_array_to_bytes(buffer);
            deserialiser<constexpr_test_record> const deser =
                deserialise<constexpr_test_record>(const_bytes_span{buffer_bytes});

            test_assert(deser.get<"i32">() == 9'214'357);
            test_assert(deser.get<"pair_u16_i8">().first() == 765);
            test_assert(deser.get<"pair_u16_i8">().second() == -50);
            test_assert(deser.get<"tuple_u8_u16_u32">().get<0>() == 71);
            test_assert(deser.get<"tuple_u8_u16_u32">().get<1>() == 266);
            test_assert(deser.get<"tuple_u8_u16_u32">().get<2>() == 76589);
            test_assert(std::ranges::equal(deser.get<"sarray_u8">().elements(), std::array{11, 22, 33, 44}));
            test_assert(deser.get<"opt_i32">().value() == -685249);
            test_assert(deser.get<"var_u64_i8">().get<1>() == 48);
        }();
    };

};
}

#include <algorithm>
#include <array>
#include <cstdint>

#include <serialpp/common.hpp>
#include <serialpp/list.hpp>
#include <serialpp/optional.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/compound.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    static_assert(FIXED_DATA_SIZE<Optional<Optional<std::int32_t>>> == 2);

    STEST_CASE(Serialiser_OptionalOfOptional) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Optional<Optional<std::int16_t>>>();
        SerialiseSource<Optional<Optional<std::int16_t>>> const source{{{-1654}}};
        Serialiser<Optional<Optional<std::int16_t>>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 2, 2, 0, 6};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 6> const expected_buffer{
            0x01, 0x00,     // Outer optional value offset
            0x03, 0x00,     // Inner optional value offset
            0x8A, 0xF9      // Inner optional value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_OptionalOfOptional) {
        std::array<unsigned char, 8> const buffer{
            0x01, 0x00,             // Outer optional value offset
            0x03, 0x00,             // Inner optional value offset
            0xC2, 0x5F, 0x02, 0x8E  // Inner optional value
        };
        auto const deserialiser = deserialise<Optional<Optional<std::int32_t>>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.has_value());
        auto const inner = deserialiser.value();
        test_assert(inner.has_value());
        test_assert(inner.value() == -1'912'447'038);
    }


    static_assert(FIXED_DATA_SIZE<List<List<std::uint64_t>>> == 2 + 2);

    STEST_CASE(Serialiser_ListOfList) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<List<List<std::uint32_t>>>();
        SerialiseSource<List<List<std::uint32_t>>> const source{{
            {{11'223'344, 1'566'778'899, 123'456'789}},
            {{10'203'040}}
        }};
        Serialiser<List<List<std::uint32_t>>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 4, 4, 0, 28};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 28> const expected_buffer{
            0x02, 0x00,     // Outer list size
            0x00, 0x00,     // Outer list offset
            0x03, 0x00,     // Inner list 0 size
            0x08, 0x00,     // Inner list 0 offset
            0x01, 0x00,     // Inner list 1 size
            0x14, 0x00,     // Inner list 1 offset
            0x30, 0x41, 0xAB, 0x00,     // Inner list 0 elements
            0x13, 0x26, 0x63, 0x5D,
            0x15, 0xCD, 0x5B, 0x07,
            0xA0, 0xAF, 0x9B, 0x00      // Inner list 1 elements
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_ListOfList) {
        std::array<unsigned char, 20> const buffer{
            0x02, 0x00,     // Outer list size
            0x00, 0x00,     // Outer list offset
            0x03, 0x00,     // Inner list 0 size
            0x08, 0x00,     // Inner list 0 offset
            0x01, 0x00,     // Inner list 1 size
            0x0E, 0x00,     // Inner list 1 offset
            0x12, 0x34,     // Inner list 0 elements
            0x4F, 0x7A,
            0x31, 0x12,
            0x11, 0x33,     // Inner list 1 elements
        };

        auto const deserialiser = deserialise<List<List<std::uint16_t>>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.size() == 2);

        auto const inner0 = deserialiser[0];
        test_assert(inner0.size() == 3);
        std::array<std::uint16_t, 3> const expected_elements0{13330, 31311, 4657};
        test_assert(std::ranges::equal(inner0.elements(), expected_elements0));

        auto const inner1 = deserialiser[1];
        test_assert(inner1.size() == 1);
        std::array<std::uint16_t, 1> const expected_elements1{13073};
        test_assert(std::ranges::equal(inner1.elements(), expected_elements1));
    }


    static_assert(FIXED_DATA_SIZE<CompoundTestStruct3> == 34);

    STEST_CASE(Serialiser_CompoundStruct) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<CompoundTestStruct3>();
        SerialiseSource<CompoundTestStruct3> const source{
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
        };
        Serialiser<CompoundTestStruct3> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 34, 34, 0, 105};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 105> const expected_buffer{
            // Fixed data
            0x03, 0x00,     // struct2.struct1.list_u16 size
            0x00, 0x00,     // struct2.struct1.list_u16 offset
            0x7B,           // struct2.struct1.u8
            0x00, 0x00,     // struct2.struct1.opt_i64 value offset
            0x07, 0x00,     // struct2.opt_struct1 value offset
            0x02, 0x00,     // struct2.list_struct1 size
            0x13, 0x00,     // struct2.list_struct1 offset
            0x01, 0x00,     // struct2.array_struct1[0].list_u16 size
            0x2B, 0x00,     // struct2.array_struct1[0].list_u16 offset
            0x01,           // struct2.array_struct1[0].u8
            0x2E, 0x00,     // struct2.array_struct1[0].opt_i64 offset
            0x00, 0x00,     // struct2.array_struct1[1].list_u16 size
            0x35, 0x00,     // struct2.array_struct1[1].list_u16 offset
            0x10,           // struct2.array_struct1[1].u8
            0x00, 0x00,     // struct2.array_struct1[1].opt_i64 offset
            0x05, 0x00,     // struct1.list_u16 size
            0x35, 0x00,     // struct1.list_u16 offset
            0x2C,           // struct1.u8
            0x40, 0x00,     // struct1.opt_i64 value offset
            // Variable data
            0xAF, 0xBE,     // struct2.struct1.list_u16 elements
            0x57, 0x70,
            0x23, 0xB6,
            0x03, 0x00,     // struct2.opt_struct1.list_u16 size
            0x0D, 0x00,     // struct2.opt_struct1.list_u16 offset
            0x94,           // struct2.opt_struct1.u8
            0x00, 0x00,     // struct2.opt_struct1.opt_i64 value offset
            0x34, 0x68,     // struct2.opt_struct1.list_u16 elements
            0x6A, 0xD2,
            0xCC, 0xE3,
            0x01, 0x00,     // struct2.list_struct1[0].list_u16 size
            0x21, 0x00,     // struct2.list_struct1[0].list_u16 offset
            0x3D,           // struct2.list_struct1[0].u8
            0x00, 0x00,     // struct2.list_struct1[0].opt_i64 value offset
            0x00, 0x00,     // struct2.list_struct1[1].list_u16 size
            0x23, 0x00,     // struct2.list_struct1[1].list_u16 offset
            0x14,           // struct2.list_struct1[1].u8
            0x24, 0x00,     // struct2.list_struct1[1].opt_i64 value offset
            0x38, 0x4D,     // struct2.list_struct1[0].list_u16 elements
            0x01, 0xA1, 0x10, 0x3D, 0x03, 0x36, 0x85, 0xFF, // struct2.list_struct1[1].opt_i64 value
            0x14, 0x00,     // struct2.array_struct1[0].list_u16 elements
            0xBD, 0xE8, 0xF2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // struct2.array_struct1[0].opt_i64 value
            0x4A, 0x28,     // struct1.list_u16 elements
            0x93, 0x14,
            0x1F, 0xDC,
            0xAD, 0x2C,
            0x8F, 0x95,
            0x1C, 0xBE, 0xA0, 0xF4, 0xE5, 0xFC, 0x05, 0x00  // struct1.opt_i64 value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_CompoundStruct) {
        std::array<unsigned char, 105> const buffer{
            // Fixed data
            0x03, 0x00,     // struct2.struct1.list_u16 size
            0x00, 0x00,     // struct2.struct1.list_u16 offset
            0x7B,           // struct2.struct1.u8
            0x00, 0x00,     // struct2.struct1.opt_i64 value offset
            0x07, 0x00,     // struct2.opt_struct1 value offset
            0x02, 0x00,     // struct2.list_struct1 size
            0x13, 0x00,     // struct2.list_struct1 offset
            0x01, 0x00,     // struct2.array_struct1[0].list_u16 size
            0x2B, 0x00,     // struct2.array_struct1[0].list_u16 offset
            0x01,           // struct2.array_struct1[0].u8
            0x2E, 0x00,     // struct2.array_struct1[0].opt_i64 offset
            0x00, 0x00,     // struct2.array_struct1[1].list_u16 size
            0x35, 0x00,     // struct2.array_struct1[1].list_u16 offset
            0x10,           // struct2.array_struct1[1].u8
            0x00, 0x00,     // struct2.array_struct1[1].opt_i64 offset
            0x05, 0x00,     // struct1.list_u16 size
            0x35, 0x00,     // struct1.list_u16 offset
            0x2C,           // struct1.u8
            0x40, 0x00,     // struct1.opt_i64 value offset
            // Variable data
            0xAF, 0xBE,     // struct2.struct1.list_u16 elements
            0x57, 0x70,
            0x23, 0xB6,
            0x03, 0x00,     // struct2.opt_struct1.list_u16 size
            0x0D, 0x00,     // struct2.opt_struct1.list_u16 offset
            0x94,           // struct2.opt_struct1.u8
            0x00, 0x00,     // struct2.opt_struct1.opt_i64 value offset
            0x34, 0x68,     // struct2.opt_struct1.list_u16 elements
            0x6A, 0xD2,
            0xCC, 0xE3,
            0x01, 0x00,     // struct2.list_struct1[0].list_u16 size
            0x21, 0x00,     // struct2.list_struct1[0].list_u16 offset
            0x3D,           // struct2.list_struct1[0].u8
            0x00, 0x00,     // struct2.list_struct1[0].opt_i64 value offset
            0x00, 0x00,     // struct2.list_struct1[1].list_u16 size
            0x23, 0x00,     // struct2.list_struct1[1].list_u16 offset
            0x14,           // struct2.list_struct1[1].u8
            0x24, 0x00,     // struct2.list_struct1[1].opt_i64 value offset
            0x38, 0x4D,     // struct2.list_struct1[0].list_u16 elements
            0x01, 0xA1, 0x10, 0x3D, 0x03, 0x36, 0x85, 0xFF, // struct2.list_struct1[1].opt_i64 value
            0x14, 0x00,     // struct2.array_struct1[0].list_u16 elements
            0xBD, 0xE8, 0xF2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // struct2.array_struct1[0].opt_i64 value
            0x4A, 0x28,     // struct1.list_u16 elements
            0x93, 0x14,
            0x1F, 0xDC,
            0xAD, 0x2C,
            0x8F, 0x95,
            0x1C, 0xBE, 0xA0, 0xF4, 0xE5, 0xFC, 0x05, 0x00  // struct1.opt_i64 value
        };
        auto const deserialiser = deserialise<CompoundTestStruct3>(as_const_bytes_view(buffer));

        auto const struct2 = deserialiser.get<"struct2">();

        auto const struct2_struct1 = struct2.get<"struct1">();
        test_assert(std::ranges::equal(struct2_struct1.get<"list_u16">().elements(), std::array{48815, 28759, 46627}));
        test_assert(struct2_struct1.get<"u8">() == 123);
        test_assert(!struct2_struct1.get<"opt_i64">().has_value());

        auto const struct2_opt_struct1 = struct2.get<"opt_struct1">();
        test_assert(struct2_opt_struct1.has_value());
        auto const struct2_opt_struct1_v = struct2_opt_struct1.value();
        test_assert(
            std::ranges::equal(struct2_opt_struct1_v.get<"list_u16">().elements(), std::array{26676, 53866, 58316}));
        test_assert(struct2_opt_struct1_v.get<"u8">() == 148);
        test_assert(!struct2_opt_struct1_v.get<"opt_i64">().has_value());

        auto const struct2_list_struct1 = struct2.get<"list_struct1">();
        test_assert(struct2_list_struct1.size() == 2);
        test_assert(std::ranges::equal(struct2_list_struct1.at(0).get<"list_u16">().elements(), std::array{19768}));
        test_assert(struct2_list_struct1.at(0).get<"u8">() == 61);
        test_assert(!struct2_list_struct1.at(0).get<"opt_i64">().has_value());
        test_assert(struct2_list_struct1.at(1).get<"list_u16">().empty());
        test_assert(struct2_list_struct1.at(1).get<"u8">() == 20);
        test_assert(struct2_list_struct1.at(1).get<"opt_i64">().has_value());
        test_assert(struct2_list_struct1.at(1).get<"opt_i64">().value() == -34'562'034'598'108'927ll);

        auto const struct2_array_struct1 = struct2.get<"array_struct1">();
        test_assert(std::ranges::equal(struct2_array_struct1.at(0).get<"list_u16">().elements(), std::array{20}));
        test_assert(struct2_array_struct1.at(0).get<"u8">() == 1);
        test_assert(struct2_array_struct1.at(0).get<"opt_i64">().has_value());
        test_assert(struct2_array_struct1.at(0).get<"opt_i64">().value() == -857923);
        test_assert(struct2_array_struct1.at(1).get<"list_u16">().empty());
        test_assert(struct2_array_struct1.at(1).get<"u8">() == 16);
        test_assert(!struct2_array_struct1.at(1).get<"opt_i64">().has_value());

        auto const struct1 = deserialiser.get<"struct1">();
        test_assert(
            std::ranges::equal(struct1.get<"list_u16">().elements(), std::array{10314, 5267, 56351, 11437, 38287}));
        test_assert(struct1.get<"u8">() == 44);
        test_assert(struct1.get<"opt_i64">().has_value());
        test_assert(struct1.get<"opt_i64">().value() == 1'685'439'465'438'748ll);
    }

}

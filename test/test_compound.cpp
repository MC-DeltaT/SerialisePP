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
        // TODO
    }

    STEST_CASE(Deserialiser_OptionalOfOptional) {
        std::array<unsigned char, 8> const buffer{
            0x01, 0x00,             // Outer optional value offset
            0x03, 0x00,             // Inner optional value offset
            0xC2, 0x5F, 0x02, 0x8E  // Inner optional value
        };
        auto const deserialiser = deserialise<Optional<Optional<std::int32_t>>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.has_value());
        auto const inner_deserialiser = deserialiser.value();
        test_assert(inner_deserialiser.has_value());
        test_assert(inner_deserialiser.value() == -1'912'447'038);
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

    STEST_CASE(Deserialise_ListOfList) {
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


    static_assert(FIXED_DATA_SIZE<CompoundTestStruct3> == 20);

    STEST_CASE(Serialiser_CompoundStruct) {
        // TODO
    }

    STEST_CASE(Deserialiser_CompoundStruct) {
        // TODO
    }

}

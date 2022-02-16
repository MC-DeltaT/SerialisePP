#include <array>
#include <span>

#include <serialpp/common.hpp>
#include <serialpp/struct.hpp>

#include "helpers/common.hpp"
#include "helpers/struct.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    static_assert(FIXED_DATA_SIZE<TestStruct> == 15);

    STEST_CASE(SerialiseSource_Struct) {
        SerialiseSource<TestStruct> source{std::int8_t{1}, std::uint32_t{2u}, std::int16_t{3}, std::uint64_t{4u}};
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
    }

    STEST_CASE(Serialiser_Struct) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<TestStruct>();
        SerialiseSource<TestStruct> const source{
            std::int8_t{-34},
            std::uint32_t{206'000u},
            std::int16_t{36},
            std::uint64_t{360'720u}
        };
        Serialiser<TestStruct> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_target{buffer, 15, 15, 0, 15};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 15> const expected_buffer{
            0xDE,                   // a
            0xB0, 0x24, 0x03, 0x00, // foo
            0x24, 0x00,             // my field
            0x10, 0x81, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00  // qux
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Struct) {
        std::array<unsigned char, 15> const buffer{
            0x9C,                   // a
            0x15, 0xCD, 0x5B, 0x07, // foo
            0x30, 0x75,             // my field
            0xFF, 0xE7, 0x76, 0x48, 0x17, 0x00, 0x00, 0x00  // qux
        };
        auto const deserialiser = deserialise<TestStruct>(std::as_bytes(std::span{buffer}));
        test_assert(deserialiser.get<"a">() == -100);
        test_assert(deserialiser.get<"foo">() == 123'456'789u);
        test_assert(deserialiser.get<"my field">() == 30000);
        test_assert(deserialiser.get<"qux">() == 99'999'999'999ull);
    }

}

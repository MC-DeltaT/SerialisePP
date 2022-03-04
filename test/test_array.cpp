#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <serialpp/array.hpp>
#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    // TODO: testing with MockSerialisable?

    static_assert(FIXED_DATA_SIZE<Array<std::int32_t, 17>> == 68);
    static_assert(FIXED_DATA_SIZE<Array<std::uint64_t, 0>> == 0);

    STEST_CASE(Serialiser_Array_Empty) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Array<std::uint64_t, 0>>();
        SerialiseSource<Array<std::uint64_t, 0>> const source{};
        Serialiser<Array<std::uint64_t, 0>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 0, 0, 0, 0};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Serialiser_Array_Nonempty) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Array<std::uint16_t, 5>>();
        SerialiseSource<Array<std::uint16_t, 5>> const source{12, 45, 465, 24643, 674};
        Serialiser<Array<std::uint16_t, 5>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 10, 10, 0, 10};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 10> const expected_buffer{
            0x0C, 0x00,     // element 0
            0x2D, 0x00,     // element 1
            0xD1, 0x01,     // element 2
            0x43, 0x60,     // element 3
            0xA2, 0x02      // element 4
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Array_Empty) {
        std::array<unsigned char, 0> const buffer{};
        auto const deserialiser = deserialise<Array<char, 0>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.size() == 0);
        test_assert(deserialiser.elements().empty());
        for (std::size_t i = 0; i < 100; ++i) {
            test_assert_throws<std::out_of_range>([&deserialiser, i] {
                (void)deserialiser.at(i);
            });
        }
    }

    STEST_CASE(Deserialiser_Array_Nonempty) {
        std::array<unsigned char, 12> const buffer{
            0xF0, 0x0E, 0xC3, 0x45,     // element 0
            0xC6, 0x4C, 0xD7, 0x9E,     // element 1
            0x01, 0x00, 0x00, 0x32      // element 2
        };
        auto const deserialiser = deserialise<Array<std::int32_t, 3>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.size() == 3);
        std::array<std::int32_t, 3> const expected_elements{1'170'411'248, -1'630'057'274, 838'860'801};
        test_assert(std::ranges::equal(deserialiser.elements(), expected_elements));
        test_assert(deserialiser[0] == 1'170'411'248);
        test_assert(deserialiser.at(0) == 1'170'411'248);
        test_assert(deserialiser[1] == -1'630'057'274);
        test_assert(deserialiser.at(1) == -1'630'057'274);
        test_assert(deserialiser[2] == 838'860'801);
        test_assert(deserialiser.at(2) == 838'860'801);
        auto const [element0, element1, element2] = deserialiser;
        test_assert(element0 == 1'170'411'248);
        test_assert(element1 == -1'630'057'274);
        test_assert(element2 == 838'860'801);
        for (std::size_t i = 3; i < 100; ++i) {
            test_assert_throws<std::out_of_range>([&deserialiser, i] {
                (void)deserialiser.at(i);
            });
        }
    }

}

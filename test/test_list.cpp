#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include <serialpp/common.hpp>
#include <serialpp/list.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"
#include "helpers/utility.hpp"


namespace serialpp::test {

    // TODO: testing with MockSerialisable?

    static_assert(FIXED_DATA_SIZE<List<std::int8_t>> == 2 + 2);
    static_assert(FIXED_DATA_SIZE<List<MockSerialisable<1000>>> == 2 + 2);

    STEST_CASE(SerialiseSource_List_DefaultConstruct) {
        SerialiseSource<List<int>> const source{};
    }

    STEST_CASE(SerialiseSource_List_BracedInit) {
        SerialiseSource<List<long>> const source1{{1}};
        SerialiseSource<List<std::uint8_t>> const source2{{1, 2u, 'c'}};    // Small range optimisation
        SerialiseSource<List<std::uint64_t>> const source3{{1, 2, 3, 4, 5, 6, 7, 8}};   // Large range
    }

    STEST_CASE(SerialiseSource_List_BorrowSmallRange) {
        std::vector<int> const v;
        auto const r = std::ranges::ref_view(v);
        SerialiseSource<List<long>> const source{r};
    }

    STEST_CASE(SerialiseSource_List_BorrowLargeRange) {
        LifecycleData lifecycle;
        std::array<LifecycleObserver<SerialiseSource<int>>, 4> range{
            {{lifecycle, 1}, {lifecycle, 2}, {lifecycle, 3}, {lifecycle, 4}}};
        SerialiseSource<List<int>> const source{range};
        test_assert(lifecycle == LifecycleData{.constructs = 4});
    }

    STEST_CASE(SerialiseSource_List_OwnSmallRange) {
        std::vector<int> const v;
        auto r = std::ranges::ref_view(v);
        SerialiseSource<List<long>> const source{std::move(r)};
    }

    STEST_CASE(SerialiseSource_List_OwnLargeRange) {
        LifecycleData lifecycle;
        std::array<LifecycleObserver<SerialiseSource<int>>, 4> range{
            {{lifecycle, 1}, {lifecycle, 2}, {lifecycle, 3}, {lifecycle, 4}}};
        SerialiseSource<List<int>> const source{std::move(range)};
        test_assert(lifecycle == LifecycleData{.constructs = 4, .move_constructs = 4});
    }

    STEST_CASE(SerialiseSource_List_OwnEmptyRange) {
        std::vector<int> v;
        SerialiseSource<List<long>> const source{std::move(v)};
    }

    STEST_CASE(Serialiser_List_Empty) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<List<std::uint64_t>>();
        SerialiseSource<List<std::uint64_t>> const source{std::ranges::views::empty<std::uint64_t>};
        Serialiser<List<std::uint64_t>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 4, 4, 0, 4};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 4> const expected_buffer{
            0x00, 0x00,     // Size
            0x00, 0x00      // Offset
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Serialiser_List_Nonempty) {
        SerialiseBuffer buffer;
        buffer.extend(14);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[4 + i] = static_cast<std::byte>(i + 1);
        }
        SerialiseTarget const target{buffer, 4, 0, 4, 14};
        SerialiseSource<List<std::uint32_t>> const source{{
            23, 67'456'534, 0, 345'342, 456, 4356, 3, 7567, 2'532'865'138,
        }};
        Serialiser<List<std::uint32_t>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 4, 4, 0, 50};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 50> const expected_buffer{
            0x09, 0x00,     // Size
            0x0A, 0x00,     // Offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x17, 0x00, 0x00, 0x00,     // Elements
            0x16, 0x4E, 0x05, 0x04,
            0x00, 0x00, 0x00, 0x00,
            0xFE, 0x44, 0x05, 0x00,
            0xC8, 0x01, 0x00, 0x00,
            0x04, 0x11, 0x00, 0x00,
            0x03, 0x00, 0x00, 0x00,
            0x8F, 0x1D, 0x00, 0x00,
            0x72, 0x74, 0xF8, 0x96,
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_List_Empty) {
        std::array<unsigned char, 4> const buffer{
            0x00, 0x00,     // Size
            0x00, 0x00      // Offset
        };
        auto const deserialiser = deserialise<List<std::int32_t>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.size() == 0);
        test_assert(deserialiser.empty());
        test_assert(deserialiser.elements().empty());
        for (std::size_t i = 0; i < 100; ++i) {
            test_assert_throws<std::out_of_range>([&deserialiser, i] {
                (void)deserialiser.at(i);
            });
        }
    }

    STEST_CASE(Deserialiser_List_Nonempty) {
        std::array<unsigned char, 20> const buffer{
            0x05, 0x00,     // Size
            0x06, 0x00,     // Offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06,     // Dummy padding
            0x74, 0xC1,     // Elements
            0x99, 0x5C,
            0x6E, 0x64,
            0x36, 0xD1,
            0x71, 0xDA
        };
        auto const deserialiser = deserialise<List<std::uint16_t>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.size() == 5);
        test_assert(!deserialiser.empty());
        std::array<std::uint16_t, 5> const expected_elements{49524, 23705, 25710, 53558, 55921};
        test_assert(std::ranges::equal(deserialiser.elements(), expected_elements));
        test_assert(deserialiser[0] == 49524);
        test_assert(deserialiser[1] == 23705);
        test_assert(deserialiser[2] == 25710);
        test_assert(deserialiser[3] == 53558);
        test_assert(deserialiser[4] == 55921);
        test_assert(deserialiser.at(0) == 49524);
        test_assert(deserialiser.at(1) == 23705);
        test_assert(deserialiser.at(2) == 25710);
        test_assert(deserialiser.at(3) == 53558);
        test_assert(deserialiser.at(4) == 55921);
        for (std::size_t i = 5; i < 100; ++i) {
            test_assert_throws<std::out_of_range>([&deserialiser, i] {
                (void)deserialiser.at(i);
            });
        }
    }

    STEST_CASE(Deserialiser_List_OffsetOutOfRange) {
        std::array<unsigned char, 20> const buffer{
            0x05, 0x00,     // Size
            0x10, 0x00,     // Offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06,     // Dummy padding
            0x74, 0xC1,     // Elements
            0x99, 0x5C,
            0x6E, 0x64,
            0x36, 0xD1,
            0x71, 0xDA
        };
        auto const deserialiser = deserialise<List<std::uint16_t>>(as_const_bytes_view(buffer));
        for (std::size_t i = 0; i < 5; ++i) {
            test_assert_throws<VariableBufferSizeError>([&deserialiser, i] {
                (void)deserialiser[i];
            });
        }
        test_assert_throws<VariableBufferSizeError>([&deserialiser] {
            std::ranges::for_each(deserialiser.elements(), [](auto) {});
        });
    }

}

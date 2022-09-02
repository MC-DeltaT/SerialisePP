#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>
#include <serialpp/dynamic_array.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/lifecycle.hpp"
#include "helpers/mock_serialisable.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block dynamic_array_tests = [] {

    test_case("to_dynamic_array_size() valid") = [] {
        test_assert(detail::to_dynamic_array_size(1'000'000u) == 1'000'000u);
    };

    test_case("to_dynamic_array_size() invalid") = [] {
        test_assert_throws<object_size_error>([] {
            (void)detail::to_dynamic_array_size(1'000'000'000'000ull);
        });
    };


    static_assert(variable_size_serialisable<dynamic_array<char>>);
    static_assert(variable_size_serialisable<dynamic_array<mock_serialisable<10, true>>>);
    static_assert(fixed_data_size_v<dynamic_array<std::int8_t>> == 4 + 4);
    static_assert(fixed_data_size_v<dynamic_array<mock_serialisable<1000>>> == 4 + 4);

    test_case("serialise_source dynamic_array default construct") = [] {
        serialise_source<dynamic_array<int>> const source;
    };

    test_case("serialise_source dynamic_array default construct constexpr") = [] {
        []() consteval {
            serialise_source<dynamic_array<int>> const source;
        }();
    };

    test_case("serialise_source dynamic_array braced init") = [] {
        serialise_source<dynamic_array<long>> const source1{{1}};
        serialise_source<dynamic_array<std::uint8_t>> const source2{{1, 2u, 'c'}};    // Small range optimisation
        serialise_source<dynamic_array<std::uint64_t>> const source3{{1, 2, 3, 4, 5, 6, 7, 8}};   // Large range
    };

    test_case("serialise_source dynamic_array braced init constexpr") = [] {
        []() consteval {
            serialise_source<dynamic_array<long>> const source1{{1}};
            serialise_source<dynamic_array<std::uint8_t>> const source2{{1, 2u, 'c'}};    // Small range optimisation
            serialise_source<dynamic_array<std::uint64_t>> const source3{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};  // Large range
        }();
    };

    test_case("serialise_source dynamic_array borrow small range") = [] {
        std::vector<int> const v;
        auto const r = std::ranges::ref_view(v);
        serialise_source<dynamic_array<long>> const source{r};
    };

    test_case("serialise_source dynamic_array borrow small range constexpr") = [] {
        []() consteval {
            std::vector<int> const v;
            auto const r = std::ranges::ref_view(v);
            serialise_source<dynamic_array<long>> const source{r};
        }();
    };

    test_case("serialise_source dynamic_array borrow large range") = [] {
        lifecycle_data lifecycle;
        std::array<lifecycle_observer<serialise_source<int>>, 4> range{
            {{lifecycle, 1}, {lifecycle, 2}, {lifecycle, 3}, {lifecycle, 4}}};
        serialise_source<dynamic_array<int>> const source{range};
        test_assert(lifecycle == lifecycle_data{.constructs = 4});
    };

    test_case("serialise_source dynamic_array borrow large range constexpr") = [] {
        []() consteval {
            lifecycle_data lifecycle;
            std::array<lifecycle_observer<serialise_source<int>>, 4> range{
                {{lifecycle, 1}, {lifecycle, 2}, {lifecycle, 3}, {lifecycle, 4}}};
            serialise_source<dynamic_array<int>> const source{range};
            test_assert(lifecycle == lifecycle_data{.constructs = 4});
        }();
    };

    test_case("serialise_source dynamic_array own small range") = [] {
        std::vector<int> const v;
        auto r = std::ranges::ref_view(v);
        serialise_source<dynamic_array<long>> const source{std::move(r)};
    };

    test_case("serialise_source dynamic_array own small range constexpr") = [] {
        []() consteval {
            std::vector<int> const v;
            auto r = std::ranges::ref_view(v);
            serialise_source<dynamic_array<long>> const source{std::move(r)};
        }();
    };

    test_case("serialise_source dynamic_array own large range") = [] {
        lifecycle_data lifecycle;
        std::array<lifecycle_observer<serialise_source<int>>, 4> range{
            {{lifecycle, 1}, {lifecycle, 2}, {lifecycle, 3}, {lifecycle, 4}}};
        serialise_source<dynamic_array<int>> const source{std::move(range)};
        test_assert(lifecycle == lifecycle_data{.constructs = 4, .move_constructs = 4});
    };

    test_case("serialise_source dynamic_array own large range constexpr") = [] {
        []() consteval {
            lifecycle_data lifecycle;
            std::array<lifecycle_observer<serialise_source<int>>, 4> range{
                {{lifecycle, 1}, {lifecycle, 2}, {lifecycle, 3}, {lifecycle, 4}}};
            serialise_source<dynamic_array<int>> const source{std::move(range)};
            test_assert(lifecycle == lifecycle_data{.constructs = 4, .move_constructs = 4});
        }();
    };

    test_case("serialise_source dynamic_array own empty range") = [] {
        std::vector<int> v;
        serialise_source<dynamic_array<long>> const source{std::move(v)};
    };

    test_case("serialise_source dynamic_array own empty range constexpr") = [] {
        []() consteval {
            std::vector<int> v;
            serialise_source<dynamic_array<long>> const source{std::move(v)};
        }();
    };

    test_case("serialiser dynamic_array empty") = [] {
        using type = dynamic_array<std::uint64_t>;
        basic_buffer buffer;
        buffer.initialise(8);
        serialise_source<type> const source{std::ranges::views::empty<std::uint64_t>};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 8> const expected_buffer{
            0x00, 0x00, 0x00, 0x00,     // Size
            0x00, 0x00, 0x00, 0x00      // Offset
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    // test_case("serialiser dynamic_array mock nonempty") = [] {
        // TODO: how to get data from serialise_source?
    // };

    test_case("serialiser dynamic_array scalar nonempty small range") = [] {
        using type = dynamic_array<std::uint32_t>;
        basic_buffer buffer;
        buffer.initialise(18);
        for (unsigned i = 8; i < 18; ++i) {
            buffer.span()[i] = static_cast<std::byte>(i - 7);
        }
        std::vector<long long> const elements{23, 67'456'534, 0, 345'342, 456, 4356, 3, 7567, 2'532'865'138};
        serialise_source<type> const source{elements};  // viewed as std::ranges::views::all
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 54> const expected_buffer{
            0x09, 0x00, 0x00, 0x00,     // Size
            0x12, 0x00, 0x00, 0x00,     // Offset
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
    };

    // TODO: enable when compilers aren't broken
    // test_case("serialiser dynamic_array scalar nonempty small range constexpr") = [] {
    //     []() consteval {
    //         using type = dynamic_array<std::uint32_t>;
    //         basic_buffer buffer;
    //         buffer.initialise(18);
    //         for (unsigned i = 8; i < 18; ++i) {
    //             buffer.span()[i] = static_cast<std::byte>(i - 7);
    //         }
    //         std::vector<long long> const elements{23, 67'456'534, 0, 345'342, 456, 4356, 3, 7567, 2'532'865'138};
    //         serialise_source<type> const source{elements};  // viewed as std::ranges::views::all
    //         serialiser<type>::serialise(source, buffer, 0);

    //         std::array<unsigned char, 54> const expected_buffer{
    //             0x09, 0x00, 0x00, 0x00,     // Size
    //             0x12, 0x00, 0x00, 0x00,     // Offset
    //             0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
    //             0x17, 0x00, 0x00, 0x00,     // Elements
    //             0x16, 0x4E, 0x05, 0x04,
    //             0x00, 0x00, 0x00, 0x00,
    //             0xFE, 0x44, 0x05, 0x00,
    //             0xC8, 0x01, 0x00, 0x00,
    //             0x04, 0x11, 0x00, 0x00,
    //             0x03, 0x00, 0x00, 0x00,
    //             0x8F, 0x1D, 0x00, 0x00,
    //             0x72, 0x74, 0xF8, 0x96,
    //         };
    //         test_assert(buffer_equal(buffer, expected_buffer));
    //     }();
    // };

    test_case("serialiser dynamic_array scalar nonempty large range") = [] {
        using type = dynamic_array<std::uint32_t>;
        basic_buffer buffer;
        buffer.initialise(18);
        for (unsigned i = 8; i < 18; ++i) {
            buffer.span()[i] = static_cast<std::byte>(i - 7);
        }
        serialise_source<type> const source{{
            23, 67'456'534, 0, 345'342, 456, 4356, 3, 7567, 2'532'865'138
        }};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 54> const expected_buffer{
            0x09, 0x00, 0x00, 0x00,     // Size
            0x12, 0x00, 0x00, 0x00,     // Offset
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
    };

    // TODO: enable when compilers aren't broken
    // test_case("serialiser dynamic_array scalar nonempty large range constexpr") = [] {
    //     []() consteval {
    //         using type = dynamic_array<std::uint32_t>;
    //         basic_buffer buffer;
    //         buffer.initialise(18);
    //         for (unsigned i = 8; i < 18; ++i) {
    //             buffer.span()[i] = static_cast<std::byte>(i - 7);
    //         }
    //         serialise_source<type> const source{{
    //             23, 67'456'534, 0, 345'342, 456, 4356, 3, 7567, 2'532'865'138
    //         }};
    //         serialiser<type>::serialise(source, buffer, 0);

    //         std::array<unsigned char, 54> const expected_buffer{
    //             0x09, 0x00, 0x00, 0x00,     // Size
    //             0x12, 0x00, 0x00, 0x00,     // Offset
    //             0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
    //             0x17, 0x00, 0x00, 0x00,     // Elements
    //             0x16, 0x4E, 0x05, 0x04,
    //             0x00, 0x00, 0x00, 0x00,
    //             0xFE, 0x44, 0x05, 0x00,
    //             0xC8, 0x01, 0x00, 0x00,
    //             0x04, 0x11, 0x00, 0x00,
    //             0x03, 0x00, 0x00, 0x00,
    //             0x8F, 0x1D, 0x00, 0x00,
    //             0x72, 0x74, 0xF8, 0x96,
    //         };
    //         test_assert(buffer_equal(buffer, expected_buffer));
    //     }();
    // };

    test_case("deserialiser dynamic_array empty") = [] {
        std::array<unsigned char, 8> const buffer{
            0x00, 0x00, 0x00, 0x00,     // Size
            0x00, 0x00, 0x00, 0x00      // Offset
        };
        using type = dynamic_array<std::int32_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.size() == 0);
        test_assert(deser.empty());
        test_assert(deser.elements().empty());
        for (std::size_t i = 0; i < 100; ++i) {
            test_assert_throws<std::out_of_range>([&deser, i] {
                (void)deser.at(i);
            });
        }
    };

    test_case("deserialiser dynamic_array mock nonempty") = [] {
        std::array<unsigned char, 50> const buffer{
            0x04, 0x00, 0x00, 0x00,     // Size
            0x10, 0x00, 0x00, 0x00      // Offset
        };
        auto const buffer_span = as_const_bytes_span(buffer);
        using type = dynamic_array<mock_serialisable<5>>;
        deserialiser<type> const deser{buffer_span, 0};
        test_assert(deser.size() == 4);
        test_assert(bytes_span_same(deser.at(0)._buffer, buffer_span));
        test_assert(deser.at(0)._fixed_offset == 16);
        test_assert(bytes_span_same(deser.at(1)._buffer, buffer_span));
        test_assert(deser.at(1)._fixed_offset == 21);
        test_assert(bytes_span_same(deser.at(2)._buffer, buffer_span));
        test_assert(deser.at(2)._fixed_offset == 26);
        test_assert(bytes_span_same(deser.at(3)._buffer, buffer_span));
        test_assert(deser.at(3)._fixed_offset == 31);

    };

    test_case("deserialiser dynamic_array scalar nonempty") = [] {
        std::array<unsigned char, 24> const buffer{
            0x05, 0x00, 0x00, 0x00,     // Size
            0x0E, 0x00, 0x00, 0x00,     // Offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06,     // Dummy padding
            0x74, 0xC1,     // Elements
            0x99, 0x5C,
            0x6E, 0x64,
            0x36, 0xD1,
            0x71, 0xDA
        };
        using type = dynamic_array<std::uint16_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.size() == 5);
        test_assert(!deser.empty());
        std::array<std::uint16_t, 5> const expected_elements{49524, 23705, 25710, 53558, 55921};
        test_assert(std::ranges::equal(deser.elements(), expected_elements));
        test_assert(deser[0] == 49524);
        test_assert(deser[1] == 23705);
        test_assert(deser[2] == 25710);
        test_assert(deser[3] == 53558);
        test_assert(deser[4] == 55921);
        test_assert(deser.at(0) == 49524);
        test_assert(deser.at(1) == 23705);
        test_assert(deser.at(2) == 25710);
        test_assert(deser.at(3) == 53558);
        test_assert(deser.at(4) == 55921);
        for (std::size_t i = 5; i < 100; ++i) {
            test_assert_throws<std::out_of_range>([&deser, i] {
                (void)deser.at(i);
            });
        }
    };

    test_case("deserialiser dynamic_array scalar nonempty constexpr") = [] {
        []() consteval {
            std::array<unsigned char, 24> const buffer{
                0x05, 0x00, 0x00, 0x00,     // Size
                0x0E, 0x00, 0x00, 0x00,     // Offset
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06,     // Dummy padding
                0x74, 0xC1,     // Elements
                0x99, 0x5C,
                0x6E, 0x64,
                0x36, 0xD1,
                0x71, 0xDA
            };
            using type = dynamic_array<std::uint16_t>;
            auto const buffer_bytes = uchar_array_to_bytes(buffer);
            deserialiser<type> const deser{const_bytes_span{buffer_bytes}, 0};
            test_assert(deser.size() == 5);
            test_assert(!deser.empty());
            std::array<std::uint16_t, 5> const expected_elements{49524, 23705, 25710, 53558, 55921};
            test_assert(std::ranges::equal(deser.elements(), expected_elements));
            test_assert(deser[0] == 49524);
            test_assert(deser[1] == 23705);
            test_assert(deser[2] == 25710);
            test_assert(deser[3] == 53558);
            test_assert(deser[4] == 55921);
            test_assert(deser.at(0) == 49524);
            test_assert(deser.at(1) == 23705);
            test_assert(deser.at(2) == 25710);
            test_assert(deser.at(3) == 53558);
            test_assert(deser.at(4) == 55921);
        }();
    };

    test_case("deserialiser dynamic_array offset out of bounds") = [] {
        std::array<unsigned char, 24> const buffer{
            0x05, 0x00, 0x00, 0x00,     // Size
            0xC3, 0x01, 0x00, 0x00,     // Offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06,     // Dummy padding
            0x74, 0xC1,     // Elements
            0x99, 0x5C,
            0x6E, 0x64,
            0x36, 0xD1,
            0x71, 0xDA
        };
        using type = dynamic_array<std::uint16_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        for (std::size_t i = 0; i < 5; ++i) {
            test_assert_throws<buffer_bounds_error>([&deser, i] {
                (void)deser[i];
            });
        }
        test_assert_throws<buffer_bounds_error>([&deser] {
            std::ranges::for_each(deser.elements(), [](std::uint16_t) {});
        });
    };

    test_case("deserialiser dynamic_array element partially out of bounds") = [] {
        std::array<unsigned char, 24> const buffer{
            0x05, 0x00, 0x00, 0x00,     // Size
            0x14, 0x00, 0x00, 0x00,     // Offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06,     // Dummy padding
            0x74, 0xC1,     // Elements
            0x99, 0x5C,
            0x6E, 0x64,
            0x36, 0xD1,
            0x71, 0xDA
        };
        using type = dynamic_array<std::uint16_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        for (std::size_t i = 0; i < 2; ++i) {
            (void)deser[i];
        }
        for (std::size_t i = 2; i < 5; ++i) {
            test_assert_throws<buffer_bounds_error>([&deser, i] {
                (void)deser[i];
            });
        }
        test_assert_throws<buffer_bounds_error>([&deser] {
            std::ranges::for_each(deser.elements(), [](std::uint16_t) {});
        });
    };

};
}

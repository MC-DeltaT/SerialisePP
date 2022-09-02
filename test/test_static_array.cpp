#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/static_array.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/mock_serialisable.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block static_array_tests = [] {

    static_assert(fixed_size_serialisable<static_array<mock_serialisable<345, false>, 73>>);
    static_assert(fixed_size_serialisable<static_array<mock_serialisable<10, true>, 0>>);
    static_assert(variable_size_serialisable<static_array<mock_serialisable<12, true>, 3>>);
    static_assert(fixed_data_size_v<static_array<mock_serialisable<4>, 17>> == 68);
    static_assert(fixed_data_size_v<static_array<mock_serialisable<8>, 0>> == 0);

    static_assert(std::semiregular<serialise_source<static_array<std::int32_t, 0>>>);
    static_assert(std::semiregular<serialise_source<static_array<std::int32_t, 5>>>);

    test_case("serialiser static_array empty") = [] {
        using type = static_array<std::uint64_t, 0>;
        basic_buffer buffer;
        buffer.initialise(0);
        serialise_source<type> const source;
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser static_array mock nonempty") = [] {
        using type = static_array<mock_serialisable<6>, 5>;
        basic_buffer buffer;
        buffer.initialise(30);
        serialise_source<type> const source;
        serialiser<type>::serialise(source, buffer, 0);

        test_assert(source.elements[0].buffers == std::vector{&buffer});
        test_assert(source.elements[0].fixed_offsets == std::vector<std::size_t>{0});
        test_assert(source.elements[1].buffers == std::vector{&buffer});
        test_assert(source.elements[1].fixed_offsets == std::vector<std::size_t>{6});
        test_assert(source.elements[2].buffers == std::vector{&buffer});
        test_assert(source.elements[2].fixed_offsets == std::vector<std::size_t>{12});
        test_assert(source.elements[3].buffers == std::vector{&buffer});
        test_assert(source.elements[3].fixed_offsets == std::vector<std::size_t>{18});
        test_assert(source.elements[4].buffers == std::vector{&buffer});
        test_assert(source.elements[4].fixed_offsets == std::vector<std::size_t>{24});
    };

    test_case("serialiser static_array scalar nonempty") = [] {
        using type = static_array<std::uint16_t, 5>;
        basic_buffer buffer;
        buffer.initialise(10);
        serialise_source<type> const source{12, 45, 465, 24643, 674};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 10> const expected_buffer{
            0x0C, 0x00,     // element 0
            0x2D, 0x00,     // element 1
            0xD1, 0x01,     // element 2
            0x43, 0x60,     // element 3
            0xA2, 0x02      // element 4
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser static_array empty") = [] {
        std::array<unsigned char, 0> const buffer{};
        using type = static_array<char, 0>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.size() == 0);
        test_assert(deser.elements().empty());
        for (std::size_t i = 0; i < 100; ++i) {
            test_assert_throws<std::out_of_range>([&deser, i] {
                (void)deser.at(i);
            });
        }
    };

    test_case("deserialiser static_array mock nonempty") = [] {
        std::array<std::byte, 550> const buffer{};
        using type = static_array<mock_serialisable<135>, 4>;
        deserialiser<type> const deser{const_bytes_span{buffer}, 0};
        test_assert(bytes_span_same(deser.get<0>()._buffer, const_bytes_span{buffer}));
        test_assert(deser.get<0>()._fixed_offset == 0);
        test_assert(bytes_span_same(deser.get<1>()._buffer, const_bytes_span{buffer}));
        test_assert(deser.get<1>()._fixed_offset == 135);
        test_assert(bytes_span_same(deser.get<2>()._buffer, const_bytes_span{buffer}));
        test_assert(deser.get<2>()._fixed_offset == 270);
        test_assert(bytes_span_same(deser.get<3>()._buffer, const_bytes_span{buffer}));
        test_assert(deser.get<3>()._fixed_offset == 405);
    };

    test_case("deserialiser static_array scalar nonempty") = [] {
        std::array<unsigned char, 12> const buffer{
            0xF0, 0x0E, 0xC3, 0x45,     // element 0
            0xC6, 0x4C, 0xD7, 0x9E,     // element 1
            0x01, 0x00, 0x00, 0x32      // element 2
        };
        using type = static_array<std::int32_t, 3>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.size() == 3);
        std::array<std::int32_t, 3> const expected_elements{1'170'411'248, -1'630'057'274, 838'860'801};
        test_assert(std::ranges::equal(deser.elements(), expected_elements));
        test_assert(deser[0] == 1'170'411'248);
        test_assert(deser.at(0) == 1'170'411'248);
        test_assert(deser[1] == -1'630'057'274);
        test_assert(deser.at(1) == -1'630'057'274);
        test_assert(deser[2] == 838'860'801);
        test_assert(deser.at(2) == 838'860'801);
        auto const [element0, element1, element2] = deser;
        test_assert(element0 == 1'170'411'248);
        test_assert(element1 == -1'630'057'274);
        test_assert(element2 == 838'860'801);
        for (std::size_t i = 3; i < 100; ++i) {
            test_assert_throws<std::out_of_range>([&deser, i] {
                (void)deser.at(i);
            });
        }
    };

};
}

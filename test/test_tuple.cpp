#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/tuple.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/mock_serialisable.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block tuple_tests = [] {

    static_assert(fixed_size_serialisable<tuple<>>);
    static_assert(fixed_size_serialisable<tuple<unsigned, mock_serialisable<32, false>, std::int16_t>>);
    static_assert(variable_size_serialisable<tuple<int, char, mock_serialisable<1, true>>>);
    static_assert(fixed_data_size_v<tuple<>> == 0);
    static_assert(fixed_data_size_v<tuple<std::uint32_t, std::byte, mock_serialisable<14>>> == 19);

    static_assert(std::semiregular<serialise_source<tuple<>>>);
    static_assert(std::semiregular<serialise_source<tuple<std::uint16_t, std::int16_t>>>);

    test_case("serialiser tuple empty") = [] {
        basic_buffer buffer;
        buffer.initialise(0);
        serialise_source<tuple<>> const source;
        serialiser<tuple<>>::serialise(source, buffer, 0);
        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser tuple mocks") = [] {
        using type = tuple<mock_serialisable<14>, mock_serialisable<6>, mock_serialisable<8>>;
        basic_buffer buffer;
        buffer.initialise(28);
        serialise_source<type> const source;
        serialiser<type>::serialise(source, buffer, 0);

        test_assert(std::get<0>(source).buffers == std::vector{&buffer});
        test_assert(std::get<0>(source).fixed_offsets == std::vector<std::size_t>{0});
        test_assert(std::get<1>(source).buffers == std::vector{&buffer});
        test_assert(std::get<1>(source).fixed_offsets == std::vector<std::size_t>{14});
        test_assert(std::get<2>(source).buffers == std::vector{&buffer});
        test_assert(std::get<2>(source).fixed_offsets == std::vector<std::size_t>{20});
    };

    test_case("serialiser tuple scalars") = [] {
        using type = tuple<std::uint8_t, std::uint8_t, std::int32_t>;
        basic_buffer buffer;
        buffer.initialise(6);
        serialise_source<type> const source{86, 174, 23'476'598};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 6> const expected_buffer{
            0x56,           // element 0
            0xAE,           // element 1
            0x76, 0x39, 0x66, 0x01  // element 2
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser tuple empty") = [] {
        std::array<std::byte, 0> const buffer{};
        deserialiser<tuple<>> const deser{const_bytes_span{buffer}, 0};
    };

    test_case("deserialiser tuple mocks") = [] {
        using type = tuple<mock_serialisable<24>, mock_serialisable<58>, mock_serialisable<8>, mock_serialisable<69>>;
        std::array<std::byte, 216> const buffer{};
        deserialiser<type> const deser{const_bytes_span{buffer}, 0};
        test_assert(bytes_span_same(deser.get<0>()._buffer, const_bytes_span{buffer}));
        test_assert(deser.get<0>()._fixed_offset == 0);
        test_assert(bytes_span_same(deser.get<1>()._buffer, const_bytes_span{buffer}));
        test_assert(deser.get<1>()._fixed_offset == 24);
        test_assert(bytes_span_same(deser.get<2>()._buffer, const_bytes_span{buffer}));
        test_assert(deser.get<2>()._fixed_offset == 82);
        test_assert(bytes_span_same(deser.get<3>()._buffer, const_bytes_span{buffer}));
        test_assert(deser.get<3>()._fixed_offset == 90);
    };

    test_case("deserialiser tuple scalars") = [] {
        std::array<unsigned char, 14> const buffer{
            0xDA, 0xC1, 0x24, 0x16, 0x00, 0x00, 0x05, 0x9D,     // element 0
            0xDD, 0x52,                 // element 1
            0x37, 0x45, 0x0A, 0x5B      // element 2
        };
        using type = tuple<std::int64_t, std::int16_t, std::uint32_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.get<0>() == -7'132'294'434'499'804'710ll);
        test_assert(deser.get<1>() == 21213);
        test_assert(deser.get<2>() == 1'527'399'735);
        auto const [element0, element1, element2] = deser;
        test_assert(element0 == -7'132'294'434'499'804'710ll);
        test_assert(element1 == 21213);
        test_assert(element2 == 1'527'399'735);
    };

};
}

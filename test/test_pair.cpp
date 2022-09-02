#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>
#include <serialpp/pair.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/mock_serialisable.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block pair_tests = [] {

    static_assert(fixed_size_serialisable<pair<mock_serialisable<3, false>, mock_serialisable<8, false>>>);
    static_assert(variable_size_serialisable<pair<mock_serialisable<90, false>, mock_serialisable<16, true>>>);
    static_assert(variable_size_serialisable<pair<mock_serialisable<90, true>, mock_serialisable<16, false>>>);
    static_assert(fixed_data_size_v<pair<mock_serialisable<3, false>, mock_serialisable<8, false>>> == 11);

    static_assert(std::semiregular<serialise_source<pair<std::byte, std::uint64_t>>>);

    test_case("serialiser pair mocks") = [] {
        using type = pair<mock_serialisable<20>, mock_serialisable<12>>;
        basic_buffer buffer;
        buffer.initialise(32);
        serialise_source<type> const source;
        serialiser<type>::serialise(source, buffer, 0);

        test_assert(source.first.buffers == std::vector{&buffer});
        test_assert(source.first.fixed_offsets == std::vector<std::size_t>{0});
        test_assert(source.second.buffers == std::vector{&buffer});
        test_assert(source.second.fixed_offsets == std::vector<std::size_t>{20});
    };

    test_case("serialiser pair scalars") = [] {
        using type = pair<std::int32_t, std::uint16_t>;
        basic_buffer buffer;
        buffer.initialise(6);
        serialise_source<type> const source{-5'466'734, 4242};
        serialiser<type>::serialise(source, buffer, 0);

        std::array<unsigned char, 6> const expected_buffer{
            0x92, 0x95, 0xAC, 0xFF,     // first
            0x92, 0x10                  // second
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser pair mocks") = [] {
        std::array<std::byte, 50> const buffer{};
        using type = pair<mock_serialisable<7>, mock_serialisable<39>>;
        deserialiser<type> const deser{const_bytes_span{buffer}, 0};
        auto const [first, second] = deser;
        test_assert(bytes_span_same(first._buffer, const_bytes_span{buffer}));
        test_assert(first._fixed_offset == 0);
        test_assert(bytes_span_same(second._buffer, const_bytes_span{buffer}));
        test_assert(second._fixed_offset == 7);
    };

    test_case("deserialiser pair scalars") = [] {
        std::array<unsigned char, 5> const buffer{
            0xE7,                       // first
            0x34, 0x63, 0x4A, 0x83      // second
        };
        using type = pair<std::int8_t, std::uint32_t>;
        deserialiser<type> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.first() == -25);
        test_assert(deser.second() == 2'202'690'356ull);
        auto const [first, second] = deser;
        test_assert(first == -25);
        test_assert(second == 2'202'690'356ull);
    };

};
}

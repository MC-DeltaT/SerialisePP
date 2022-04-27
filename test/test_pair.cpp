#include <array>
#include <cstddef>
#include <cstdint>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>
#include <serialpp/pair.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block pair_tests = [] {

    static_assert(fixed_data_size_v<pair<std::int8_t, std::uint64_t>> == 9);

    test_case("serialiser pair mocks") = [] {
        using type = pair<mock_serialisable<20>, mock_serialisable<12>>;
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<type>(buffer);
        serialise_source<type> const source;
        serialiser<type> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 32, 32, 0, 32};
        test_assert(new_target == expected_new_target);
        test_assert(source.first.targets.size() == 1);
        test_assert(source.first.targets.at(0) == serialise_target{buffer, 32, 0, 20, 32});
        test_assert(source.second.targets.size() == 1);
        test_assert(source.second.targets.at(0) == serialise_target{buffer, 32, 20, 12, 32});
    };

    test_case("serialiser pair scalars") = [] {
        using type = pair<std::int32_t, std::uint16_t>;
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<type>(buffer);
        serialise_source<type> const source{-5'466'734, 4242};
        serialiser<type> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 6, 6, 0, 6};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 6> const expected_buffer{
            0x92, 0x95, 0xAC, 0xFF,     // first
            0x92, 0x10                  // second
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser pair mocks") = [] {
        std::array<std::byte, 50> const buffer{};
        using type = pair<mock_serialisable<7>, mock_serialisable<39>>;
        deserialiser<type> const deser = deserialise<type>(buffer);
        auto const [first, second] = deser;
        test_assert(bytes_view_same(first._fixed_data, const_bytes_span{buffer.data(), 7}));
        test_assert(bytes_view_same(first._variable_data, const_bytes_span{buffer.data() + 46, 4}));
        test_assert(bytes_view_same(second._fixed_data, const_bytes_span{buffer.data() + 7, 39}));
        test_assert(bytes_view_same(second._variable_data, const_bytes_span{buffer.data() + 46, 4}));
    };

    test_case("deserialiser pair scalars") = [] {
        std::array<unsigned char, 5> const buffer{
            0xE7,                       // first
            0x34, 0x63, 0x4A, 0x83      // second
        };
        using type = pair<std::int8_t, std::uint32_t>;
        deserialiser<type> const deser = deserialise<type>(as_const_bytes_span(buffer));
        test_assert(deser.first() == -25);
        test_assert(deser.second() == 2'202'690'356ull);
        auto const [first, second] = deser;
        test_assert(first == -25);
        test_assert(second == 2'202'690'356ull);
    };

};
}

#include <array>
#include <cstddef>
#include <cstdint>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/tuple.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block tuple_tests = [] {

    static_assert(fixed_data_size_v<tuple<>> == 0);
    static_assert(fixed_data_size_v<tuple<std::uint32_t, float, mock_serialisable<14>>> == 22);

    test_case("serialiser tuple empty") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<tuple<>>(buffer);
        serialise_source<tuple<>> const source;
        serialiser<tuple<>> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 0, 0, 0, 0};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser tuple mocks") = [] {
        using type = tuple<mock_serialisable<14>, mock_serialisable<6>, mock_serialisable<8>>;
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<type>(buffer);
        serialise_source<type> const source;
        serialiser<type> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 28, 28, 0, 28};
        test_assert(new_target == expected_new_target);
        test_assert(std::get<0>(source).targets.size() == 1);
        test_assert(std::get<0>(source).targets.at(0) == serialise_target{buffer, 28, 0, 14, 28});
        test_assert(std::get<1>(source).targets.size() == 1);
        test_assert(std::get<1>(source).targets.at(0) == serialise_target{buffer, 28, 14, 6, 28});
        test_assert(std::get<2>(source).targets.size() == 1);
        test_assert(std::get<2>(source).targets.at(0) == serialise_target{buffer, 28, 20, 8, 28});
    };

    test_case("serialiser tuple scalars") = [] {
        using type = tuple<std::uint8_t, std::uint8_t, std::int32_t>;
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<type>(buffer);
        serialise_source<type> const source{86, 174, 23'476'598};
        serialiser<type> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 6, 6, 0, 6};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 6> const expected_buffer{
            0x56,           // element 0
            0xAE,           // element 1
            0x76, 0x39, 0x66, 0x01  // element 2
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser tuple empty") = [] {
        std::array<std::byte, 0> const buffer{};
        deserialiser<tuple<>> const deser = deserialise<tuple<>>(buffer);
    };

    test_case("deserialiser tuple mocks") = [] {
        using type = tuple<mock_serialisable<24>, mock_serialisable<58>, mock_serialisable<8>, mock_serialisable<69>>;
        std::array<std::byte, 216> const buffer{};
        deserialiser<type> const deser = deserialise<type>(buffer);
        test_assert(bytes_view_same(deser.get<0>()._fixed_data, const_bytes_span{buffer.data(), 24}));
        test_assert(bytes_view_same(deser.get<0>()._variable_data, const_bytes_span{buffer.data() + 159, 57}));
        test_assert(bytes_view_same(deser.get<1>()._fixed_data, const_bytes_span{buffer.data() + 24, 58}));
        test_assert(bytes_view_same(deser.get<1>()._variable_data, const_bytes_span{buffer.data() + 159, 57}));
        test_assert(bytes_view_same(deser.get<2>()._fixed_data, const_bytes_span{buffer.data() + 82, 8}));
        test_assert(bytes_view_same(deser.get<2>()._variable_data, const_bytes_span{buffer.data() + 159, 57}));
        test_assert(bytes_view_same(deser.get<3>()._fixed_data, const_bytes_span{buffer.data() + 90, 69}));
        test_assert(bytes_view_same(deser.get<3>()._variable_data, const_bytes_span{buffer.data() + 159, 57}));
    };

    test_case("deserialiser tuple scalars") = [] {
        std::array<unsigned char, 14> const buffer{
            0xDA, 0xC1, 0x24, 0x16, 0x00, 0x00, 0x05, 0x9D,     // element 0
            0xDD, 0x52,                 // element 1
            0x37, 0x45, 0x0A, 0x5B      // element 2
        };
        using type = tuple<std::int64_t, std::int16_t, std::uint32_t>;
        deserialiser<type> const deser = deserialise<type>(as_const_bytes_span(buffer));
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

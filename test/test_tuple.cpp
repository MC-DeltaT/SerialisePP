#include <array>
#include <cstddef>
#include <cstdint>

#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/tuple.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    static_assert(FIXED_DATA_SIZE<Tuple<std::uint32_t, float, MockSerialisable<14>>> == 22);

    STEST_CASE(Serialiser_Tuple_Empty) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Tuple<>>();
        SerialiseSource<Tuple<>> const source{};
        Serialiser<Tuple<>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 0, 0, 0, 0};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Serialiser_Tuple_Nonempty_Mock) {
        using Type = Tuple<MockSerialisable<14>, MockSerialisable<6>, MockSerialisable<8>>;
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Type>();
        SerialiseSource<Type> const source{};
        Serialiser<Type> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 28, 28, 0, 28};
        test_assert(new_target == expected_new_target);
        test_assert(std::get<0>(source).targets.size() == 1);
        test_assert(std::get<0>(source).targets.at(0) == SerialiseTarget{buffer, 28, 0, 14, 28});
        test_assert(std::get<1>(source).targets.size() == 1);
        test_assert(std::get<1>(source).targets.at(0) == SerialiseTarget{buffer, 28, 14, 6, 28});
        test_assert(std::get<2>(source).targets.size() == 1);
        test_assert(std::get<2>(source).targets.at(0) == SerialiseTarget{buffer, 28, 20, 8, 28});
    }

    STEST_CASE(Serialiser_Tuple_Nonempty_Scalar) {
        using Type = Tuple<std::uint8_t, std::uint8_t, std::int32_t>;
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Type>();
        SerialiseSource<Type> const source{86, 174, 23'476'598};
        Serialiser<Type> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 6, 6, 0, 6};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 6> const expected_buffer{
            0x56,           // element 0
            0xAE,           // element 1
            0x76, 0x39, 0x66, 0x01  // element 2
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Tuple_Empty) {
        std::array<std::byte, 0> const buffer{};
        auto const deserialiser = deserialise<Tuple<>>(buffer);
    }

    STEST_CASE(Deserialiser_Tuple_Nonempty_Mock) {
        using Type = Tuple<MockSerialisable<24>, MockSerialisable<58>, MockSerialisable<8>, MockSerialisable<69>>;
        std::array<std::byte, 216> const buffer{};
        auto const deserialiser = deserialise<Type>(buffer);
        test_assert(bytes_view_same(deserialiser.get<0>()._fixed_data, ConstBytesView{buffer.data(), 24}));
        test_assert(bytes_view_same(deserialiser.get<0>()._variable_data, ConstBytesView{buffer.data() + 159, 57}));
        test_assert(bytes_view_same(deserialiser.get<1>()._fixed_data, ConstBytesView{buffer.data() + 24, 58}));
        test_assert(bytes_view_same(deserialiser.get<1>()._variable_data, ConstBytesView{buffer.data() + 159, 57}));
        test_assert(bytes_view_same(deserialiser.get<2>()._fixed_data, ConstBytesView{buffer.data() + 82, 8}));
        test_assert(bytes_view_same(deserialiser.get<2>()._variable_data, ConstBytesView{buffer.data() + 159, 57}));
        test_assert(bytes_view_same(deserialiser.get<3>()._fixed_data, ConstBytesView{buffer.data() + 90, 69}));
        test_assert(bytes_view_same(deserialiser.get<3>()._variable_data, ConstBytesView{buffer.data() + 159, 57}));
    }

    STEST_CASE(Deserialiser_Tuple_Nonempty_Scalar) {
        std::array<unsigned char, 14> const buffer{
            0xDA, 0xC1, 0x24, 0x16, 0x00, 0x00, 0x05, 0x9D,     // element 0
            0xDD, 0x52,                 // element 1
            0x37, 0x45, 0x0A, 0x5B      // element 2
        };
        auto const deserialiser = deserialise<Tuple<std::int64_t, std::int16_t, std::uint32_t>>(
            as_const_bytes_view(buffer));
        test_assert(deserialiser.get<0>() == -7'132'294'434'499'804'710ll);
        test_assert(deserialiser.get<1>() == 21213);
        test_assert(deserialiser.get<2>() == 1'527'399'735);
        auto const [element0, element1, element2] = deserialiser;
        test_assert(element0 == -7'132'294'434'499'804'710ll);
        test_assert(element1 == 21213);
        test_assert(element2 == 1'527'399'735);
    }

}

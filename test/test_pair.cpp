#include <array>
#include <cstddef>
#include <cstdint>

#include <serialpp/common.hpp>
#include <serialpp/pair.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    static_assert(FIXED_DATA_SIZE<Pair<std::int8_t, std::uint64_t>> == 9);

    STEST_CASE(Serialiser_Pair_Mock) {
        using Type = Pair<MockSerialisable<20>, MockSerialisable<12>>;
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Type>();
        SerialiseSource<Type> const source{};
        Serialiser<Type> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 32, 32, 0, 32};
        test_assert(new_target == expected_new_target);
        test_assert(source.first.targets.size() == 1);
        test_assert(source.first.targets.at(0) == SerialiseTarget{buffer, 32, 0, 20, 32});
        test_assert(source.second.targets.size() == 1);
        test_assert(source.second.targets.at(0) == SerialiseTarget{buffer, 32, 20, 12, 32});
    }

    STEST_CASE(Serialiser_Pair_Scalar) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Pair<std::int32_t, std::uint16_t>>();
        SerialiseSource<Pair<std::int32_t, std::uint16_t>> const source{-5'466'734, 4242};
        Serialiser<Pair<std::int32_t, std::uint16_t>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 6, 6, 0, 6};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 6> const expected_buffer{
            0x92, 0x95, 0xAC, 0xFF,     // first
            0x92, 0x10                  // second
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Pair_Mock) {
        std::array<std::byte, 50> const buffer{};
        auto const deserialiser = deserialise<Pair<MockSerialisable<7>, MockSerialisable<39>>>(buffer);
        auto const [first, second] = deserialiser;
        test_assert(bytes_view_same(first._fixed_data, ConstBytesView{buffer.data(), 7}));
        test_assert(bytes_view_same(first._variable_data, ConstBytesView{buffer.data() + 46, 4}));
        test_assert(bytes_view_same(second._fixed_data, ConstBytesView{buffer.data() + 7, 39}));
        test_assert(bytes_view_same(second._variable_data, ConstBytesView{buffer.data() + 46, 4}));
    }

    STEST_CASE(Deserialiser_Pair_Scalar) {
        std::array<unsigned char, 5> const buffer{
            0xE7,                       // first
            0x34, 0x63, 0x4A, 0x83      // second
        };
        auto const deserialiser = deserialise<Pair<std::int8_t, std::uint32_t>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.first() == -25);
        test_assert(deserialiser.second() == 2'202'690'356ull);
        auto const [first, second] = deserialiser;
        test_assert(first == -25);
        test_assert(second == 2'202'690'356ull);
    }

}

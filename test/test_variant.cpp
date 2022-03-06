#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/variant.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    static_assert(FIXED_DATA_SIZE<Variant<std::uint8_t, MockSerialisable<100>, std::int32_t>> == 3);

    STEST_CASE(Serialiser_Variant_Empty) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Variant<>>();
        SerialiseSource<Variant<>> const source{};
        Serialiser<Variant<>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 3, 3, 0, 3};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 3> const expected_buffer{
            0x00,       // Type index
            0x00, 0x00  // Value offset
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Serialiser_Variant_Nonempty_Mock) {
        using Type = Variant<MockSerialisable<534>, MockSerialisable<87>, MockSerialisable<9066>, MockSerialisable<14>>;
        SerialiseBuffer buffer;
        buffer.extend(13);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[3 + i] = static_cast<std::byte>(i + 1);
        }
        SerialiseSource<Type> const source{std::in_place_index<3>};
        SerialiseTarget const target{buffer, 3, 0, 3, 13};
        Serialiser<Type> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_target{buffer, 3, 3, 0, 27};
        test_assert(new_target == expected_target);
        test_assert(std::get<3>(source).targets.size() == 1);
        test_assert(std::get<3>(source).targets.at(0) == SerialiseTarget{buffer, 3, 13, 14, 27});
    }

    STEST_CASE(Serialiser_Variant_Nonempty_Scalar) {
        SerialiseBuffer buffer;
        buffer.extend(13);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[3 + i] = static_cast<std::byte>(i + 1);
        }
        SerialiseSource<Variant<std::uint32_t, std::byte, std::int64_t>> const source{
            std::in_place_index<2>, 3'245'678};
        SerialiseTarget const target{buffer, 3, 0, 3, 13};
        Serialiser<Variant<std::uint32_t, std::byte, std::int64_t>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_target{buffer, 3, 3, 0, 21};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 21> const expected_buffer{
            0x02,           // Type index
            0x0A, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Variant_Empty) {
        std::array<unsigned char, 3> const buffer{
            0x00,       // Type index
            0x00, 0x00  // Value offset
        };
        auto const deserialiser = deserialise<Variant<>>(as_const_bytes_view(buffer));
        deserialiser.visit([](auto) {
            fail_test();
        });
    }

    STEST_CASE(Deserialiser_Variant_Nonempty_Mock) {
        std::array<unsigned char, 70> const buffer{
            0x01,           // Type index
            0x2A, 0x00,     // Offset
        };
        auto const buffer_view = as_const_bytes_view(buffer);
        auto const deserialiser = deserialise<
            Variant<MockSerialisable<10>, MockSerialisable<15>, MockSerialisable<24>>>(buffer_view);
        test_assert(deserialiser.index() == 1);
        test_assert(bytes_view_same(deserialiser.get<1>()._fixed_data, ConstBytesView{buffer_view.data() + 45, 15}));
        test_assert(bytes_view_same(deserialiser.get<1>()._variable_data, ConstBytesView{buffer_view.data() + 3, 67}));
    }

    STEST_CASE(Deserialiser_Variant_Nonempty_Scalar) {
        std::array<unsigned char, 21> const buffer{
            0x02,           // Type index
            0x0A, 0x00,     // Value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00      // Value
        };
        auto const deserialiser = deserialise<Variant<std::uint32_t, std::uint8_t, std::int64_t>>(
            as_const_bytes_view(buffer));
        test_assert(deserialiser.index() == 2);
        test_assert(deserialiser.get<2>() == 3'245'678);
        test_assert(42 == deserialiser.visit([](auto value) {
            test_assert(std::same_as<decltype(value), std::int64_t>);
            test_assert(value == 3'245'678);
            return 42;
        }));
        test_assert_throws<std::bad_variant_access>([&deserialiser] { (void)deserialiser.get<0>(); });
        test_assert_throws<std::bad_variant_access>([&deserialiser] { (void)deserialiser.get<1>(); });
    }

}
#include <array>
#include <cstddef>
#include <cstdint>

#include <serialpp/common.hpp>
#include <serialpp/struct.hpp>

#include "helpers/common.hpp"
#include "helpers/struct.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    // TODO: empty struct

    static_assert(FIXED_DATA_SIZE<BasicTestStruct> == 1 + 4 + 2 + 8);

    STEST_CASE(SerialiseSource_Struct) {
        SerialiseSource<BasicTestStruct> source{std::int8_t{1}, std::uint32_t{2u}, std::int16_t{3}, std::uint64_t{4u}};
        test_assert(source.get<"a">() == 1);
        test_assert(source.get<"foo">() == 2u);
        test_assert(source.get<"my field">() == 3);
        test_assert(source.get<"qux">() == 4u);

        source.get<"qux">() = 9'876'543'210ull;
        source.get<"a">() = -1;
        test_assert(source.get<"a">() == -1);
        test_assert(source.get<"foo">() == 2u);
        test_assert(source.get<"my field">() == 3);
        test_assert(source.get<"qux">() == 9'876'543'210ull);
    }

    STEST_CASE(Serialiser_Struct_Mock) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<MockTestStruct>();
        SerialiseSource<MockTestStruct> const source{};
        Serialiser<MockTestStruct> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 48, 48, 0, 48};
        test_assert(new_target == expected_new_target);
        test_assert(source.get<"magic">().targets.size() == 1);
        test_assert(source.get<"magic">().targets.at(0) == SerialiseTarget{buffer, 48, 0, 20, 48});
        test_assert(source.get<"foo">().targets.size() == 1);
        test_assert(source.get<"foo">().targets.at(0) == SerialiseTarget{buffer, 48, 20, 10, 48});
        test_assert(source.get<"field">().targets.size() == 1);
        test_assert(source.get<"field">().targets.at(0) == SerialiseTarget{buffer, 48, 30, 5, 48});
        test_assert(source.get<"qux">().targets.size() == 1);
        test_assert(source.get<"qux">().targets.at(0) == SerialiseTarget{buffer, 48, 35, 11, 48});
        test_assert(source.get<"anotherone">().targets.size() == 1);
        test_assert(source.get<"anotherone">().targets.at(0) == SerialiseTarget{buffer, 48, 46, 2, 48});
    }

    STEST_CASE(Serialiser_Struct_Scalar) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<BasicTestStruct>();
        SerialiseSource<BasicTestStruct> const source{
            std::int8_t{-34},
            std::uint32_t{206'000u},
            std::int16_t{36},
            std::uint64_t{360'720u}
        };
        Serialiser<BasicTestStruct> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_target{buffer, 15, 15, 0, 15};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 15> const expected_buffer{
            0xDE,                   // a
            0xB0, 0x24, 0x03, 0x00, // foo
            0x24, 0x00,             // my field
            0x10, 0x81, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00  // qux
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Struct_Mock) {
        std::array<std::byte, 100> const buffer{};
        auto const deserialiser = deserialise<MockTestStruct>(buffer);
        test_assert(bytes_view_same(deserialiser.get<"magic">()._fixed_data, ConstBytesView{buffer.data(), 20}));
        test_assert(
            bytes_view_same(deserialiser.get<"magic">()._variable_data, ConstBytesView{buffer.data() + 48, 52}));
        test_assert(bytes_view_same(deserialiser.get<"foo">()._fixed_data, ConstBytesView{buffer.data() + 20, 10}));
        test_assert(bytes_view_same(deserialiser.get<"foo">()._variable_data, ConstBytesView{buffer.data() + 48, 52}));
        test_assert(bytes_view_same(deserialiser.get<"field">()._fixed_data, ConstBytesView{buffer.data() + 30, 5}));
        test_assert(
            bytes_view_same(deserialiser.get<"field">()._variable_data, ConstBytesView{buffer.data() + 48, 52}));
        test_assert(bytes_view_same(deserialiser.get<"qux">()._fixed_data, ConstBytesView{buffer.data() + 35, 11}));
        test_assert(bytes_view_same(deserialiser.get<"qux">()._variable_data, ConstBytesView{buffer.data() + 48, 52}));
        test_assert(
            bytes_view_same(deserialiser.get<"anotherone">()._fixed_data, ConstBytesView{buffer.data() + 46, 2}));
        test_assert(
            bytes_view_same(deserialiser.get<"anotherone">()._variable_data, ConstBytesView{buffer.data() + 48, 52}));
    }

    STEST_CASE(Deserialiser_Struct_Scalar) {
        std::array<unsigned char, 15> const buffer{
            0x9C,                   // a
            0x15, 0xCD, 0x5B, 0x07, // foo
            0x30, 0x75,             // my field
            0xFF, 0xE7, 0x76, 0x48, 0x17, 0x00, 0x00, 0x00  // qux
        };
        auto const deserialiser = deserialise<BasicTestStruct>(as_const_bytes_view(buffer));
        test_assert(deserialiser.get<"a">() == -100);
        test_assert(deserialiser.get<"foo">() == 123'456'789u);
        test_assert(deserialiser.get<"my field">() == 30000);
        test_assert(deserialiser.get<"qux">() == 99'999'999'999ull);
        auto const [a, foo, my_field, qux] = deserialiser;
        test_assert(a == -100);
        test_assert(foo == 123'456'789u);
        test_assert(my_field == 30000);
        test_assert(qux == 99'999'999'999ull);
    }

}

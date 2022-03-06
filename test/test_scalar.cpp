#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    static_assert(Scalar<Void>);
    static_assert(Scalar<char>);
    static_assert(Scalar<unsigned char>);
    static_assert(Scalar<std::byte>);
    static_assert(Scalar<bool>);
    static_assert(Scalar<int>);
    static_assert(Scalar<unsigned>);
    static_assert(Scalar<std::uint8_t>);
    static_assert(Scalar<std::int8_t>);
    static_assert(Scalar<std::int32_t>);
    static_assert(Scalar<std::uint64_t>);
    static_assert(Scalar<float>);
    static_assert(Scalar<double>);

    static_assert(!Scalar<MockSerialisable<1>>);
    static_assert(!Scalar<std::string>);
    static_assert(!Scalar<long double>);


    static_assert(FIXED_DATA_SIZE<Void> == 0);

    STEST_CASE(Serialiser_Void) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<Void>();
        SerialiseSource<Void> const source{};
        Serialiser<Void> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 0, 0, 0, 0};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Void) {
        std::array<std::byte, 0> const buffer{};
        auto const deserialiser = deserialise<Void>(buffer);
        test_assert(deserialiser.value() == Void{});
    }


    static_assert(FIXED_DATA_SIZE<std::byte> == 1);

    STEST_CASE(Serialiser_Byte) {
        SerialiseBuffer buffer;
        auto target = buffer.initialise<std::byte>();
        SerialiseSource<std::byte> const source{std::byte{0x12}};
        Serialiser<std::byte> const serialiser;
        target = serialiser(source, target);

        std::array<unsigned char, 1> const expected_buffer{0x12};
        test_assert(buffer_equal(buffer, expected_buffer));
        SerialiseTarget const expected_target{buffer, 1, 0, 1, 1};
        test_assert(target == expected_target);
    }

    STEST_CASE(Deserialiser_Byte) {
        std::array<unsigned char, 1> const buffer{0xF4};
        auto const deserialiser = deserialise<std::byte>(as_const_bytes_view(buffer));
        test_assert(deserialiser.value() == std::byte{0xF4});
    }


    static_assert(FIXED_DATA_SIZE<std::uint32_t> == 4);

    STEST_CASE(Serialiser_UInt) {
        SerialiseBuffer buffer;
        auto target = buffer.initialise<std::uint32_t>();
        SerialiseSource<std::uint32_t> const source{43'834'534u};
        Serialiser<std::uint32_t> const serialiser;
        target = serialiser(source, target);

        std::array<unsigned char, 4> const expected_buffer{0xA6, 0xDC, 0x9C, 0x02};
        test_assert(buffer_equal(buffer, expected_buffer));
        SerialiseTarget const expected_target{buffer, 4, 0, 4, 4};
        test_assert(target == expected_target);
    }

    STEST_CASE(Deserialiser_UInt) {
        std::array<unsigned char, 4> const buffer{0x01, 0x23, 0x45, 0x67};
        auto const deserialiser = deserialise<std::uint32_t>(as_const_bytes_view(buffer));
        auto const value = deserialiser.value();
        test_assert(value == 1'732'584'193ull);
    }


    static_assert(FIXED_DATA_SIZE<std::int64_t> == 8);

    STEST_CASE(Serialiser_Int) {
        SerialiseBuffer buffer;
        auto target = buffer.initialise<std::int64_t>();
        SerialiseSource<std::int64_t> const source{-567'865'433'565'765ll};
        Serialiser<std::int64_t> const serialiser;
        target = serialiser(source, target);

        std::array<unsigned char, 8> const expected_buffer{0xBB, 0x55, 0x8D, 0x86, 0x87, 0xFB, 0xFD, 0xFF};
        test_assert(buffer_equal(buffer, expected_buffer));
        SerialiseTarget const expected_target{buffer, 8, 0, 8, 8};
        test_assert(target == expected_target);
    }

    STEST_CASE(Deserialiser_Int) {
        std::array<unsigned char, 8> const buffer{0x01, 0xC2, 0x31, 0xB3, 0xFB, 0xFF, 0xFF, 0xFF};
        auto const deserialiser = deserialise<std::int64_t>(as_const_bytes_view(buffer));
        auto const value = deserialiser.value();
        test_assert(value == -18'468'453'887ll);
    }


    static_assert(FIXED_DATA_SIZE<bool> == 1);

    STEST_CASE(Serialiser_Bool_False) {
        SerialiseBuffer buffer;
        auto target = buffer.initialise<bool>();
        SerialiseSource<bool> const source{false};
        Serialiser<bool> const serialiser;
        target = serialiser(source, target);

        std::array<unsigned char, 1> const expected_buffer{0x00};
        test_assert(buffer_equal(buffer, expected_buffer));
        SerialiseTarget const expected_target{buffer, 1, 0, 1, 1};
        test_assert(target == expected_target);
    }

    STEST_CASE(Serialiser_Bool_True) {
        SerialiseBuffer buffer;
        auto target = buffer.initialise<bool>();
        SerialiseSource<bool> const source{true};
        Serialiser<bool> const serialiser;
        target = serialiser(source, target);

        std::array<unsigned char, 1> const expected_buffer{0x01};
        test_assert(buffer_equal(buffer, expected_buffer));
        SerialiseTarget const expected_target{buffer, 1, 0, 1, 1};
        test_assert(target == expected_target);
    }

    STEST_CASE(Deserialiser_Bool_False) {
        std::array<unsigned char, 1> const buffer{0x00};
        auto const deserialiser = deserialise<bool>(as_const_bytes_view(buffer));
        test_assert(!deserialiser.value());
    }

    STEST_CASE(Deserialiser_Bool_True) {
        std::array<unsigned char, 1> const buffer{0x01};
        auto const deserialiser = deserialise<bool>(as_const_bytes_view(buffer));
        test_assert(deserialiser.value());
    }


    static_assert(FIXED_DATA_SIZE<float> == 4);

    STEST_CASE(Serialiser_Float) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<float>();
        SerialiseSource<float> const source{-100'000'000.0f};
        Serialiser<float> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 4, 0, 4, 4};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 4> const expected_buffer{0x20, 0xBC, 0xBE, 0xCC};
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Float) {
        std::array<unsigned char, 4> const buffer{0x20, 0xBC, 0xBE, 0xCC};
        auto const deserialiser = deserialise<float>(as_const_bytes_view(buffer));
        test_assert(deserialiser.value() == -100'000'000.0f);
    }


    static_assert(FIXED_DATA_SIZE<double> == 8);

    STEST_CASE(Serialiser_Double) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<double>();
        SerialiseSource<double> const source{12'345'678'900'000'000.0};
        Serialiser<double> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 8, 0, 8, 8};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 8> const expected_buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Double) {
        std::array<unsigned char, 8> const buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
        auto const deserialiser = deserialise<double>(as_const_bytes_view(buffer));
        test_assert(deserialiser.value() == 12'345'678'900'000'000.0);
    }


    STEST_CASE(AutoDeserialise_Scalar) {
        std::array<unsigned char, 100> const buffer{0x01, 0x23, 0x45, 0x67, 0x11, 0x22, 0x33};
        auto const deserialiser = deserialise<std::uint32_t>(as_const_bytes_view(buffer));
        auto const value = auto_deserialise(deserialiser);
        test_assert(value == 1'732'584'193u);
    }

}

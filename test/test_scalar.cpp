#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

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

    static_assert(!Scalar<MockSerialisable<1>>);
    static_assert(!Scalar<std::string>);


    static_assert(FIXED_DATA_SIZE<std::byte> == 1);

    STEST_CASE(Serialiser_Byte) {
        SerialiseBuffer buffer;
        auto target = buffer.initialise_for<std::byte>();
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
        auto const deserialiser = deserialise<std::byte>(std::as_bytes(std::span{buffer}));
        test_assert(deserialiser.value() == std::byte{0xF4});
    }

    
    static_assert(FIXED_DATA_SIZE<std::uint32_t> == 4);

    STEST_CASE(Serialiser_UInt) {
        // TODO
    }

    STEST_CASE(Deserialiser_UInt) {
        std::array<unsigned char, 4> const buffer{0x01, 0x23, 0x45, 0x67};
        auto const deserialiser = deserialise<std::uint32_t>(std::as_bytes(std::span{buffer}));
        auto const value = deserialiser.value();
        test_assert(value == 1'732'584'193ull);
    }


    // TODO: signed integer


    static_assert(FIXED_DATA_SIZE<bool> == 1);

    STEST_CASE(Serialiser_BoolFalse) {
        SerialiseBuffer buffer;
        auto target = buffer.initialise_for<bool>();
        SerialiseSource<bool> const source{false};
        Serialiser<bool> const serialiser;
        target = serialiser(source, target);

        std::array<unsigned char, 1> const expected_buffer{0x00};
        test_assert(buffer_equal(buffer, expected_buffer));
        SerialiseTarget const expected_target{buffer, 1, 0, 1, 1};
        test_assert(target == expected_target);
    }

    STEST_CASE(Serialiser_BoolTrue) {
        SerialiseBuffer buffer;
        auto target = buffer.initialise_for<bool>();
        SerialiseSource<bool> const source{true};
        Serialiser<bool> const serialiser;
        target = serialiser(source, target);

        std::array<unsigned char, 1> const expected_buffer{0x01};
        test_assert(buffer_equal(buffer, expected_buffer));
        SerialiseTarget const expected_target{buffer, 1, 0, 1, 1};
        test_assert(target == expected_target);
    }

    STEST_CASE(Deserialiser_BoolFalse) {
        std::array<unsigned char, 1> const buffer{0x00};
        auto const deserialiser = deserialise<bool>(std::as_bytes(std::span{buffer}));
        test_assert(!deserialiser.value());
    }

    STEST_CASE(Deserialiser_BoolTrue) {
        std::array<unsigned char, 1> const buffer{0x01};
        auto const deserialiser = deserialise<bool>(std::as_bytes(std::span{buffer}));
        test_assert(deserialiser.value());
    }


    STEST_CASE(AutoDeserialiseScalar_Scalar) {
        std::array<unsigned char, 100> const buffer{0x01, 0x23, 0x45, 0x67, 0x11, 0x22, 0x33};
        auto const deserialiser = deserialise<std::uint32_t>(std::as_bytes(std::span{buffer}));
        auto const value = auto_deserialise_scalar(deserialiser);
        test_assert(value == 1'732'584'193u);
    }

    STEST_CASE(AutoDeserialiseScalar_Nonscalar) {
        std::array<std::byte, 100> const buffer{};
        auto const deserialiser = deserialise<MockSerialisable<23>>(ConstBytesView{buffer});
        auto const result = auto_deserialise_scalar(deserialiser);
        test_assert(result == deserialiser);
    }

}

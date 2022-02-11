#include "helpers/test.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"


namespace serialpp::test {

    static_assert(Scalar<char>);
    static_assert(Scalar<unsigned char>);
    static_assert(Scalar<std::byte>);
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
        SerialiseSource<std::byte> const source{std::byte{139}};
        Serialiser<std::byte> const serialiser;
        target = serialiser(source, target);

        std::array<std::byte, 1> const expected_buffer{std::byte{139}};
        test_assert(buffer_equal(buffer, expected_buffer));
        SerialiseTarget const expected_target{buffer, 1, 0, 1, 1};
        test_assert(target == expected_target);
    }

    STEST_CASE(Deserialiser_Byte) {
        std::array<std::byte, 1> const buffer{std::byte{235}};
        auto const deserialiser = deserialise<std::byte>(ConstBytesView{buffer});
        test_assert(deserialiser.value() == std::byte{235});
    }

    
    // TODO: unsigned integer


    // TODO: signed integer


    // TODO: bool

}

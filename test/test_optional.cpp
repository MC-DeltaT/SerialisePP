#include <array>
#include <cstddef>
#include <cstdint>

#include <serialpp/common.hpp>
#include <serialpp/optional.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    static_assert(FIXED_DATA_SIZE<Optional<char>> == 2);
    static_assert(FIXED_DATA_SIZE<Optional<std::uint64_t>> == 2);
    static_assert(FIXED_DATA_SIZE<Optional<MockSerialisable<10000>>> == 2);

    STEST_CASE(Serialiser_Optional_Empty) {
        SerialiseBuffer buffer;
        SerialiseSource<Optional<std::int32_t>> const source{};
        auto const target = buffer.initialise<Optional<std::int32_t>>();
        Serialiser<Optional<std::int32_t>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_target{buffer, 2, 2, 0, 2};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 2> const expected_buffer{0x00, 0x00};
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Serialiser_Optional_Nonempty_Mock) {
        using Type = Optional<MockSerialisable<25>>;
        SerialiseBuffer buffer;
        buffer.extend(12);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[2 + i] = static_cast<std::byte>(i + 1);
        }
        SerialiseTarget const target{buffer, 2, 0, 2, 12};
        SerialiseSource<Type> const source{{}};
        Serialiser<Type> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_new_target{buffer, 2, 2, 0, 37};
        test_assert(new_target == expected_new_target);
        test_assert(source.value().targets.size() == 1);
        test_assert(source.value().targets.at(0) == SerialiseTarget{buffer, 2, 12, 25, 37});
    }

    STEST_CASE(Serialiser_Optional_Nonempty_Scalar) {
        SerialiseBuffer buffer;
        buffer.extend(12);
        for (unsigned i = 0; i < 10; ++i) {
            buffer.span()[2 + i] = static_cast<std::byte>(i + 1);
        }
        SerialiseSource<Optional<std::uint32_t>> const source{3245678};
        SerialiseTarget const target{buffer, 2, 0, 2, 12};
        Serialiser<Optional<std::uint32_t>> const serialiser;
        auto const new_target = serialiser(source, target);

        SerialiseTarget const expected_target{buffer, 2, 2, 0, 16};
        test_assert(new_target == expected_target);
        std::array<unsigned char, 16> const expected_buffer{
            0x0B, 0x00,     // Optional value offset
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,     // Dummy padding
            0x6E, 0x86, 0x31, 0x00      // Optional value
        };
        test_assert(buffer_equal(buffer, expected_buffer));
    }

    STEST_CASE(Deserialiser_Optional_Empty) {
        std::array<unsigned char, 2> const buffer{0x00, 0x00};
        auto const deserialiser = deserialise<Optional<std::uint64_t>>(as_const_bytes_view(buffer));
        test_assert(!deserialiser.has_value());
        test_assert_throws<std::bad_optional_access>([&deserialiser] {
            (void)deserialiser.value();
        });
    }

    STEST_CASE(Deserialiser_Optional_Nonempty_Mock) {
        std::array<unsigned char, 70> const buffer{
            0x05, 0x00      // Optional value offset
        };
        auto const buffer_view = as_const_bytes_view(buffer);
        auto const deserialiser = deserialise<Optional<MockSerialisable<48>>>(buffer_view);
        test_assert(deserialiser.has_value());
        test_assert(bytes_view_same(deserialiser.value()._fixed_data, ConstBytesView{buffer_view.data() + 6, 48}));
        test_assert(bytes_view_same(deserialiser.value()._variable_data, ConstBytesView{buffer_view.data() + 2, 68}));
    }

    STEST_CASE(Deserialiser_Optional_Nonempty_Scalar) {
        std::array<unsigned char, 8> const buffer{
            0x05, 0x00,     // Optional value offset
            0x11, 0x22, 0x33, 0x44,     // Dummy padding
            0xFE, 0xDC      // Optional value
        };
        auto const deserialiser = deserialise<Optional<std::int16_t>>(as_const_bytes_view(buffer));
        test_assert(deserialiser.has_value());
        test_assert(deserialiser.value() == -8962);
        test_assert(*deserialiser == -8962);
    }

    STEST_CASE(Deserialiser_Optional_OffsetOutOfRange) {
        std::array<unsigned char, 8> const buffer{
            0x07, 0x00,     // Optional value offset
            0x11, 0x22, 0x33, 0x44,     // Dummy padding
            0xFE, 0xDC      // Optional value
        };
        auto const deserialiser = deserialise<Optional<std::int16_t>>(as_const_bytes_view(buffer));
        test_assert_throws<VariableBufferSizeError>([&deserialiser] {
            (void)deserialiser.value();
        });
    }

}

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block scalar_tests = [] {

    static_assert(!scalar<std::string>);
    static_assert(!scalar<long double>);


    static_assert(scalar<null>);
    static_assert(fixed_size_serialisable<null>);
    static_assert(fixed_data_size_v<null> == 0);
    static_assert(std::semiregular<serialise_source<null>>);

    test_case("serialiser null") = [] {
        basic_buffer buffer;
        buffer.initialise(0);
        serialise_source<null> const source;
        serialiser<null>::serialise(source, buffer, 0);

        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser null") = [] {
        std::array<std::byte, 0> const buffer{};
        deserialiser<null> const deser{const_bytes_span{buffer}, 0};
        test_assert(deser.value() == null{});
    };


    static_assert(scalar<std::byte>);
    static_assert(fixed_size_serialisable<std::byte>);
    static_assert(fixed_data_size_v<std::byte> == 1);
    static_assert(std::semiregular<serialise_source<std::byte>>);

    test_case("serialiser byte") = [] {
        basic_buffer buffer;
        buffer.initialise(1);
        serialise_source<std::byte> const source{std::byte{0x12}};
        serialiser<std::byte>::serialise(source, buffer, 0);

        std::array<unsigned char, 1> const expected_buffer{0x12};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser byte") = [] {
        std::array<unsigned char, 1> const buffer{0xF4};
        deserialiser<std::byte> const deser{as_const_bytes_span(buffer), 0};
        test_assert(deser.value() == std::byte{0xF4});
    };


    static_assert(scalar<std::uint32_t>);
    static_assert(fixed_size_serialisable<std::uint32_t>);
    static_assert(fixed_data_size_v<std::uint32_t> == 4);
    static_assert(std::semiregular<serialise_source<std::uint32_t>>);

    test_case("serialiser unsigned integer") = [] {
        basic_buffer buffer;
        buffer.initialise(4);
        serialise_source<std::uint32_t> const source{43'834'534u};
        serialiser<std::uint32_t>::serialise(source, buffer, 0);

        std::array<unsigned char, 4> const expected_buffer{0xA6, 0xDC, 0x9C, 0x02};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser unsigned integer") = [] {
        std::array<unsigned char, 4> const buffer{0x01, 0x23, 0x45, 0x67};
        deserialiser<std::uint32_t> const deser{as_const_bytes_span(buffer), 0};
        std::same_as<std::uint32_t> auto const value = deser.value();
        test_assert(value == 1'732'584'193ull);
    };


    static_assert(scalar<std::int64_t>);
    static_assert(fixed_size_serialisable<std::int64_t>);
    static_assert(fixed_data_size_v<std::int64_t> == 8);
    static_assert(std::semiregular<serialise_source<std::int64_t>>);

    test_case("serialiser signed integer") = [] {
        basic_buffer buffer;
        buffer.initialise(8);
        serialise_source<std::int64_t> const source{-567'865'433'565'765ll};
        serialiser<std::int64_t>::serialise(source, buffer, 0);

        std::array<unsigned char, 8> const expected_buffer{0xBB, 0x55, 0x8D, 0x86, 0x87, 0xFB, 0xFD, 0xFF};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser signed integer") = [] {
        std::array<unsigned char, 8> const buffer{0x01, 0xC2, 0x31, 0xB3, 0xFB, 0xFF, 0xFF, 0xFF};
        deserialiser<std::int64_t> const deser{as_const_bytes_span(buffer), 0};
        std::same_as<std::int64_t> auto const value = deser.value();
        test_assert(value == -18'468'453'887ll);
    };


    static_assert(scalar<bool>);
    static_assert(fixed_size_serialisable<bool>);
    static_assert(fixed_data_size_v<bool> == 1);
    static_assert(std::semiregular<serialise_source<bool>>);

    test_case("serialiser bool false") = [] {
        basic_buffer buffer;
        buffer.initialise(1);
        serialise_source<bool> const source{false};
        serialiser<bool>::serialise(source, buffer, 0);

        std::array<unsigned char, 1> const expected_buffer{0x00};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("serialiser bool true") = [] {
        basic_buffer buffer;
        buffer.initialise(1);
        serialise_source<bool> const source{true};
        serialiser<bool>::serialise(source, buffer, 0);

        std::array<unsigned char, 1> const expected_buffer{0x01};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser bool false") = [] {
        std::array<unsigned char, 1> const buffer{0x00};
        deserialiser<bool> const deser{as_const_bytes_span(buffer), 0};
        std::same_as<bool> auto const value = deser.value();
        test_assert(!value);
    };

    test_case("deserialiser bool true") = [] {
        std::array<unsigned char, 1> const buffer{0x01};
        deserialiser<bool> const deser{as_const_bytes_span(buffer), 0};
        std::same_as<bool> auto const value = deser.value();
        test_assert(value);
    };


    if constexpr (sizeof(float) == 4 && std::numeric_limits<float>::is_iec559
            && (std::endian::native == std::endian::little || std::endian::native == std::endian::big)) {
        static_assert(scalar<float>);
        static_assert(floating_point<float>);
        static_assert(fixed_size_serialisable<float>);
        static_assert(fixed_data_size_v<float> == 4);
        static_assert(std::semiregular<serialise_source<float>>);

        test_case("serialiser float") = [] {
            basic_buffer buffer;
            buffer.initialise(4);
            serialise_source<float> const source{-100'000'000.0f};
            serialiser<float>::serialise(source, buffer, 0);

            std::array<unsigned char, 4> const expected_buffer{0x20, 0xBC, 0xBE, 0xCC};
            test_assert(buffer_equal(buffer, expected_buffer));
        };

        test_case("serialiser float constexpr") = [] {
            []() consteval {
                basic_buffer buffer;
                buffer.initialise(4);
                serialise_source<float> const source{-100'000'000.0f};
                serialiser<float>::serialise(source, buffer, 0);

                std::array<unsigned char, 4> const expected_buffer{0x20, 0xBC, 0xBE, 0xCC};
                test_assert(buffer_equal(buffer, expected_buffer));
            }();
        };

        test_case("deserialiser float") = [] {
            std::array<unsigned char, 4> const buffer{0x20, 0xBC, 0xBE, 0xCC};
            deserialiser<float> const deser{as_const_bytes_span(buffer), 0};
            std::same_as<float> auto const value = deser.value();
            test_assert(value == -100'000'000.0f);
        };

        test_case("deserialiser float constexpr") = [] {
            []() consteval {
                std::array<unsigned char, 4> const buffer{0x20, 0xBC, 0xBE, 0xCC};
                auto const buffer_bytes = uchar_array_to_bytes(buffer);
                deserialiser<float> const deser{const_bytes_span{buffer_bytes}, 0};
                std::same_as<float> auto const value = deser.value();
                test_assert(value == -100'000'000.0f);
            }();
        };
    }


    if constexpr (sizeof(double) == 8 && std::numeric_limits<double>::is_iec559
            && (std::endian::native == std::endian::little || std::endian::native == std::endian::big)) {
        static_assert(scalar<double>);
        static_assert(floating_point<double>);
        static_assert(fixed_size_serialisable<double>);
        static_assert(fixed_data_size_v<double> == 8);
        static_assert(std::semiregular<serialise_source<double>>);

        test_case("serialiser double") = [] {
            basic_buffer buffer;
            buffer.initialise(8);
            serialise_source<double> const source{12'345'678'900'000'000.0};
            serialiser<double>::serialise(source, buffer, 0);

            std::array<unsigned char, 8> const expected_buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
            test_assert(buffer_equal(buffer, expected_buffer));
        };

        test_case("serialiser double constexpr") = [] {
            []() consteval {
                basic_buffer buffer;
                buffer.initialise(8);
                serialise_source<double> const source{12'345'678'900'000'000.0};
                serialiser<double>::serialise(source, buffer, 0);

                std::array<unsigned char, 8> const expected_buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
                test_assert(buffer_equal(buffer, expected_buffer));
            }();
        };

        test_case("deserialiser double") = [] {
            std::array<unsigned char, 8> const buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
            deserialiser<double> const deser{as_const_bytes_span(buffer), 0};
            std::same_as<double> auto const value = deser.value();
            test_assert(value == 12'345'678'900'000'000.0);
        };

        test_case("deserialiser double constexpr") = [] {
            []() consteval {
                std::array<unsigned char, 8> const buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
                auto const buffer_bytes = uchar_array_to_bytes(buffer);
                deserialiser<double> const deser{const_bytes_span{buffer_bytes}, 0};
                std::same_as<double> auto const value = deser.value();
                test_assert(value == 12'345'678'900'000'000.0);
            }();
        };
    }


    test_case("auto_deserialise() scalar") = [] {
        std::array<unsigned char, 100> const buffer{0x01, 0x23, 0x45, 0x67, 0x11, 0x22, 0x33};
        deserialiser<std::uint32_t> const deser{as_const_bytes_span(buffer), 0};
        std::same_as<std::uint32_t> auto const value = auto_deserialise(deser);
        test_assert(value == 1'732'584'193u);
    };

};
}

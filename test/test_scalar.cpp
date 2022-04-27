#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>
#include <serialpp/scalar.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block scalar_tests = [] {

    static_assert(scalar<null>);
    static_assert(scalar<char>);
    static_assert(scalar<unsigned char>);
    static_assert(scalar<std::byte>);
    static_assert(scalar<bool>);
    static_assert(scalar<int>);
    static_assert(scalar<unsigned>);
    static_assert(scalar<std::uint8_t>);
    static_assert(scalar<std::int8_t>);
    static_assert(scalar<std::int32_t>);
    static_assert(scalar<std::uint64_t>);

    static_assert(!scalar<mock_serialisable<1>>);
    static_assert(!scalar<std::string>);
    static_assert(!scalar<long double>);


    static_assert(fixed_data_size_v<null> == 0);

    test_case("serialiser null") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<null>(buffer);
        serialise_source<null> const source;
        serialiser<null> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        serialise_target const expected_new_target{buffer, 0, 0, 0, 0};
        test_assert(new_target == expected_new_target);
        std::array<unsigned char, 0> const expected_buffer{};
        test_assert(buffer_equal(buffer, expected_buffer));
    };

    test_case("deserialiser null") = [] {
        std::array<std::byte, 0> const buffer{};
        deserialiser<null> const deser = deserialise<null>(buffer);
        test_assert(deser.value() == null{});
    };


    static_assert(fixed_data_size_v<std::byte> == 1);

    test_case("serialiser byte") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<std::byte>(buffer);
        serialise_source<std::byte> const source{std::byte{0x12}};
        serialiser<std::byte> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        std::array<unsigned char, 1> const expected_buffer{0x12};
        test_assert(buffer_equal(buffer, expected_buffer));
        serialise_target const expected_new_target{buffer, 1, 0, 1, 1};
        test_assert(new_target == expected_new_target);
    };

    test_case("deserialiser byte") = [] {
        std::array<unsigned char, 1> const buffer{0xF4};
        deserialiser<std::byte> const deser = deserialise<std::byte>(as_const_bytes_span(buffer));
        test_assert(deser.value() == std::byte{0xF4});
    };


    static_assert(fixed_data_size_v<std::uint32_t> == 4);

    test_case("serialiser uint") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<std::uint32_t>(buffer);
        serialise_source<std::uint32_t> const source{43'834'534u};
        serialiser<std::uint32_t> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        std::array<unsigned char, 4> const expected_buffer{0xA6, 0xDC, 0x9C, 0x02};
        test_assert(buffer_equal(buffer, expected_buffer));
        serialise_target const expected_new_target{buffer, 4, 0, 4, 4};
        test_assert(new_target == expected_new_target);
    };

    test_case("deserialiser uint") = [] {
        std::array<unsigned char, 4> const buffer{0x01, 0x23, 0x45, 0x67};
        deserialiser<std::uint32_t> const deser = deserialise<std::uint32_t>(as_const_bytes_span(buffer));
        std::same_as<std::uint32_t> auto const value = deser.value();
        test_assert(value == 1'732'584'193ull);
    };


    static_assert(fixed_data_size_v<std::int64_t> == 8);

    test_case("serialiser int") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<std::int64_t>(buffer);
        serialise_source<std::int64_t> const source{-567'865'433'565'765ll};
        serialiser<std::int64_t> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        std::array<unsigned char, 8> const expected_buffer{0xBB, 0x55, 0x8D, 0x86, 0x87, 0xFB, 0xFD, 0xFF};
        test_assert(buffer_equal(buffer, expected_buffer));
        serialise_target const expected_new_target{buffer, 8, 0, 8, 8};
        test_assert(new_target == expected_new_target);
    };

    test_case("deserialiser int") = [] {
        std::array<unsigned char, 8> const buffer{0x01, 0xC2, 0x31, 0xB3, 0xFB, 0xFF, 0xFF, 0xFF};
        deserialiser<std::int64_t> const deser = deserialise<std::int64_t>(as_const_bytes_span(buffer));
        std::same_as<std::int64_t> auto const value = deser.value();
        test_assert(value == -18'468'453'887ll);
    };


    static_assert(fixed_data_size_v<bool> == 1);

    test_case("serialiser bool false") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<bool>(buffer);
        serialise_source<bool> const source{false};
        serialiser<bool> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        std::array<unsigned char, 1> const expected_buffer{0x00};
        test_assert(buffer_equal(buffer, expected_buffer));
        serialise_target const expected_new_target{buffer, 1, 0, 1, 1};
        test_assert(new_target == expected_new_target);
    };

    test_case("serialiser bool true") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<bool>(buffer);
        serialise_source<bool> const source{true};
        serialiser<bool> const ser;
        serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

        std::array<unsigned char, 1> const expected_buffer{0x01};
        test_assert(buffer_equal(buffer, expected_buffer));
        serialise_target const expected_new_target{buffer, 1, 0, 1, 1};
        test_assert(new_target == expected_new_target);
    };

    test_case("deserialiser bool false") = [] {
        std::array<unsigned char, 1> const buffer{0x00};
        deserialiser<bool> const deser = deserialise<bool>(as_const_bytes_span(buffer));
        test_assert(!deser.value());
    };

    test_case("deserialiser bool true") = [] {
        std::array<unsigned char, 1> const buffer{0x01};
        deserialiser<bool> const deser = deserialise<bool>(as_const_bytes_span(buffer));
        test_assert(deser.value());
    };


    if constexpr (sizeof(float) == 4 && std::numeric_limits<float>::is_iec559
            && (std::endian::native == std::endian::little || std::endian::native == std::endian::big)) {
        static_assert(floating_point<float>);

        static_assert(fixed_data_size_v<float> == 4);

        test_case("serialiser float") = [] {
            basic_serialise_buffer buffer;
            serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<float>(buffer);
            serialise_source<float> const source{-100'000'000.0f};
            serialiser<float> const ser;
            serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

            serialise_target const expected_new_target{buffer, 4, 0, 4, 4};
            test_assert(new_target == expected_new_target);
            std::array<unsigned char, 4> const expected_buffer{0x20, 0xBC, 0xBE, 0xCC};
            test_assert(buffer_equal(buffer, expected_buffer));
        };

        test_case("serialiser float constexpr") = [] {
            []() consteval {
                basic_serialise_buffer buffer;
                serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<float>(buffer);
                serialise_source<float> const source{-100'000'000.0f};
                serialiser<float> const ser;
                serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

                serialise_target const expected_new_target{buffer, 4, 0, 4, 4};
                test_assert(new_target == expected_new_target);
                std::array<unsigned char, 4> const expected_buffer{0x20, 0xBC, 0xBE, 0xCC};
                test_assert(buffer_equal(buffer, expected_buffer));
            }();
        };

        test_case("deserialiser float") = [] {
            std::array<unsigned char, 4> const buffer{0x20, 0xBC, 0xBE, 0xCC};
            deserialiser<float> const deser = deserialise<float>(as_const_bytes_span(buffer));
            test_assert(deser.value() == -100'000'000.0f);
        };

        test_case("deserialiser float constexpr") = [] {
            []() consteval {
                std::array<unsigned char, 4> const buffer{0x20, 0xBC, 0xBE, 0xCC};
                auto const buffer_bytes = uchar_array_to_bytes(buffer);
                deserialiser<float> const deser = deserialise<float>(buffer_bytes);
                test_assert(deser.value() == -100'000'000.0f);
            }();
        };
    }


    if constexpr (sizeof(double) == 8 && std::numeric_limits<double>::is_iec559
            && (std::endian::native == std::endian::little || std::endian::native == std::endian::big)) {
        static_assert(scalar<double>);

        static_assert(fixed_data_size_v<double> == 8);

        test_case("serialiser double") = [] {
            basic_serialise_buffer buffer;
            serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<double>(buffer);
            serialise_source<double> const source{12'345'678'900'000'000.0};
            serialiser<double> const ser;
            serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

            serialise_target const expected_new_target{buffer, 8, 0, 8, 8};
            test_assert(new_target == expected_new_target);
            std::array<unsigned char, 8> const expected_buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
            test_assert(buffer_equal(buffer, expected_buffer));
        };

        test_case("serialiser double constexpr") = [] {
            []() consteval {
                basic_serialise_buffer buffer;
                serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<double>(buffer);
                serialise_source<double> const source{12'345'678'900'000'000.0};
                serialiser<double> const ser;
                serialise_target<basic_serialise_buffer<>> const new_target = ser(source, target);

                serialise_target const expected_new_target{buffer, 8, 0, 8, 8};
                test_assert(new_target == expected_new_target);
                std::array<unsigned char, 8> const expected_buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
                test_assert(buffer_equal(buffer, expected_buffer));
            }();
        };

        test_case("deserialiser double") = [] {
            std::array<unsigned char, 8> const buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
            deserialiser<double> const deser = deserialise<double>(as_const_bytes_span(buffer));
            test_assert(deser.value() == 12'345'678'900'000'000.0);
        };

        test_case("deserialiser double constexpr") = [] {
            []() consteval {
                std::array<unsigned char, 8> const buffer{0x80, 0x3A, 0xAC, 0x2E, 0x2A, 0xEE, 0x45, 0x43};
                auto const buffer_bytes = uchar_array_to_bytes(buffer);
                deserialiser<double> const deser = deserialise<double>(buffer_bytes);
                test_assert(deser.value() == 12'345'678'900'000'000.0);
            }();
        };
    }


    test_case("auto_deserialise() scalar") = [] {
        std::array<unsigned char, 100> const buffer{0x01, 0x23, 0x45, 0x67, 0x11, 0x22, 0x33};
        deserialiser<std::uint32_t> const deser = deserialise<std::uint32_t>(as_const_bytes_span(buffer));
        std::same_as<std::uint32_t> auto const value = auto_deserialise(deser);
        test_assert(value == 1'732'584'193u);
    };

};
}

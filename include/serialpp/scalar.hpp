#pragma once

#include <algorithm>
#include <bit>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    // Serialisable type with no data and zero size.
    struct null {
        friend constexpr auto operator<=>(null, null) noexcept = default;       // Instances always compare equal.
    };


    // To be supported, float must be IEEE-754 binary32, and double must be IEEE-754 binary64.
    template<typename T>
    concept floating_point = ((std::same_as<T, float> && sizeof(T) == 4) || (std::same_as<T, double> && sizeof(T) == 8))
        && std::numeric_limits<T>::is_iec559;


    // Fundamental type. Has no variable data.
    template<typename T>
    concept scalar = std::same_as<T, null> || std::same_as<T, std::byte> || std::integral<T> || floating_point<T>;


    template<scalar S>
    class serialise_source<S> {
    public:
        [[no_unique_address]]   // For null.
        S value;

        constexpr serialise_source() noexcept :
            value{}
        {}

        constexpr serialise_source(S const value) noexcept :
            value{value}
        {}

        constexpr serialise_source& operator=(S const new_value) noexcept {
            value = new_value;
            return *this;
        }

        [[nodiscard]]
        constexpr operator S&() noexcept {
            return value;
        }

        [[nodiscard]]
        constexpr operator S const&() const noexcept {
            return value;
        }
    };


    template<scalar S>
    inline constexpr bool enable_auto_deserialise<S> = true;


    /*
        null:
            Has an empty representation, i.e. 0 bytes.
    */

    template<>
    struct fixed_data_size<null> : detail::size_t_constant<0> {};

    template<>
    struct serialiser<null> {
        static constexpr void serialise(serialise_source<null>, serialise_buffer auto&, std::size_t) {}

        static constexpr void serialise(serialise_source<null>, mutable_bytes_span, std::size_t) {}
    };

    template<>
    class deserialiser<null> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr null value() const {
            return null{};
        }
    };


    /*
        Byte:
            Represented as-is, as 8 bits.
    */

    template<>
    struct fixed_data_size<std::byte> : detail::size_t_constant<1> {};

    template<>
    struct serialiser<std::byte> {
        static constexpr void serialise(serialise_source<std::byte> const source, serialise_buffer auto& buffer,
                std::size_t const fixed_offset) {
            serialise(source, buffer.span(), fixed_offset);
        }

        static constexpr void serialise(serialise_source<std::byte> const source, mutable_bytes_span const buffer,
                std::size_t const fixed_offset) {
            auto const fixed_buffer = buffer.subspan(fixed_offset, 1);
            fixed_buffer[0] = source.value;
        }
    };

    template<>
    class deserialiser<std::byte> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        // Deserialises the value.
        [[nodiscard]]
        constexpr std::byte value() const {
            return _fixed_data()[0];
        }
    };


    /*
        Integers:
            Represented with two's complement and little endianness.
    */


    template<std::unsigned_integral U>
    struct fixed_data_size<U> : detail::size_t_constant<sizeof(U)> {};

    template<std::unsigned_integral U>
    struct serialiser<U> {
        static constexpr void serialise(serialise_source<U> const source, serialise_buffer auto& buffer,
                std::size_t const fixed_offset) {
            serialise(source, buffer.span(), fixed_offset);
        }

        static constexpr void serialise(serialise_source<U> const source, mutable_bytes_span const buffer,
                std::size_t const fixed_offset) {
            auto const fixed_buffer = buffer.subspan(fixed_offset, sizeof(U));
            if (std::is_constant_evaluated() || detail::is_mixed_endian) {
                // Mixed endianness, or constexpr (can't reinterpret_cast at compile time).
                U value = source.value;
                for (std::size_t i = 0; i < sizeof(U); ++i) {
                    fixed_buffer[i] = static_cast<std::byte>(value & 0xFFu);
                    value >>= 8;
                }
            }
            // Compilers aren't always smart enough to figure out the bit shifting thing above is just a copy.
            else if constexpr (detail::is_little_endian) {
                std::copy_n(
                    reinterpret_cast<std::byte const*>(&source.value), sizeof(U), fixed_buffer.begin());
            }
            else if constexpr (detail::is_big_endian) {
                auto const source_begin = reinterpret_cast<std::byte const*>(&source.value);
                auto const source_end = source_begin + sizeof(U);
                std::reverse_copy(source_begin, source_end, fixed_buffer.begin());
            }
            else {
                // Unreachable.
                static_assert(!std::unsigned_integral<U>);
            }
        }
    };

    template<std::unsigned_integral U>
    class deserialiser<U> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        // Deserialises the value.
        [[nodiscard]]
        constexpr U value() const {
            if (std::is_constant_evaluated() || detail::is_mixed_endian) {
                // Mixed endianness, or constexpr (can't reinterpret_cast at compile time).
                U value{};
                for (std::size_t i = 0; i < sizeof(U); ++i) {
                    value |= std::to_integer<U>(_fixed_data()[i]) << (i * 8);
                }
                return value;
            }
            // Compilers aren't always smart enough to figure out the bit shifting thing above is just a copy.
            else if constexpr (detail::is_little_endian) {
                U value{};
                std::copy_n(_fixed_data().begin(), sizeof(U), reinterpret_cast<std::byte*>(&value));
                return value;
            }
            else if constexpr (detail::is_big_endian) {
                U value{};
                std::reverse_copy(
                    _fixed_data().begin(), _fixed_data().begin() + sizeof(U), reinterpret_cast<std::byte*>(&value));
                return value;
            }
            else {
                // Unreachable.
                static_assert(!std::unsigned_integral<U>);
            }
        }
    };


    template<std::signed_integral S>
    struct fixed_data_size<S> : fixed_data_size<std::make_unsigned_t<S>> {};

    template<std::signed_integral S>
    struct serialiser<S> {
        static constexpr void serialise(serialise_source<S> const source, serialise_buffer auto& buffer,
                std::size_t const fixed_offset) {
            serialise(source, buffer.span(), fixed_offset);
        }

        static constexpr void serialise(serialise_source<S> const source, mutable_bytes_span const buffer,
                std::size_t const fixed_offset) {
            using unsigned_type = std::make_unsigned_t<S>;
            auto const unsigned_value = static_cast<unsigned_type>(source);
            serialpp::serialise(serialise_source<unsigned_type>{unsigned_value}, buffer, fixed_offset);
        }
    };

    template<std::signed_integral S>
    class deserialiser<S> : private deserialiser<std::make_unsigned_t<S>> {
    public:
        using deserialiser<std::make_unsigned_t<S>>::deserialiser;

        // Deserialises the value.
        [[nodiscard]]
        constexpr S value() const {
            return static_cast<S>(deserialiser<std::make_unsigned_t<S>>::value());
        }
    };


    /*
        Booleans:
            True is represented as the single byte 0x01. False is represented as the single byte 0x00.
    */

    template<>
    struct fixed_data_size<bool> : fixed_data_size<std::byte> {};

    template<>
    struct serialiser<bool> {
        static constexpr void serialise(serialise_source<bool> const source, serialise_buffer auto& buffer,
                std::size_t const fixed_offset) {
            serialise(source, buffer.span(), fixed_offset);
        }

        static constexpr void serialise(serialise_source<bool> const source, mutable_bytes_span const buffer,
                std::size_t const fixed_offset) {
            auto const byte_value = source ? std::byte{0x01} : std::byte{0x00};
            serialpp::serialise(serialise_source<std::byte>{byte_value}, buffer, fixed_offset);
        }
    };

    template<>
    class deserialiser<bool> : private deserialiser<std::byte> {
    public:
        using deserialiser<std::byte>::deserialiser;

        // Deserialises the value.
        [[nodiscard]]
        constexpr bool value() const {
            return std::to_integer<bool>(deserialiser<std::byte>::value());
        }
    };


    /*
        Floating point numbers:
            Represented in IEEE-754 binary format and little endianness. float is 32 bytes, double is 64 bytes.
    */

    template<floating_point F>
    struct fixed_data_size<F> : detail::size_t_constant<sizeof(F)> {};

    template<floating_point F>
    struct serialiser<F> {
        static constexpr void serialise(serialise_source<F> const source, serialise_buffer auto& buffer,
                std::size_t const fixed_offset) {
            serialise(source, buffer.span(), fixed_offset);
        }

        static constexpr void serialise(serialise_source<F> const source, mutable_bytes_span const buffer,
                std::size_t const fixed_offset) {
            // I think the std::bit_cast here is legit, since:
            //  - the size of the types is definitely the same
            //  - the fixed width integer types have no padding bits
            //  - we know F is IEEE-754 binary format, which has no padding bits
            if constexpr (sizeof(F) == 4) {
                auto const uint_value = std::bit_cast<std::uint32_t>(source.value);
                serialpp::serialise(serialise_source<std::uint32_t>{uint_value}, buffer, fixed_offset);
            }
            else if constexpr (sizeof(F) == 8) {
                auto const uint_value = std::bit_cast<std::uint64_t>(source.value);
                serialpp::serialise(serialise_source<std::uint64_t>{uint_value}, buffer, fixed_offset);
            }
            else {
                // Unreachable.
                static_assert(!floating_point<F>);
            }
        }
    };

    template<floating_point F>
    class deserialiser<F> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        // Deserialises the value.
        [[nodiscard]]
        constexpr F value() const {
            // I think the std::bit_cast here is legit, since:
            //  - the size of the types is definitely the same
            //  - the fixed width integer types have no padding bits
            //  - we know F is IEEE-754 binary format, which has no padding bits
            if constexpr (sizeof(F) == 4) {
                return std::bit_cast<F>(deserialise<std::uint32_t>(_buffer, _fixed_offset));
            }
            else if constexpr (sizeof(F) == 8) {
                return std::bit_cast<F>(deserialise<std::uint64_t>(_buffer, _fixed_offset));
            }
            else {
                // Unreachable.
                static_assert(!floating_point<F>);
            }
        }
    };

}

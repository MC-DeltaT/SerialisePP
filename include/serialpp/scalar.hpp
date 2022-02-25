#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    // Fundamental type. Has no variable data.
    template<typename T>
    concept Scalar = std::integral<T> || std::same_as<T, std::byte>;


    template<Scalar S>
    struct SerialiseSource<S> {
        S value;

        constexpr SerialiseSource() noexcept :
            value{}
        {}

        constexpr SerialiseSource(S value) noexcept :
            value{value}
        {}

        [[nodiscard]]
        constexpr operator S&() noexcept {
            return value;
        }

        [[nodiscard]]
        constexpr operator S const&() const noexcept {
            return value;
        }
    };


    // TODO: is it an issue that nonscalar serialisers adjust the fixed data offset, but scalar serialisers do not?


    /*
        Byte:
            Represented as-is, as 8 bits.
    */

    template<>
    struct FixedDataSize<std::byte> : SizeTConstant<1> {};

    template<>
    struct Serialiser<std::byte> {
        SerialiseTarget operator()(SerialiseSource<std::byte> source, SerialiseTarget target) const {
            auto const buffer = target.field_fixed_data();
            assert(buffer.size() >= 1);
            buffer[0] = source.value;
            return target;
        }
    };

    template<>
    class Deserialiser<std::byte> : public DeserialiserBase<std::byte> {
    public:
        using DeserialiserBase<std::byte>::DeserialiserBase;

        // Deserialises the value.
        [[nodiscard]]
        std::byte value() const {
            assert(_fixed_data.size() >= 1);
            return _fixed_data[0];
        }
    };


    /*
        Integers:
            Represented with two's complement and little endianness.
    */


    template<std::unsigned_integral U>
    struct FixedDataSize<U> : SizeTConstant<sizeof(U)> {};

    template<std::unsigned_integral U>
    struct Serialiser<U> {
        SerialiseTarget operator()(SerialiseSource<U> source, SerialiseTarget target) const {
            auto const buffer = target.field_fixed_data();
            assert(buffer.size() >= sizeof(U));
            // Compilers aren't always smart enough to figure out the bit shifting thing is just a copy.
            if constexpr (std::endian::native == std::endian::little) {
                std::memcpy(buffer.data(), &source.value, sizeof(U));
            }
            else if constexpr (std::endian::native == std::endian::big) {
                auto const source_begin = reinterpret_cast<std::byte const*>(&source.value);
                auto const source_end = source_begin + sizeof(U);
                std::reverse_copy(source_begin, source_end, buffer.begin());
            }
            else {
                // Mixed endianness.
                auto value = source.value;
                for (std::size_t i = 0; i < sizeof(U); ++i) {
                    buffer[i] = static_cast<std::byte>(value & 0xFFu);
                    value >>= 8;
                }
            }
            return target;
        };
    };

    template<std::unsigned_integral U>
    class Deserialiser<U> : public DeserialiserBase<U> {
    public:
        using DeserialiserBase<U>::DeserialiserBase;

        // Deserialises the value.
        [[nodiscard]]
        U value() const {
            assert(this->_fixed_data.size() >= sizeof(U));
            U value = 0;
            if constexpr (std::endian::native == std::endian::little) {
                std::memcpy(&value, this->_fixed_data.data(), sizeof(U));
            }
            else if constexpr (std::endian::native == std::endian::big) {
                std::reverse_copy(
                    this->_fixed_data.begin(), this->_fixed_data.end(), reinterpret_cast<std::byte*>(&value));
            }
            else {
                // Mixed endianness.
                for (std::size_t i = 0; i < sizeof(U); ++i) {
                    value |= std::to_integer<U>(this->_fixed_data[i]) << (i * 8);
                }
            }
            return value;
        }
    };


    template<std::signed_integral S>
    struct FixedDataSize<S> : FixedDataSize<std::make_unsigned_t<S>> {};

    template<std::signed_integral S>
    struct Serialiser<S> : Serialiser<std::make_unsigned_t<S>> {
        auto operator()(SerialiseSource<S> source, SerialiseTarget target) const {
            return Serialiser<std::make_unsigned_t<S>>{}(static_cast<std::make_unsigned_t<S>>(source), target);
        }
    };

    template<std::signed_integral S>
    class Deserialiser<S> : public Deserialiser<std::make_unsigned_t<S>> {
    public:
        using Deserialiser<std::make_unsigned_t<S>>::Deserialiser;

        // Deserialises the value.
        [[nodiscard]]
        S value() const {
            return static_cast<S>(Deserialiser<std::make_unsigned_t<S>>::value());
        }
    };


    /*
        Booleans:
            True is represented as the single byte 0x01. False is represented as the single byte 0x00.
    */

    template<>
    struct FixedDataSize<bool> : FixedDataSize<std::uint8_t> {};

    template<>
    struct Serialiser<bool> : Serialiser<std::uint8_t> {
        auto operator()(SerialiseSource<bool> source, SerialiseTarget target) const {
            return Serialiser<std::uint8_t>{}(static_cast<bool>(source), target);
        }
    };

    template<>
    class Deserialiser<bool> : public Deserialiser<std::uint8_t> {
    public:
        using Deserialiser<std::uint8_t>::Deserialiser;

        // Deserialises the value.
        [[nodiscard]]
        bool value() const {
            return static_cast<bool>(Deserialiser<std::uint8_t>::value());
        }
    };


    // If deserialiser is for a scalar, then returns the deserialised value, otherwise returns deserialiser unchanged.
    template<Serialisable T>
    [[nodiscard]]
    auto auto_deserialise_scalar(Deserialiser<T> const& deserialiser) {
        if constexpr (Scalar<T>) {
            return deserialiser.value();
        }
        else {
            return deserialiser;
        }
    }

}

#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <span>
#include <vector>

#include "utility.hpp"


namespace serialpp {

    static_assert(CHAR_BIT == 8, "Serialise++ only supports platforms with 8-bit bytes");


    using ConstBytesView = std::span<std::byte const>;
    using MutableBytesView = std::span<std::byte>;


    // Used for variable data offsets.
    using DataOffset = std::uint16_t;


    // The size in bytes of the fixed data section of a serialisable type (as an std::integral_constant).
    template<typename T>
    struct FixedDataSize;

    // The size in bytes of the fixed data section of a serialisable type.
    template<typename T>
    static inline constexpr auto FIXED_DATA_SIZE = FixedDataSize<T>::value;


    // Converts a value to its bytes representation.
    template<typename T>
    struct Serialiser;


    // Converts a bytes representation back to a value.
    template<typename T>
    struct Deserialiser;


    // Helper for implementing Deserialiser.
    struct DeserialiserBase {
        ConstBytesView fixed_data;      // The "fixed data section", which is the first FIXED_DATA_SIZE<T> bytes.
        ConstBytesView variable_data;   // The "variable data section", which is the rest of the buffer after the fixed data.
    
        // Struct initialisation is still broken in C++20 -.-
        DeserialiserBase(ConstBytesView fixed_data, ConstBytesView variable_data) :
            fixed_data{fixed_data}, variable_data{variable_data}
        {}
    };


    // Container/generator for a value to be serialised.
    template<typename T>
    struct SerialiseSource;


    struct SerialiseTarget;


    template<typename T>
    concept Serialisable = requires {
        { FixedDataSize<T>::value } -> std::convertible_to<std::size_t>;
        requires std::default_initializable<Serialiser<T>>;
        requires Callable<Serialiser<T>, SerialiseTarget, SerialiseSource<T>, SerialiseTarget>;
        requires std::constructible_from<Deserialiser<T>, ConstBytesView, ConstBytesView>;
    };


    // Buffer of bytes into which values are serialised.
    class SerialiseBuffer {
    public:
        SerialiseBuffer(std::size_t reserved_size = 4096) {
            _data.reserve(reserved_size);
        }

        // Initialises the buffer for a type T to be serialised into.
        // The returned SerialiseTarget is set up ready for Serialiser<T>.
        template<Serialisable T>
        SerialiseTarget initialise_for() {
            constexpr auto fixed_size = FIXED_DATA_SIZE<T>;
            _data.resize(fixed_size);
            return {this, fixed_size, 0, fixed_size, fixed_size};
        }

        // Adds more space to the end of the buffer.
        // Only Serialiser implementations should need to use this.
        void extend(std::size_t count) {
            _data.resize(_data.size() + count);
        }

        MutableBytesView span() {
            return {_data};
        }

        ConstBytesView span() const {
            return {_data};
        }

    private:
        std::vector<std::byte> _data;
    };


    // Helper for serialising into a SerialiseBuffer.
    // Keeps track of positions in the buffer which the current value should be serialised into (i.e. for complex types).
    struct SerialiseTarget {
        SerialiseBuffer* buffer;
        std::size_t fixed_size;				// Size of fixed data section
        std::size_t field_fixed_offset;     // Offset of current field from start of buffer.
        std::size_t field_fixed_size;		// Size of current field's fixed data.
        std::size_t field_variable_offset;	// Offset of current field from start of buffer.

        // Gets the buffer for the current field's fixed data.
        MutableBytesView field_fixed_data() const {
            assert(field_fixed_offset + field_fixed_size <= buffer->span().size());
            return buffer->span().subspan(field_fixed_offset).first(field_fixed_size);
        }

        // Invokes func with a SerialiseTarget set up for a field of type T in the fixed data section.
        // Returns a SerialiseTarget set up for the next serialisation.
        template<Serialisable T>
        SerialiseTarget push_fixed_field(Callable<SerialiseTarget, SerialiseTarget> auto&& func) const {
            constexpr auto field_fixed_size = FIXED_DATA_SIZE<T>;
            auto target = *this;
            target.field_fixed_size = field_fixed_size;
            target = func(target);
            // Note: recalculate exact fixed offset to avoid incrementing it multiple times for 1 field.
            target.field_fixed_offset = field_fixed_offset + field_fixed_size;
            return target;
        }

        // Invokes func with a SerialiseTarget set up to for a field of type T in the variable data section.
        // Returns a SerialiseTarget set up for the next serialisation.
        template<Serialisable T>
        SerialiseTarget push_variable_field(Callable<SerialiseTarget, SerialiseTarget> auto&& func) const {
            constexpr auto field_fixed_size = FIXED_DATA_SIZE<T>;

            assert(buffer->span().size() >= field_variable_offset);
            auto const free_variable_buffer = buffer->span().size() - field_variable_offset;
            if (free_variable_buffer < field_fixed_size) {
                buffer->extend(field_fixed_size - free_variable_buffer);
            }

            auto target = *this;
            target.field_fixed_offset = field_variable_offset;
            target.field_fixed_size = field_fixed_size;
            target.field_variable_offset = field_variable_offset + field_fixed_size;
            target = func(target);

            // All subfield data went into the variable data section and not the fixed data section,
            // so we revert the fixed section but keep the new variable section.
            auto result = *this;
            result.field_variable_offset = target.field_variable_offset;
            return result;
        }
    };

}

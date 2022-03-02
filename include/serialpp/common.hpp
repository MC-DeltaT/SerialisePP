#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <format>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <typeinfo>
#include <utility>
#include <vector>

#include "utility.hpp"


namespace serialpp {

    static_assert(CHAR_BIT == 8, "Serialise++ only supports platforms with 8-bit bytes");


    using ConstBytesView = std::span<std::byte const>;
    using MutableBytesView = std::span<std::byte>;


    // Used for variable data offsets.
    using DataOffset = std::uint16_t;


    // Safely casts to DataOffset.
    [[nodiscard]]
    inline DataOffset to_data_offset(std::size_t offset) {
        assert(std::cmp_less_equal(offset, std::numeric_limits<DataOffset>::max()));
        return static_cast<DataOffset>(offset);
    }


    class DeserialiseError : public std::runtime_error {
    public:
        using runtime_error::runtime_error;

        ~DeserialiseError() = 0;
    };

    inline DeserialiseError::~DeserialiseError() = default;


    // Indicates when a bytes buffer is the wrong size to deserialise the requested type.
    class BufferSizeError : public DeserialiseError {
    public:
        using DeserialiseError::DeserialiseError;

        ~BufferSizeError() = 0;
    };

    inline BufferSizeError::~BufferSizeError() = default;


    // Indicates when the fixed data buffer is the wrong size to deserialise the required type.
    class FixedBufferSizeError : public BufferSizeError {
    public:
        using BufferSizeError::BufferSizeError;
    };


    // Indicates when the variable data buffer is the wrong size to deserialise the required type.
    class VariableBufferSizeError : public BufferSizeError {
    public:
        using BufferSizeError::BufferSizeError;
    };


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
    class Deserialiser;


    // Helper for implementing Deserialiser.
    template<typename T>
    class DeserialiserBase {
    public:
        constexpr DeserialiserBase(ConstBytesView fixed_data, ConstBytesView variable_data) noexcept :
            _fixed_data{fixed_data}, _variable_data{variable_data}
        {
            check_deserialise_buffer_fixed_size<T>(_fixed_data);
        }

    protected:
        ConstBytesView _fixed_data;
        ConstBytesView _variable_data;

        // Throws VariableBufferSizeError if offset is not within the bounds of the variable data buffer.
        void _check_variable_offset(std::size_t offset) const {
            if (offset >= _variable_data.size()) {
                throw VariableBufferSizeError{
                    std::format("Variable data buffer of size {} is too small for variable data offset {}",
                        _variable_data.size(), offset)};
            }
        }
    };


    // Container/generator for a value to be serialised.
    template<typename T>
    class SerialiseSource;


    class SerialiseTarget;


    template<typename T>
    concept Serialisable = requires {
        { FixedDataSize<T>::value } -> std::convertible_to<std::size_t>;
        requires std::default_initializable<Serialiser<T>>;
        requires Callable<Serialiser<T>, SerialiseTarget, SerialiseSource<T>, SerialiseTarget>;
        requires std::constructible_from<Deserialiser<T>, ConstBytesView, ConstBytesView>;
    };


    // Throws BufferSizeError if buffer is too small to contain an instance of T.
    template<Serialisable T>
    void check_deserialise_buffer_fixed_size(ConstBytesView buffer) {
        if (buffer.size() < FIXED_DATA_SIZE<T>) {
            throw FixedBufferSizeError{
                std::format("Fixed data buffer of size {} is too small to deserialise type {} with fixed size {}",
                    buffer.size(), typeid(T).name(), FIXED_DATA_SIZE<T>)};
        }
    }


    // Buffer of bytes into which values are serialised.
    class SerialiseBuffer {
    public:
        SerialiseBuffer(std::size_t reserved_size = 4096) {
            _data.reserve(reserved_size);
        }

        // Initialises the buffer for a type T to be serialised into.
        // The returned SerialiseTarget is set up ready for Serialiser<T>.
        template<Serialisable T>
        SerialiseTarget initialise() {
            constexpr auto fixed_size = FIXED_DATA_SIZE<T>;
            _data.resize(fixed_size);
            return {*this, fixed_size, 0, fixed_size, fixed_size};
        }

        // Adds more space to the end of the buffer.
        // Only Serialiser implementations should need to use this.
        void extend(std::size_t count) {
            _data.resize(_data.size() + count);
        }

        [[nodiscard]]
        MutableBytesView span() {
            return {_data};
        }

        [[nodiscard]]
        ConstBytesView span() const {
            return {_data};
        }

    private:
        std::vector<std::byte> _data;
    };


    // Helper for serialising into a SerialiseBuffer.
    // Keeps track of positions in the buffer which the current value should be serialised into (i.e. for complex types).
    class SerialiseTarget {
    public:
        SerialiseTarget(SerialiseBuffer& buffer, std::size_t fixed_size, std::size_t field_fixed_offset,
                std::size_t field_fixed_size, std::size_t field_variable_offset) :
            _buffer{&buffer}, _fixed_size{fixed_size}, _field_fixed_offset{field_fixed_offset},
            _field_fixed_size{field_fixed_size}, _field_variable_offset{field_variable_offset}
        {
            auto const buffer_size = buffer.span().size();
            assert(fixed_size <= buffer_size);
            assert(field_fixed_offset + field_fixed_size <= buffer_size);
            assert(field_variable_offset >= fixed_size);
            assert(field_variable_offset <= buffer_size);
        }

        // Gets the buffer for the current field's fixed data.
        [[nodiscard]]
        MutableBytesView field_fixed_data() const {
            assert(_field_fixed_offset + _field_fixed_size <= _buffer->span().size());
            return _buffer->span().subspan(_field_fixed_offset).first(_field_fixed_size);
        }

        // Gets the offset of the current field's variable data relative to the end of the fixed data.
        [[nodiscard]]
        std::size_t relative_field_variable_offset() const {
            assert(_field_variable_offset >= _fixed_size);
            return _field_variable_offset - _fixed_size;
        }

        // TODO: make push functions higher-order functions?

        // Invokes func with a SerialiseTarget set up for a field of type T in the fixed data section.
        // Returns a SerialiseTarget set up for the next serialisation.
        template<Serialisable T>
        SerialiseTarget push_fixed_field(Callable<SerialiseTarget, SerialiseTarget> auto&& func) const {
            constexpr auto new_field_fixed_size = FIXED_DATA_SIZE<T>;
            auto target = *this;
            target._field_fixed_size = new_field_fixed_size;
            target = std::invoke(func, std::as_const(target));
            // Note: recalculate exact fixed offset and size to avoid changing it multiple times for the same field when
            // nesting push_fixed_field() calls.
            target._field_fixed_offset = _field_fixed_offset + new_field_fixed_size;
            assert(_field_fixed_size >= new_field_fixed_size);
            target._field_fixed_size = _field_fixed_size - new_field_fixed_size;
            return target;
        }

        // Invokes func with a SerialiseTarget set up to for count consecutive fields of type T in the variable data
        // section.
        // Returns a SerialiseTarget set up for the next serialisation.
        // If you are putting multiple fields into the variable data section, then you should use a single call to this
        // function to do so, so that the fields' fixed data is allocated together (e.g. for List). Otherwise, if the
        // fields allocate variable data, it will go inbetween the fixed data, and you won't know where each field
        // starts.
        template<Serialisable T>
        SerialiseTarget push_variable_fields(std::size_t count,
                Callable<SerialiseTarget, SerialiseTarget> auto&& func) const {
            auto const fields_fixed_size = FIXED_DATA_SIZE<T> * count;

            assert(_buffer->span().size() >= _field_variable_offset);
            auto const free_variable_buffer = _buffer->span().size() - _field_variable_offset;
            if (free_variable_buffer < fields_fixed_size) {
                _buffer->extend(fields_fixed_size - free_variable_buffer);
            }

            auto target = *this;
            target._field_fixed_offset = _field_variable_offset;
            target._field_fixed_size = fields_fixed_size;
            target._field_variable_offset = _field_variable_offset + fields_fixed_size;
            target = std::invoke(func, std::as_const(target));

            // All subfield data went into the variable data section and not the fixed data section,
            // so we revert the fixed section but keep the new variable section.
            auto result = *this;
            result._field_variable_offset = target._field_variable_offset;
            return result;
        }

        [[nodiscard]]
        friend auto operator<=>(SerialiseTarget const& lhs, SerialiseTarget const& rhs) = default;

    private:
        SerialiseBuffer* _buffer;
        std::size_t _fixed_size;            // Size of fixed data section.
        std::size_t _field_fixed_offset;    // Offset of current field's fixed data from start of buffer.
        std::size_t _field_fixed_size;		// Size of current field's fixed data.
        std::size_t _field_variable_offset;	// Offset of current field's variable data from start of buffer.
    };


    // Serialises an entire object.
    template<Serialisable T>
    void serialise(SerialiseSource<T> const& source, SerialiseBuffer& buffer) {
        auto const target = buffer.initialise<T>();
        Serialiser<T>{}(source, target);
    }


    // Obtains a deserialiser for a type.
    // buffer should contain exactly an instance of T (i.e. you can only use this function to deserialise the result of
    // serialising an entire object with serialise()).
    template<Serialisable T>
    [[nodiscard]]
    Deserialiser<T> deserialise(ConstBytesView buffer) {
        check_deserialise_buffer_fixed_size<T>(buffer);
        return Deserialiser<T>{buffer.first(FIXED_DATA_SIZE<T>), buffer.subspan(FIXED_DATA_SIZE<T>)};
    }

}

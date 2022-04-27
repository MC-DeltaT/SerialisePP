#pragma once

#include <cassert>
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
#include <type_traits>
#include <utility>
#include <vector>

#include "utility.hpp"


namespace serialpp {

    static_assert(CHAR_BIT == 8, "Serialise++ only supports platforms with 8-bit bytes");


    using const_bytes_span = std::span<std::byte const>;
    using mutable_bytes_span = std::span<std::byte>;


    // Used for variable data offsets.
    using data_offset_t = std::uint32_t;


    // The size in bytes of the fixed data section of a serialisable type (as an std::integral_constant).
    template<typename T>
    struct fixed_data_size;

    // The size in bytes of the fixed data section of a serialisable type.
    template<typename T>
    static inline constexpr std::size_t fixed_data_size_v = fixed_data_size<T>::value;


    // Converts value to their bytes representations.
    template<typename T>
    struct serialiser;


    // Converts bytes representations to C++ values.
    template<typename T>
    class deserialiser;


    // Container/generator for a value to be serialised.
    template<typename T>
    class serialise_source;


    // A container of bytes which data can be serialised into.
    // It requires:
    //   - span() member function which returns an std::span of the current content.
    //   - initialise(size) member function which initialises the buffer to a specified size.
    //   - extend(count) member function which extends the current buffer by count bytes, without discarding the
    //       previous content.
    template<typename T>
    concept serialise_buffer = requires (T t, T const t_const) {
        { t.span() } -> std::same_as<mutable_bytes_span>;
        { t_const.span() } -> std::same_as<const_bytes_span>;
        t.initialise(std::declval<std::size_t>());
        t.extend(std::declval<std::size_t>());
    };


    namespace detail {

        // Archetype implementation of serialise_buffer.
        // Only exists for concept checking, don't use anywhere else!
        struct serialise_buffer_archetype {
            mutable_bytes_span span();
            const_bytes_span span() const;
            void initialise(std::size_t);
            void extend(std::size_t);
        };

    }


    template<serialise_buffer Buffer>
    class serialise_target;


    // Specifies to a deserialiser constructor to skip checking the size of its fixed data buffer, e.g. because it is
    // part of a parent object that's already checked the size.
    struct no_fixed_buffer_check_t {};

    static inline constexpr no_fixed_buffer_check_t no_fixed_buffer_check{};


    template<typename T>
    concept serialisable = requires {
        { fixed_data_size<T>::value } -> std::same_as<std::size_t const&>;
        requires std::default_initializable<serialiser<T>>;
        requires detail::function<
            serialiser<T>,
            serialise_target<detail::serialise_buffer_archetype>,
            serialise_source<T>, serialise_target<detail::serialise_buffer_archetype>>;
        requires std::constructible_from<deserialiser<T>, const_bytes_span, const_bytes_span>;
        requires std::constructible_from<deserialiser<T>, no_fixed_buffer_check_t, const_bytes_span, const_bytes_span>;
    };


    // Helper for serialising into a serialise_buffer.
    // Keeps track of positions in the buffer which the current subobject should be serialised into (i.e. for complex
    // types).
    template<serialise_buffer Buffer>
    class serialise_target {
    public:
        struct push_fixed_subobject_context {
            std::size_t subobject_fixed_offset;
            std::size_t subobject_fixed_size;
            std::size_t new_subobject_fixed_size;

            [[nodiscard]]
            friend constexpr bool operator==(push_fixed_subobject_context const&, push_fixed_subobject_context const&)
                noexcept = default;
        };

        struct push_variable_subobjects_context {
            std::size_t root_fixed_size;
            std::size_t subobject_fixed_offset;
            std::size_t subobject_fixed_size;

            [[nodiscard]]
            friend constexpr bool operator==(
                push_variable_subobjects_context const&, push_variable_subobjects_context const&) noexcept = default;
        };

        constexpr serialise_target(Buffer& buffer, std::size_t root_fixed_size, std::size_t subobject_fixed_offset,
                std::size_t subobject_fixed_size, std::size_t subobject_variable_offset) :
            _buffer{&buffer}, _root_fixed_size{root_fixed_size}, _subobject_fixed_offset{subobject_fixed_offset},
            _subobject_fixed_size{subobject_fixed_size}, _subobject_variable_offset{subobject_variable_offset}
        {
            assert(_root_fixed_size <= _buffer->span().size());
            assert(_subobject_fixed_offset + _subobject_fixed_size <= _buffer->span().size());
            assert(_subobject_variable_offset >= _root_fixed_size);
            assert(_subobject_variable_offset <= _buffer->span().size());
        }

        // Gets the buffer for the current subobject's fixed data.
        [[nodiscard]]
        constexpr mutable_bytes_span subobject_fixed_data() const {
            assert(_subobject_fixed_offset + _subobject_fixed_size <= _buffer->span().size());
            return _buffer->span().subspan(_subobject_fixed_offset).first(_subobject_fixed_size);
        }

        // Gets the offset of the current subobject's variable data relative to the end of the fixed data.
        [[nodiscard]]
        constexpr std::size_t relative_subobject_variable_offset() const {
            assert(_subobject_variable_offset >= _root_fixed_size);
            return _subobject_variable_offset - _root_fixed_size;
        }

        // Sets up a serialise_target for a subobject of type T in the fixed data section.
        // This function conceptually "enters" the serialisation of the subobject.
        // When you're done serialising the subobject, you must call exit_fixed_subobject() with the returned
        // push_fixed_subobject_context instance to obtain a serialise_target ready for the next serialisation.
        template<serialisable T>
        [[nodiscard]]
        constexpr std::pair<serialise_target, push_fixed_subobject_context> enter_fixed_subobject() const {
            constexpr auto subobject_fixed_size = fixed_data_size_v<T>;

            push_fixed_subobject_context const context{
                _subobject_fixed_offset, _subobject_fixed_size, subobject_fixed_size};

            auto target = *this;
            target._subobject_fixed_size = subobject_fixed_size;

            return {target, context};
        }

        // "Exits" the serialisation of a subobject previously set up by enter_fixed_subobject().
        [[nodiscard]]
        constexpr serialise_target exit_fixed_subobject(push_fixed_subobject_context context) const {
            // Recalculate exact fixed offset and size to avoid changing it multiple times for the same subobject when
            // nesting push_fixed_subobject() calls.
            auto target = *this;
            target._subobject_fixed_offset = context.subobject_fixed_offset + context.new_subobject_fixed_size;
            assert(context.subobject_fixed_size >= context.new_subobject_fixed_size);
            target._subobject_fixed_size = context.subobject_fixed_size - context.new_subobject_fixed_size;
            return target;
        }

        // Convenience function for calling enter_fixed_subobject(), invoking func with the serialise_target, then
        // calling exit_fixed_subobject().
        template<serialisable T, detail::function<serialise_target, serialise_target> F>
        [[nodiscard]]
        constexpr serialise_target push_fixed_subobject(F&& func) const {
            auto const [target, context] = enter_fixed_subobject<T>();
            auto const new_target = std::invoke(std::forward<F>(func), target);
            return new_target.exit_fixed_subobject(context);
        }

        // Sets up serialise_target for count consecutive subobjects of type T in the variable data section.
        // This function conceptually "enters" the serialisation of the subobject.
        // When you're done serialising the subobject, you must call exit_variable_subobjects() with the returned
        // push_variable_subobjects_context instance to obtain a serialise_target ready for the next serialisation.
        // If you are putting multiple subobjects into the variable data section, then you should use a single call to
        // this function to do so, so that the subobjects' fixed data is allocated together (e.g. for dynamic_array).
        // Otherwise, if the subobjects allocate variable data, it will go inbetween the fixed data, and you won't know
        // where each subobject starts.
        template<serialisable T>
        [[nodiscard]]
        constexpr std::pair<serialise_target, push_variable_subobjects_context> enter_variable_subobjects(
                std::size_t count) const {
            auto const subobjects_fixed_size = fixed_data_size_v<T> * count;

            // Assume the buffer doesn't already have empty space at the end.
            // We could check for empty space but I don't think it will occur in any normal use cases.
            _buffer->extend(subobjects_fixed_size);

            push_variable_subobjects_context const context{
                _root_fixed_size, _subobject_fixed_offset, _subobject_fixed_size};

            auto target = *this;
            target._subobject_fixed_offset = _subobject_variable_offset;
            target._subobject_fixed_size = subobjects_fixed_size;
            target._subobject_variable_offset = _subobject_variable_offset + subobjects_fixed_size;

            return {target, context};
        }

        // "Exits" the serialisation of a subobjects previously set up by enter_variable_subobjects().
        [[nodiscard]]
        constexpr serialise_target exit_variable_subobjects(push_variable_subobjects_context context) const {
            // All subobject data went into the variable data section and not the fixed data section, so we revert the
            // fixed section but keep the new variable section.
            auto target = *this;
            target._root_fixed_size = context.root_fixed_size;
            target._subobject_fixed_offset = context.subobject_fixed_offset;
            target._subobject_fixed_size = context.subobject_fixed_size;
            return target;
        }

        // Convenience function for calling enter_variable_subobjects(), invoking func with the serialise_target, then
        // calling exit_variable_subobjects().
        template<serialisable T, detail::function<serialise_target, serialise_target> F>
        [[nodiscard]]
        constexpr serialise_target push_variable_subobjects(std::size_t count, F&& func) const {
            auto const [target, context] = enter_variable_subobjects<T>(count);
            auto const new_target = std::invoke(std::forward<F>(func), target);
            return new_target.exit_variable_subobjects(context);
        }

        [[nodiscard]]
        friend constexpr bool operator==(serialise_target const&, serialise_target const&) noexcept = default;

    private:
        Buffer* _buffer;
        std::size_t _root_fixed_size;           // Size of fixed data section.
        std::size_t _subobject_fixed_offset;    // Offset of current subobject's fixed data from start of buffer.
        std::size_t _subobject_fixed_size;		// Size of current subobject's fixed data.
        std::size_t _subobject_variable_offset;	// Offset of current subobject's variable data from start of buffer.
    };


    // Initialises buffer for a type T, and returns a serialise_target set up to serialise T.
    template<serialisable T, serialise_buffer Buffer>
    constexpr serialise_target<Buffer> initialise_buffer(Buffer& buffer) {
        constexpr auto fixed_size = fixed_data_size_v<T>;
        buffer.initialise(fixed_size);
        return {buffer, fixed_size, 0, fixed_size, fixed_size};
    }


    // Thrown when an object is too large to serialise.
    class object_size_error : public std::length_error {
    public:
        using std::length_error::length_error;
    };


    class deserialise_error : public std::runtime_error {
    public:
        using runtime_error::runtime_error;

        ~deserialise_error() = 0;
    };

    inline deserialise_error::~deserialise_error() = default;


    // Indicates when a bytes buffer is the wrong size to deserialise the requested type.
    class buffer_size_error : public deserialise_error {
    public:
        using deserialise_error::deserialise_error;

        ~buffer_size_error() = 0;
    };

    inline buffer_size_error::~buffer_size_error() = default;


    // Indicates when the fixed data buffer is the wrong size to deserialise the required type.
    class fixed_buffer_size_error : public buffer_size_error {
    public:
        using buffer_size_error::buffer_size_error;
    };


    // Indicates when the variable data buffer is the wrong size to deserialise the required type.
    class variable_buffer_size_error : public buffer_size_error {
    public:
        using buffer_size_error::buffer_size_error;
    };


    namespace detail {

        // Safely casts to data_offset_t. Throws object_size_error if the value is too big to be stored in a
        // data_offset_t.
        [[nodiscard]]
        constexpr data_offset_t to_data_offset(std::size_t offset) {
            if (std::cmp_less_equal(offset, std::numeric_limits<data_offset_t>::max())) {
                return static_cast<data_offset_t>(offset);
            }
            else {
                throw object_size_error{std::format("Variable data offset {} is too big to be represented", offset)};
            }
        }

    }


    // Throws buffer_size_error if buffer is too small to contain an instance of T.
    template<serialisable T>
    constexpr void check_fixed_buffer_size(const_bytes_span buffer) {
        if (buffer.size() < fixed_data_size_v<T>) {
            throw fixed_buffer_size_error{
                std::format("Fixed data buffer of size {} is too small to deserialise type {} with fixed size {}",
                    buffer.size(), typeid(T).name(), fixed_data_size_v<T>)};
        }
    }


    // Helper for implementing deserialiser.
    class deserialiser_base {
    public:
        constexpr deserialiser_base(no_fixed_buffer_check_t, const_bytes_span fixed_data,
                const_bytes_span variable_data) noexcept :
            _fixed_data{fixed_data}, _variable_data{variable_data}
        {}

    protected:
        const_bytes_span _fixed_data;
        const_bytes_span _variable_data;

        // Throws variable_buffer_size_error if offset is not within the bounds of the variable data buffer.
        constexpr void _check_variable_offset(std::size_t offset) const {
            if (offset >= _variable_data.size()) {
                throw variable_buffer_size_error{
                    std::format("Variable data buffer of size {} is too small for variable data offset {}",
                        _variable_data.size(), offset)};
            }
        }
    };


    // Specialising to true enables automatic deserialisation of T when requesting a T from a deserialiser for a
    // compound type. If enabled, the deserialiser must have a value() member function.
    template<serialisable T>
    static inline constexpr bool enable_auto_deserialise = false;

    // If enable_auto_deserialise<T> is true, then returns the deserialised value, otherwise returns deser unchanged.
    template<serialisable T>
    [[nodiscard]]
    constexpr decltype(auto) auto_deserialise(deserialiser<T> const& deser) {
        if constexpr (enable_auto_deserialise<T>) {
            return deser.value();
        }
        else {
            return deser;
        }
    }

    // Gets the type resulting from performing automatic deserialisation.
    template<serialisable T>
    using auto_deserialise_t =
        std::remove_cvref_t<decltype(auto_deserialise(std::declval<deserialiser<T> const>()))>;


    // Serialises an entire object.
    template<serialisable T>
    constexpr void serialise(serialise_source<T> const& source, serialise_buffer auto& buffer) {
        auto const target = initialise_buffer<T>(buffer);
        serialiser<T>{}(source, target);
    }


    // Obtains a deserialiser for a type.
    // buffer should contain exactly an instance of T (i.e. you can only use this function to deserialise the result of
    // serialising an entire object via serialise()).
    template<serialisable T>
    [[nodiscard]]
    constexpr deserialiser<T> deserialise(const_bytes_span buffer) {
        check_fixed_buffer_size<T>(buffer);
        return deserialiser<T>{
            no_fixed_buffer_check, buffer.first(fixed_data_size_v<T>), buffer.subspan(fixed_data_size_v<T>)};
    }

}

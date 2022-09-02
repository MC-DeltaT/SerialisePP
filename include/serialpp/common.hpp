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

#include "utility.hpp"


namespace serialpp {

    static_assert(CHAR_BIT == 8, "Serialise++ only supports platforms with 8-bit bytes");


    using const_bytes_span = std::span<std::byte const>;
    using mutable_bytes_span = std::span<std::byte>;


    using data_offset_t = std::uint32_t;


    // A container of bytes which data can be serialised into.
    // It requires:
    //   - initialise(size) member function which initialises the buffer for a new serialisation. size is the number of
    //       bytes required in the buffer up-front. Returns the new value of span().
    //   - extend(count) member function which extends the current buffer by count bytes, keeping the previous content.
    //       Returns the new value of span().
    //   - span() member function which returns an std::span of the buffer for the current serialisation. This span may
    //       be invalidated by calls to initialise() or extend().
    // Throughout this library, it's assumed that serialise_buffer types are not views - i.e. they are not required to
    // be movable or copyable, and are always passed by reference.
    template<typename T>
    concept serialise_buffer = requires(T& t, std::size_t const size) {
        { t.initialise(size) } -> std::same_as<mutable_bytes_span>;
        { t.extend(size) } -> std::same_as<mutable_bytes_span>;
        { t.span() } noexcept -> std::same_as<mutable_bytes_span>;
    };


    // The size in bytes of the fixed data section of a serialisable type (as an std::integral_constant).
    template<typename T>
    struct fixed_data_size;

    // The size in bytes of the fixed data section of a serialisable type.
    template<typename T>
    inline constexpr std::size_t fixed_data_size_v = fixed_data_size<T>::value;


    // Container/generator for a value to be serialised.
    // Can contain whatever data you want.
    // Should be movable for maximum usability and integration with Serialise++ (not strictly required, though).
    template<typename T>
    class serialise_source;


    // Converts value to their bytes representations.
    template<typename T>
    struct serialiser {
        // Converts the value in source to bytes and stores them in buffer.
        // fixed_offset specifies the offset from the beginning of buffer to a section of size fixed_data_size_v<T>
        // where the value's fixed data is to be written. This section is to be allocated by the caller.
        // The serialiser need not check if fixed_offset is valid, i.e. the caller must ensure:
        //   (fixed_offset + fixed_data_size_v<T>) <= buffer.span().size()
        static void serialise(serialise_source<T> const& source, serialise_buffer auto& buffer,
                std::size_t fixed_offset) = delete;

        // An optional overload for types which don't use variable data.
        static void serialise(serialise_source<T> const& source, mutable_bytes_span buffer, std::size_t fixed_offset)
                = delete;
    };


    // Converts bytes representations to values.
    template<typename T>
    class deserialiser {
    public:
        // buffer is the full bytes buffer.
        // fixed_offset is the index in buffer at which this value's fixed data begins.
        // The deserialiser need not check if fixed_offset is valid, i.e. the caller must ensure:
        //   (fixed_offset + fixed_data_size_v<T>) <= buffer.size()
        deserialiser(const_bytes_span buffer, std::size_t fixed_offset) = delete;
    };


    // Specialising to true enables automatic deserialisation of T.
    // If enabled, the deserialiser must have a value() member function.
    template<typename T>
    inline constexpr bool enable_auto_deserialise = false;


    namespace detail {

        // Archetype implementation of serialise_buffer.
        // Only exists for concept checking, don't use anywhere else!
        struct serialise_buffer_archetype {
            serialise_buffer_archetype() = delete;
            serialise_buffer_archetype(serialise_buffer_archetype&&) = delete;
            serialise_buffer_archetype(serialise_buffer_archetype const&) = delete;
            ~serialise_buffer_archetype() = delete;

            serialise_buffer_archetype& operator=(serialise_buffer_archetype&&) = delete;
            serialise_buffer_archetype& operator=(serialise_buffer_archetype const&) = delete;

            mutable_bytes_span initialise(std::size_t);
            mutable_bytes_span extend(std::size_t);
            mutable_bytes_span span() noexcept;
        };

    }


    template<typename T>
    concept serialisable = requires(serialise_source<T> const source, detail::serialise_buffer_archetype buffer,
            std::size_t const fixed_offset) {
        { fixed_data_size<T>::value } -> std::same_as<std::size_t const&>;
        serialiser<T>::serialise(source, buffer, fixed_offset);
        requires std::constructible_from<deserialiser<T>, const_bytes_span const&, std::size_t const&>;
        { enable_auto_deserialise<T> } -> std::same_as<bool const&>;
    };

    // Serialisable type which never uses any variable data.
    template<typename T>
    concept fixed_size_serialisable =
        serialisable<T>
        && requires(serialise_source<T> const source, mutable_bytes_span const buffer, std::size_t const fixed_offset) {
            serialiser<T>::serialise(source, buffer, fixed_offset);
        };

    // Serialisable type which may use variable data.
    template<typename T>
    concept variable_size_serialisable = serialisable<T> && !fixed_size_serialisable<T>;


    // A buffer with which serialiser<T> can be invoked.
    // Either a type which satisfies serialise_buffer, or if T satisfies fixed_size_serialisable, mutable_bytes_span.
    template<typename B, typename T>
    concept buffer_for =
        (serialisable<T> && serialise_buffer<B>)
        || (fixed_size_serialisable<T> && std::same_as<std::remove_cvref_t<B>, mutable_bytes_span>);


    namespace detail {

        template<serialisable T>
        struct deserialise_type : std::type_identity<deserialiser<T>> {};

        template<serialisable T> requires enable_auto_deserialise<T>
        struct deserialise_type<T> : std::decay<decltype(std::declval<deserialiser<T> const&>().value())> {};

    }


    // Gets the type resulting from deserialisation.
    // If T doesn't enable automatic deserialisation, the type is just deserialiser<T>.
    // If T does enable automatic deserialisation, the type is the return type of the deserialiser's value() function.
    template<serialisable T>
    using deserialise_t = detail::deserialise_type<T>::type;


    // Base for exceptions relating to serialisation.
    class serialise_error : public std::exception {
    public:
        virtual ~serialise_error() = 0;
    };

    inline serialise_error::~serialise_error() = default;


    // Thrown when an object is too large to serialise.
    class object_size_error : public serialise_error {
    public:
        object_size_error(std::string message) noexcept :
            _message{std::move(message)}
        {}

        [[nodiscard]]
        char const* what() const noexcept override {
            return _message.c_str();
        }

    private:
        std::string _message;
    };


    // Base for exceptions relating to deserialisation.
    class deserialise_error : public std::exception {
    public:
        virtual ~deserialise_error() = 0;
    };

    inline deserialise_error::~deserialise_error() = default;


    // Thrown when deserialisation would require out-of-bounds buffer access.
    // This can occur if the buffer is too small or a variable data offset is invalid.
    class buffer_bounds_error : public deserialise_error {
    public:
        buffer_bounds_error(std::string message) noexcept :
            _message{std::move(message)}
        {}

        [[nodiscard]]
        char const* what() const noexcept override {
            return _message.c_str();
        }

    private:
        std::string _message;
    };


    namespace detail {

        // Safely casts to data_offset_t. Throws object_size_error if the value is too big to be stored in a
        // data_offset_t.
        [[nodiscard]]
        constexpr data_offset_t to_data_offset(std::size_t const offset) {
            if (std::cmp_less_equal(offset, std::numeric_limits<data_offset_t>::max())) {
                return static_cast<data_offset_t>(offset);
            }
            else {
                throw object_size_error{std::format("Data offset {} is too big to be represented", offset)};
            }
        }


        // Throws buffer_bounds_error if buffer is too small to contain an instance of T starting at the specified
        // offset.
        template<serialisable T>
        constexpr void check_buffer_size_for(const_bytes_span const buffer, std::size_t const offset) {
            if (offset > buffer.size() || buffer.size() - offset < fixed_data_size_v<T>) {
                throw buffer_bounds_error{
                    std::format(
                        "Data buffer of size {} is too small to deserialise type {} with fixed size {} at offset {}",
                        buffer.size(), typeid(T).name(), fixed_data_size_v<T>, offset)};
            }
        }

    }


    // Invokes func with the common optimal buffer type for Ts.
    // Extracting buffer.span() high in the tree of serialiser calls for fixed_size_serialisable types helps the
    // compiler generate the most optimised machine code.
    template<serialisable... Ts, serialise_buffer B, std::invocable<B&> F>
        requires (variable_size_serialisable<Ts> || ...)
    constexpr decltype(auto) with_buffer_for(B& buffer, F&& func) noexcept(std::is_nothrow_invocable_v<F, B&>) {
        return std::invoke(std::forward<F>(func), buffer);
    }

    template<fixed_size_serialisable... Ts, serialise_buffer B, std::invocable<mutable_bytes_span const&> F>
    constexpr decltype(auto) with_buffer_for(B& buffer, F&& func)
            noexcept(std::is_nothrow_invocable_v<F, mutable_bytes_span const&>) {
        auto const buffer_span = buffer.span();
        return std::invoke(std::forward<F>(func), buffer_span);
    }

    template<fixed_size_serialisable... Ts, std::invocable<mutable_bytes_span const&> F>
    constexpr decltype(auto) with_buffer_for(mutable_bytes_span const buffer, F&& func)
            noexcept(std::is_nothrow_invocable_v<F, mutable_bytes_span const&>) {
        return std::invoke(std::forward<F>(func), buffer);
    }


    // Invokes func with a fixed offset set up for a subobject of type T in the fixed data section.
    // Returns a fixed offset ready for the next serialisation.
    template<serialisable T, std::invocable<std::size_t const&> F>
    [[nodiscard]]
    constexpr std::size_t push_fixed_subobject(std::size_t const fixed_offset, F&& func)
            noexcept(std::is_nothrow_invocable_v<F, std::size_t const&>) {
        std::invoke(std::forward<F>(func), fixed_offset);
        // Recalculate the fixed offset to avoid changing it multiple times for the same subobject when nesting
        // push_fixed_subobject() calls.
        auto const next_fixed_offset = fixed_offset + fixed_data_size_v<T>;
        return next_fixed_offset;
    }

    // Invokes func with a fixed offset set up for count consecutive subobjects of type T in the variable data section.
    // If you are putting multiple subobjects into the variable data section, then you should use a single call to
    // this function to do so, so that the subobjects' fixed data is allocated together (e.g. for dynamic_array).
    // Otherwise, if the subobjects allocate variable data, it will go inbetween the fixed data, and you won't know
    // where each subobject starts.
    template<serialisable T, std::invocable<std::size_t const&> F>
    constexpr void push_variable_subobjects(std::size_t const count, serialise_buffer auto& buffer, F&& func) {
        // Assume the buffer doesn't have unused space at the end.
        auto const subobject_fixed_offset = buffer.span().size();
        buffer.extend(fixed_data_size_v<T> * count);

        std::invoke(std::forward<F>(func), subobject_fixed_offset);
    }


    // Helper for implementing deserialiser.
    class deserialiser_base {
    public:
        constexpr deserialiser_base(const_bytes_span const buffer, std::size_t const fixed_offset) :
            _buffer{buffer}, _fixed_offset{fixed_offset}
        {}

    protected:
        const_bytes_span _buffer;
        std::size_t _fixed_offset;

        // Obtains a const_bytes_span of this object's fixed data.
        constexpr const_bytes_span _fixed_data() const {
            return _buffer.subspan(_fixed_offset);
        }
    };


    // If enable_auto_deserialise<T> is true, then returns the deserialised value, otherwise just returns deser.
    template<serialisable T>
    [[nodiscard]]
    constexpr deserialise_t<T> auto_deserialise(deserialiser<T> const& deser) {
        if constexpr (enable_auto_deserialise<T>) {
            return deser.value();
        }
        else {
            return deser;
        }
    }


    // Invokes serialiser<T> with the optimal buffer for T.
    template<serialisable T>
    constexpr void serialise(serialise_source<T> const& source, buffer_for<T> auto&& buffer,
            std::size_t const fixed_offset) {
        // Explicit lambda return type is required here due to weirdness with value-dependence and instantiation during
        // concept checking.
        with_buffer_for<T>(buffer, [&source, fixed_offset](auto&& buffer) -> void {
            if constexpr (fixed_size_serialisable<T>) {
                // Without the explicit return type, this can get instantiated even if the "if constexpr" is false.
                assert(fixed_offset <= buffer.size());
            }
            else {
                assert(fixed_offset <= buffer.span().size());
            }
            serialiser<T>::serialise(source, buffer, fixed_offset);
        });
    }


    namespace detail {

        // Partially applies serialise() with source and buffer, leaving the fixed offset argument unbound.
        // Useful for push_fixed_subobject() and push_variable_subobjects().
        template<serialisable T, buffer_for<T> B>
            // Don't allow rvalue serialise_buffer, since in general lifetime won't be extended into lambda capture.
            requires std::is_lvalue_reference_v<B> || std::same_as<std::remove_cvref_t<B>, mutable_bytes_span>
        constexpr auto bind_serialise(serialise_source<T> const& source, B&& buffer) {
            return [&source, &buffer](std::size_t const fixed_offset) {
                serialise(source, buffer, fixed_offset);
            };
        }

    }


    // Initialises the buffer and serialises an entire object beginning from the start of the buffer.
    template<serialisable T>
    constexpr void serialise(serialise_source<T> const& source, serialise_buffer auto& buffer) {
        buffer.initialise(fixed_data_size_v<T>);
        serialise(source, buffer, 0);
    }


    // Deserialises a type from a buffer.
    // buffer is the full bytes buffer.
    // fixed_offset is the index in buffer at which this value's fixed data begins.
    // Buffer bounds checking is performed.
    template<serialisable T>
    [[nodiscard]]
    constexpr deserialise_t<T> deserialise(const_bytes_span const buffer, std::size_t const fixed_offset = 0) {
        detail::check_buffer_size_for<T>(buffer, fixed_offset);
        deserialiser<T> const deser{buffer, fixed_offset};
        return auto_deserialise(deser);
    }

}

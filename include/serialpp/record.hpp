#pragma once

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        record:
            Represented as the result of contiguously serialising each field in order of declaration. Inherited fields
            are ordered before those declared in the derived record.
    */


    // Specifies a field of a record.
    template<constant_string Name, serialisable T>
    struct field {
        using type = T;

        static constexpr constant_string name = Name;
    };


    namespace detail {

        // TODO: can we get a better name for this that's not the same as serialpp::field?
        template<typename T>
        concept field = requires {
            requires serialisable<typename T::type>;
            { T::name } -> std::same_as<constant_string<decltype(T::name)::size> const&>;
        };


        template<typename T>
        inline constexpr bool is_field_list = false;

        template<field... Fs>
        inline constexpr bool is_field_list<type_list<Fs...>> = true;


        // A type_list of field.
        template<typename T>
        concept field_list = is_field_list<T>;

    }


    namespace detail {

        // Implementation of record_type because concepts can't be recursive or forward declared.
        template<typename T>
        struct is_record_type;

    }


    // Used to specify the base record for record inheritance.
    template<typename T> requires detail::is_record_type<T>::value      // same as record_type<T>
    struct base {};


    namespace detail {

        // True if T is a type_list of record.
        template<typename T>
        inline constexpr bool is_record_list = false;

        template<typename... Ts> requires (is_record_type<Ts>::value && ...)
        inline constexpr bool is_record_list<type_list<Ts...>> = true;


        template<typename T>
        struct is_record_type : std::bool_constant<
            requires {
                requires is_record_list<typename T::bases>;
                requires field_list<typename T::base_fields>;
                requires field_list<typename T::own_fields>;
                requires field_list<typename T::fields>;
            }
        > {};

    }


    // Any record or subclass of record.
    template<typename T>
    concept record_type = detail::is_record_type<T>::value;


    namespace detail {

        // Checks if a list of fields contains a field with the specified name.
        template<field_list Fields, constant_string Name>
        inline constexpr bool contains_field =
            Fields::head::name == Name || contains_field<typename Fields::tail, Name>;

        template<constant_string Name>
        inline constexpr bool contains_field<type_list<>, Name> = false;


        // Checks if all fields have unique names.
        template<field_list Fields>
        inline constexpr bool all_fields_unique =
            !contains_field<typename Fields::tail, Fields::head::name> && all_fields_unique<typename Fields::tail>;

        template<>
        inline constexpr bool all_fields_unique<type_list<>> = true;

    }


    // Base for compound structures that can contain arbitrary serialisable fields.
    // Alias or inherit from this to implement your own record type.
    // struct my_record : record<
    //     field<"foo", std::int32_t>,
    //     field<"bar", optional<std::uint64_t>>,
    //     field<"qux", dynamic_array<std::int8_t>>
    //  > {};
    // Single inheritance is supported by using base<T> for the first argument.
    // struct my_derived_record : record<
    //     base<my_record>,
    //     field<"extra", float>
    //  > {};
    // All fields of a record must have unique names.
    template<typename... Args>
    struct record;


    template<detail::field... Fs>
    struct record<Fs...> {
        using bases = type_list<>;
        using base_fields = type_list<>;
        using own_fields = type_list<Fs...>;
        using fields = own_fields;

        static_assert(detail::all_fields_unique<fields>);
    };


    template<record_type B, detail::field... Fs>
    struct record<base<B>, Fs...> {
        // All base record types from most to least derived.
        using bases = detail::type_list_prepend<B, typename B::bases>::type;
        // All fields of the base record type (includes fields from any of its bases too).
        using base_fields = B::fields;
        // Fields declared only in this record, i.e. not inherited.
        using own_fields = type_list<Fs...>;
        // All fields (includes all base record fields too).
        using fields = detail::type_list_concat<base_fields, own_fields>::type;

        static_assert(detail::all_fields_unique<fields>);
    };


    // Checks if a record has a field with the specified name.
    template<record_type R, constant_string Name>
    inline constexpr bool has_field = detail::contains_field<typename R::fields, Name>;


    namespace detail {

        template<field_list Fields, constant_string Name>
        struct field_type
            : std::conditional_t<Fields::head::name == Name,
                std::type_identity<typename Fields::head::type>,
                field_type<typename Fields::tail, Name>> {};

    }


    // Gets the type of the record field with the specified name.
    // If there is no such field, the usage is ill-formed.
    template<record_type R, constant_string Name>
    using field_type = detail::field_type<typename R::fields, Name>::type;


    // Checks if record R1 inherits from (directly or indirectly) or is the same as record R2.
    template<class R1, class R2>
    concept record_derived_from =
        record_type<R1> && record_type<R2>
        && (std::same_as<R1, R2> || detail::type_list_contains<typename R1::bases, R2>);


    namespace detail {

        template<field_list Fields>
        inline constexpr std::size_t fields_fixed_data_size =
            fixed_data_size_v<typename Fields::head::type> + fields_fixed_data_size<typename Fields::tail>;

        template<>
        inline constexpr std::size_t fields_fixed_data_size<type_list<>> = 0;

    }


    template<record_type R>
    struct fixed_data_size<R> : detail::size_t_constant<detail::fields_fixed_data_size<typename R::fields>> {};


    namespace detail {

        // Gets the offset of a record field from the beginning of the fixed data section.
        // If there is no field with the specified name, the usage is ill-formed.
        template<field_list Fields, constant_string Name> requires contains_field<Fields, Name>
        [[nodiscard]]
        consteval std::size_t named_field_offset() noexcept {
            if constexpr (Fields::head::name == Name) {
                return 0;
            }
            else {
                return fixed_data_size_v<typename Fields::head::type>
                    + named_field_offset<typename Fields::tail, Name>();
            }
        }


        // Gets the name of a field by index.
        template<field_list Fields, std::size_t Index>
        inline constexpr auto indexed_field_name = type_list_element<Fields, Index>::type::name;


        template<constant_string Name, serialisable T>
        struct record_serialise_source_element {
            [[no_unique_address]]   // For null.
            serialise_source<T> value;
        };


        template<constant_string Name, typename T>
        [[nodiscard]]
        constexpr serialise_source<T>& record_serialise_source_get(
                record_serialise_source_element<Name, T>& source) noexcept {
            return source.value;
        }

        template<constant_string Name, typename T>
        [[nodiscard]]
        constexpr serialise_source<T> const& record_serialise_source_get(
                record_serialise_source_element<Name, T> const& source) noexcept {
            return source.value;
        }


        template<field_list Fields>
        class record_serialise_source;

        // Inspired by "Beyond struct: Meta-programming a struct Replacement in C++20" by John Bandela: https://youtu.be/FXfrojjIo80
        template<field... Fs>
        class record_serialise_source<type_list<Fs...>>
            : public record_serialise_source_element<Fs::name, typename Fs::type>... {
        public:
            record_serialise_source() = default;
            record_serialise_source(record_serialise_source&&) = default;
            record_serialise_source(record_serialise_source const&) = default;

            constexpr record_serialise_source(serialise_source<typename Fs::type>&&... elements)
                    noexcept((std::is_nothrow_constructible_v<
                        record_serialise_source_element<Fs::name, typename Fs::type>,
                        serialise_source<typename Fs::type>> && ...))
                    requires (sizeof...(Fs) > 0) :
                record_serialise_source_element<Fs::name, typename Fs::type>{
                    std::move(elements)}...
            {}

            record_serialise_source& operator=(record_serialise_source&&) = default;
            record_serialise_source& operator=(record_serialise_source const&) = default;

            // Gets a field by name.
            template<constant_string Name> requires contains_field<type_list<Fs...>, Name>
            [[nodiscard]]
            constexpr auto& get() noexcept {
                return record_serialise_source_get<Name>(*this);
            }

            // Gets a field by name.
            template<constant_string Name> requires contains_field<type_list<Fs...>, Name>
            [[nodiscard]]
            constexpr auto const& get() const noexcept {
                return record_serialise_source_get<Name>(*this);
            }

            // Gets a field by index.
            template<std::size_t Index> requires (Index < sizeof...(Fs))
            constexpr auto& get() noexcept {
                return get<indexed_field_name<Fs, Index>>();
            }

            // Gets a field by index.
            template<std::size_t Index> requires (Index < sizeof...(Fs))
            constexpr auto const& get() const noexcept {
                return get<indexed_field_name<Fs, Index>>();
            }
        };

    }


    template<record_type R>
    class serialise_source<R> : public detail::record_serialise_source<typename R::fields> {
    public:
        using detail::record_serialise_source<typename R::fields>::record_serialise_source;
    };


    namespace detail {

        template<typename B, class Fields>
        inline constexpr bool is_buffer_for_fields = false;

        template<typename B, field... Fs>
        inline constexpr bool is_buffer_for_fields<B, type_list<Fs...>> = (buffer_for<B, typename Fs::type> && ...);

    }


    template<record_type R>
    struct serialiser<R> {
        template<typename B> requires detail::is_buffer_for_fields<B, typename R::fields>
        static constexpr void serialise(serialise_source<R> const& source, B&& buffer, std::size_t fixed_offset) {
            [&source, &buffer, &fixed_offset] <std::size_t... Is> (std::index_sequence<Is...>) {
                (_serialise<Is>(source, buffer, fixed_offset), ...);
            }(std::make_index_sequence<R::fields::size>{});
        }

    private:
        template<std::size_t Index, typename B>
            requires (Index < R::fields::size) && detail::is_buffer_for_fields<B, typename R::fields>
        static constexpr void _serialise(serialise_source<R> const& source, B&& buffer, std::size_t& fixed_offset) {
            using field = typename detail::type_list_element<typename R::fields, Index>::type;
            fixed_offset = push_fixed_subobject<typename field::type>(fixed_offset,
                detail::bind_serialise(source.get<field::name>(), buffer));
        }
    };


    template<record_type R>
    class deserialiser<R> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        // Gets a field by name.
        // If there is no field with the specified name, the usage is ill-formed.
        template<constant_string Name> requires has_field<R, Name>
        [[nodiscard]]
        constexpr deserialise_t<field_type<R, Name>> get() const {
            constexpr auto offset = detail::named_field_offset<typename R::fields, Name>();
            using field_t = field_type<R, Name>;
            return _get<offset, field_t>();
        }

        // Gets a field by zero-based index.
        // If Index is out of bounds, the usage is ill-formed.
        template<std::size_t Index> requires (Index < R::fields::size)
        [[nodiscard]]
        constexpr auto get() const {
            constexpr auto name = detail::indexed_field_name<typename R::fields, Index>;
            return get<name>();
        }

        // Conversion to deserialiser for any base record type.
        template<serialisable T> requires record_derived_from<R, T>
        constexpr operator deserialiser<T>() const noexcept {
            static_assert(fixed_data_size_v<T> <= fixed_data_size_v<R>);
            return deserialise<T>(_buffer, _fixed_offset);
        }

    private:
        template<std::size_t Offset, serialisable T> requires (Offset + fixed_data_size_v<T> <= fixed_data_size_v<R>)
        [[nodiscard]]
        constexpr deserialise_t<T> _get() const {
            return deserialise<T>(_buffer, _fixed_offset + Offset);
        }
    };

}


namespace std {

    // Structured binding support


    template<::serialpp::record_type R>
    struct tuple_size<::serialpp::deserialiser<R>> : integral_constant<size_t, R::fields::size> {};


    template<size_t I, ::serialpp::record_type R>
    struct tuple_element<I, ::serialpp::deserialiser<R>>
        : type_identity<
            ::serialpp::deserialise_t<
                typename ::serialpp::detail::type_list_element<typename R::fields, I>::type::type>> {};

}

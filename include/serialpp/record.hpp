#pragma once

#include <cassert>
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
            Represented as the result of contiguously serialising each field.
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


        // Checks if a type is a type_list of field.
        template<typename T>
        static inline constexpr bool is_field_list = false;

        template<field... Fs>
        static inline constexpr bool is_field_list<type_list<Fs...>> = true;


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

        // Checks if a type is a type_list of record.
        template<typename T>
        static inline constexpr bool is_record_list = false;

        template<typename... Ts>
        static inline constexpr bool is_record_list<type_list<Ts...>> = (is_record_type<Ts>::value && ...);


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
    template<class... Args>
    struct record;


    template<detail::field... Fs>
    struct record<Fs...> {
        using bases = type_list<>;
        using base_fields = type_list<>;
        using own_fields = type_list<Fs...>;
        using fields = own_fields;
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
    };


    namespace detail {

        // Checks if a list of fields contains a field with the specified name.
        template<field_list Fields, constant_string Name>
        [[nodiscard]]
        consteval bool contains_field() noexcept {
            if constexpr (Fields::size == 0) {
                return false;
            }
            else if constexpr (Fields::head::name == Name) {
                return true;
            }
            else {
                return contains_field<typename Fields::tail, Name>();
            }
        }


        template<field_list Fields, constant_string Name> requires (contains_field<Fields, Name>())
        [[nodiscard]]
        consteval auto field_type() noexcept {
            if constexpr (Fields::head::name == Name) {
                return typename Fields::head::type{};
            }
            else {
                return field_type<typename Fields::tail, Name>();
            }
        }

    }


    // Checks if a record has a field with the specified name.
    template<record_type R, constant_string Name>
    static inline constexpr bool has_field = detail::contains_field<typename R::fields, Name>();


    // Gets the type of the record field with the specified name.
    // If there is no such field, a compile error occurs.
    template<record_type R, constant_string Name>
    using field_type = decltype(detail::field_type<typename R::fields, Name>());


    // Checks if record R1 inherits from (directly or indirectly) or is the same as record R2.
    template<class R1, class R2>
    concept record_derived_from =
        record_type<R1> && record_type<R2>
        && (std::same_as<R1, R2> || detail::type_list_contains<typename R1::bases, R2>());


    namespace detail {

        template<field_list Fields>
        static inline constexpr std::size_t fields_fixed_data_size =
            fixed_data_size_v<typename Fields::head::type> + fields_fixed_data_size<typename Fields::tail>;

        template<>
        inline constexpr std::size_t fields_fixed_data_size<type_list<>> = 0;

    }


    template<record_type R>
    struct fixed_data_size<R> : detail::size_t_constant<detail::fields_fixed_data_size<typename R::fields>> {};


    namespace detail {

        // Gets the offset of a record field from the beginning of the fixed data section.
        // If there is no field with the specified name, a compile error occurs.
        template<field_list Fields, constant_string Name> requires (contains_field<Fields, Name>())
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


        // Gets the offset of a record field from the beginning of the fixed data section.
        // If Index is out of bounds, a compile error occurs.
        template<field_list Fields, std::size_t Index> requires (Index < Fields::size)
        static inline constexpr std::size_t indexed_field_offset =
            fixed_data_size_v<typename Fields::head::type> + indexed_field_offset<typename Fields::tail, Index - 1>;

        template<field_list Fields>
        static inline constexpr std::size_t indexed_field_offset<Fields, 0> = 0;


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
        template<field... Fields>
        class record_serialise_source<type_list<Fields...>>
            : public record_serialise_source_element<Fields::name, typename Fields::type>... {
        public:
            constexpr record_serialise_source() = default;
            constexpr record_serialise_source(record_serialise_source&&) = default;
            constexpr record_serialise_source(record_serialise_source const&) = default;

            constexpr record_serialise_source(serialise_source<typename Fields::type>&&... elements)
                    noexcept((std::is_nothrow_constructible_v<
                        record_serialise_source_element<Fields::name, typename Fields::type>,
                        serialise_source<typename Fields::type>> && ...))
                    requires (sizeof...(Fields) > 0) :
                record_serialise_source_element<Fields::name, typename Fields::type>{
                    std::move(elements)}...
            {}

            record_serialise_source& operator=(record_serialise_source&&) = default;
            record_serialise_source& operator=(record_serialise_source const&) = default;

            // Gets a field by name.
            template<constant_string Name> requires (contains_field<type_list<Fields...>, Name>())
            [[nodiscard]]
            constexpr auto& get() noexcept {
                return record_serialise_source_get<Name>(*this);
            }

            // Gets a field by name.
            template<constant_string Name> requires (contains_field<type_list<Fields...>, Name>())
            [[nodiscard]]
            constexpr auto const& get() const noexcept {
                return record_serialise_source_get<Name>(*this);
            }
        };

    }


    template<record_type R>
    class serialise_source<R> : public detail::record_serialise_source<typename R::fields> {
    public:
        using detail::record_serialise_source<typename R::fields>::record_serialise_source;
    };


    template<record_type R>
    struct serialiser<R> {
        template<serialise_buffer Buffer>
        constexpr serialise_target<Buffer> operator()(serialise_source<R> const& source,
                serialise_target<Buffer> target) const {
            return _serialise<typename R::fields>(source, target);
        }

    private:
        template<detail::field_list Fields, serialise_buffer Buffer>
        [[nodiscard]]
        constexpr serialise_target<Buffer> _serialise(serialise_source<R> const& source,
                serialise_target<Buffer> target) const {
            if constexpr (Fields::size > 0) {
                using head_field = Fields::head;
                using head_type = head_field::type;
                target = target.push_fixed_subobject<head_type>([&source](auto field_target) {
                    return serialiser<head_type>{}(source.get<head_field::name>(), field_target);
                });
                return _serialise<typename Fields::tail>(source, target);
            }
            else {
                return target;
            }
        }
    };


    template<record_type R>
    class deserialiser<R> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr deserialiser(const_bytes_span fixed_data, const_bytes_span variable_data) :
            deserialiser_base{no_fixed_buffer_check, fixed_data, variable_data}
        {
            check_fixed_buffer_size<R>(_fixed_data);
        }

        // Gets a field by name.
        // If there is no field with the specified name, a compile error occurs.
        template<constant_string Name> requires has_field<R, Name>
        [[nodiscard]]
        constexpr auto_deserialise_t<field_type<R, Name>> get() const {
            constexpr auto offset = detail::named_field_offset<typename R::fields, Name>();
            using field_t = field_type<R, Name>;
            return _get<offset, field_t>();
        }

        // Gets a field by zero-based index.
        // If Index is out of bounds, a compile error occurs.
        template<std::size_t Index> requires (Index < R::fields::size)
        [[nodiscard]]
        constexpr auto get() const {
            constexpr auto offset = detail::indexed_field_offset<typename R::fields, Index>;
            using field_t = detail::type_list_element<typename R::fields, Index>::type::type;
            return _get<offset, field_t>();
        }

        // Conversion to deserialiser for any base record type.
        template<serialisable T> requires record_derived_from<R, T>
        constexpr operator deserialiser<T>() const noexcept {
            static_assert(fixed_data_size_v<T> <= fixed_data_size_v<R>);
            return deserialiser<T>{
                no_fixed_buffer_check,
                _fixed_data.first(fixed_data_size_v<T>),
                _variable_data
            };
        }

    private:
        template<std::size_t Offset, serialisable T> requires (Offset + fixed_data_size_v<T> <= fixed_data_size_v<R>)
        [[nodiscard]]
        constexpr auto_deserialise_t<T> _get() const {
            assert(Offset <= _fixed_data.size());
            deserialiser<T> const deser{
                no_fixed_buffer_check,
                _fixed_data.subspan(Offset, fixed_data_size_v<T>),
                _variable_data
            };
            return auto_deserialise(deser);
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
            ::serialpp::auto_deserialise_t<
                typename ::serialpp::detail::type_list_element<typename R::fields, I>::type::type>> {};

}

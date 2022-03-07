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
        Struct:
            Represented as the result of contiguously serialising each field of the struct.
    */


    // TODO: struct inheritance?


    // Specifies a field of a SerialisableStruct.
    template<ConstantString Name, Serialisable T>
    struct Field {
        using Type = T;

        static constexpr auto NAME = Name;
    };


    template<typename T>
    static inline constexpr bool IS_FIELD = requires {
        requires std::derived_from<T, Field<T::NAME, typename T::Type>>;
    };


    // Base for compound structures that can contain arbitrary serialisable fields.
    // Alias or inherit from this to implement your own struct.
    // struct MyStruct : SerialisableStruct<
    //     Field<"foo", std::int32_t>,
    //     Field<"bar", Optional<std::uint64_t>>,
    //     Field<"qux", List<std::int8_t>>
    //  > {};
    template<class... Fs> requires (IS_FIELD<Fs> && ...)
    struct SerialisableStruct : Fs... {
        using Fields = TypeList<Fs...>;
    };


    template<typename T>
    static inline constexpr bool IS_FIELDS_LIST = false;

    template<class... Ts>
    static inline constexpr bool IS_FIELDS_LIST<TypeList<Ts...>> = (IS_FIELD<Ts> && ...);


    template<typename T, class Fields> requires IS_FIELDS_LIST<Fields>
    static inline constexpr bool IS_DERIVED_FROM_SERIALISABLE_STRUCT = false;

    template<typename T, class... Fields> requires (IS_FIELD<Fields> && ...)
    static inline constexpr bool IS_DERIVED_FROM_SERIALISABLE_STRUCT<T, TypeList<Fields...>> =
        std::derived_from<T, SerialisableStruct<Fields...>> {};


    template<typename T>
    concept Struct =
        IS_FIELDS_LIST<typename T::Fields> && IS_DERIVED_FROM_SERIALISABLE_STRUCT<T, typename T::Fields>;


    namespace impl {

        template<class Fields, ConstantString Name> requires IS_FIELDS_LIST<Fields>
        [[nodiscard]]
        consteval bool contains_field() noexcept {
            if constexpr (Fields::Head::NAME == Name) {
                return true;
            }
            else {
                return contains_field<typename Fields::Tail, Name>();
            }
        }

    }


    // Checks if a list of fields has a field with the specified name.
    template<class Fields, ConstantString Name> requires IS_FIELDS_LIST<Fields>
    static inline constexpr bool CONTAINS_FIELD = impl::contains_field<Fields, Name>();


    // Checks if a SerialisableStruct has a field with the specified name.
    template<Struct S, ConstantString Name>
    static inline constexpr bool HAS_FIELD = impl::contains_field<typename S::Fields, Name>();


    namespace impl {

        template<ConstantString Name, typename T>
        consteval T field_type(Field<Name, T> const&);

    }


    // Gets the type of the SerialisableStruct field with the specified name.
    // If there is no such field, a compile error occurs.
    template<Struct S, ConstantString Name>
    using FieldType = decltype(impl::field_type<Name>(std::declval<S>()));


    template<class Fields> requires IS_FIELDS_LIST<Fields>
    static inline constexpr std::size_t FIELDS_FIXED_DATA_SIZE = 
        FIXED_DATA_SIZE<typename Fields::Head::Type> + FIELDS_FIXED_DATA_SIZE<typename Fields::Tail>;

    template<>
    static inline constexpr std::size_t FIELDS_FIXED_DATA_SIZE<TypeList<>> = 0;


    template<Struct T>
    struct FixedDataSize<T> : SizeTConstant<FIELDS_FIXED_DATA_SIZE<typename T::Fields>> {};


    namespace impl {

        template<class Fields, ConstantString Name> requires IS_FIELDS_LIST<Fields> && CONTAINS_FIELD<Fields, Name>
        [[nodiscard]]
        consteval std::size_t named_field_offset() noexcept {
            static_assert(!std::same_as<Fields, TypeList<>>, "No field with the specified name");
            if constexpr (Fields::Head::NAME == Name) {
                return 0;
            }
            else {
                return FIXED_DATA_SIZE<typename Fields::Head::Type> + named_field_offset<typename Fields::Tail, Name>();
            }
        }


        template<class Fields, std::size_t Index> requires IS_FIELDS_LIST<Fields> && (Index < Fields::SIZE)
        static inline constexpr std::size_t INDEXED_FIELD_OFFSET =
            FIXED_DATA_SIZE<typename Fields::Head::Type> + INDEXED_FIELD_OFFSET<typename Fields::Tail, Index - 1>;

        template<class Fields>
        static inline constexpr std::size_t INDEXED_FIELD_OFFSET<Fields, 0> = 0;

    }


    // Gets the offset of a SerialisableStruct field from the beginning of the fixed data section.
    // If there is no field with the specified name, a compile error occurs.
    template<Struct S, ConstantString Name> requires HAS_FIELD<S, Name>
    static inline constexpr auto NAMED_FIELD_OFFSET = impl::named_field_offset<typename S::Fields, Name>();

    // Gets the offset of a SerialisableStruct field from the beginning of the fixed data section.
    // If Index is out of bounds, a compile error occurs.
    template<Struct S, std::size_t Index> requires (Index < S::Fields::SIZE)
    static inline constexpr auto INDEXED_FIELD_OFFSET = impl::INDEXED_FIELD_OFFSET<typename S::Fields, Index>;


    template<ConstantString Name, typename T>
    struct StructSerialiseSourceElement {
        using Type = T;

        [[no_unique_address]]   // For Void
        T value;

        static constexpr ConstantString NAME = Name;
    };


    namespace impl {

        template<ConstantString Name, typename T>
        [[nodiscard]]
        constexpr T& get(StructSerialiseSourceElement<Name, T>& source) noexcept {
            return source.value;
        }

        template<ConstantString Name, typename T>
        [[nodiscard]]
        constexpr T const& get(StructSerialiseSourceElement<Name, T> const& source) noexcept {
            return source.value;
        }

    }


    template<class Fields> requires IS_FIELDS_LIST<Fields>
    class FieldsSerialiseSource;

    // Inspired by "Beyond struct: Meta-programming a struct Replacement in C++20" by John Bandela: https://youtu.be/FXfrojjIo80
    template<class... Fields> requires (IS_FIELD<Fields> && ...)
    class FieldsSerialiseSource<TypeList<Fields...>>
            : StructSerialiseSourceElement<Fields::NAME, SerialiseSource<typename Fields::Type>>... {
    public:
        constexpr FieldsSerialiseSource() = default;

        constexpr FieldsSerialiseSource(SerialiseSource<typename Fields::Type>&&... elements)
                requires (sizeof...(Fields) > 0) :
            StructSerialiseSourceElement<Fields::NAME, SerialiseSource<typename Fields::Type>>{std::move(elements)}...
        {}

        // Gets a field by name.
        template<ConstantString Name>
        [[nodiscard]]
        constexpr auto& get() noexcept requires CONTAINS_FIELD<TypeList<Fields...>, Name> {
            return impl::get<Name>(*this);
        }

        // Gets a field by name.
        template<ConstantString Name>
        [[nodiscard]]
        constexpr auto const& get() const noexcept requires CONTAINS_FIELD<TypeList<Fields...>, Name> {
            return impl::get<Name>(*this);
        }
    };


    template<Struct S>
    class SerialiseSource<S> : public FieldsSerialiseSource<typename S::Fields> {
    public:
        using FieldsSerialiseSource<typename S::Fields>::FieldsSerialiseSource;
    };


    template<Struct S>
    struct Serialiser<S> {
        SerialiseTarget operator()(SerialiseSource<S> const& source, SerialiseTarget target) const {
            return _serialise<typename S::Fields>(source, target);
        }

    private:
        template<class Fields> requires IS_FIELDS_LIST<Fields>
        [[nodiscard]]
        SerialiseTarget _serialise(SerialiseSource<S> const& source, SerialiseTarget target) const {
            if constexpr (Fields::SIZE > 0) {
                using Head = Fields::Head;
                using HeadType = typename Head::Type;
                target = target.push_fixed_field<HeadType>([&source](SerialiseTarget field_target) {
                    return Serialiser<HeadType>{}(source.get<Head::NAME>(), field_target);
                });
                return _serialise<typename Fields::Tail>(source, target);
            }
            else {
                return target;
            }
        }
    };


    template<Struct S>
    class Deserialiser<S> : public DeserialiserBase<S> {
    public:
        using DeserialiserBase<S>::DeserialiserBase;

        // Gets a field by name.
        // If there is no field with the specified name, a compile error occurs.
        template<ConstantString Name> requires HAS_FIELD<S, Name>
        [[nodiscard]]
        auto get() const {
            constexpr auto offset = NAMED_FIELD_OFFSET<S, Name>;
            using FieldT = FieldType<S, Name>;
            return _get<offset, FieldT>();
        }

        // Gets a field by zero-based index.
        // If Index is out of bounds, a compile error occurs.
        template<std::size_t Index> requires (Index < S::Fields::SIZE)
        [[nodiscard]]
        auto get() const {
            constexpr auto offset = INDEXED_FIELD_OFFSET<S, Index>;
            using FieldT = TypeListElement<typename S::Fields, Index>::Type;
            return _get<offset, FieldT>();
        }

    private:
        template<std::size_t Offset, Serialisable T> requires (Offset + FIXED_DATA_SIZE<T> <= FIXED_DATA_SIZE<S>)
        [[nodiscard]]
        auto _get() const {
            assert(Offset <= this->_fixed_data.size());
            auto deserialiser = Deserialiser<T>{
                this->_fixed_data.subspan(Offset, FIXED_DATA_SIZE<T>),
                this->_variable_data
            };
            return auto_deserialise(deserialiser);
        }
    };

}


namespace std {

    template<serialpp::Struct S>
    struct tuple_size<serialpp::Deserialiser<S>> : integral_constant<size_t, S::Fields::SIZE> {};


    template<size_t I, serialpp::Struct S>
    struct tuple_element<I, serialpp::Deserialiser<S>>
        : type_identity<serialpp::AutoDeserialiseResult<typename serialpp::TypeListElement<typename S::Fields, I>::Type>> {};

}

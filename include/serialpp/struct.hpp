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

        static inline constexpr auto NAME = Name;
    };


    template<typename T>
    struct IsField : std::bool_constant<
        requires { requires std::derived_from<T, Field<T::NAME, typename T::Type>>; }
    > {};


    // Base for compound structures that can contain arbitrary serialisable fields.
    // Alias or inherit from this to implement your own struct.
    // struct MyStruct : SerialisableStruct<
    //     Field<"foo", std::int32_t>,
    //     Field<"bar", Optional<std::uint64_t>>,
    //     Field<"qux", List<std::int8_t>>
    //  > {};
    template<class... Fs> requires (IsField<Fs>::value && ...)
    struct SerialisableStruct : Fs... {
        using Fields = TypeList<Fs...>;
    };


    template<typename T>
    struct IsFieldsList : std::false_type {};

    template<class... Ts> requires (IsField<Ts>::value && ...)
    struct IsFieldsList<TypeList<Ts...>> : std::true_type {};


    namespace impl {

        template<typename T, class Fields> requires IsFieldsList<Fields>::value
        struct IsDerivedFromSerialisableStruct : std::false_type {};

        template<typename T, class... Fields> requires (IsField<Fields>::value && ...)
        struct IsDerivedFromSerialisableStruct<T, TypeList<Fields...>>
            : std::is_base_of<SerialisableStruct<Fields...>, T> {};

    }


    template<typename T>
    concept Struct =
        IsFieldsList<typename T::Fields>::value && impl::IsDerivedFromSerialisableStruct<T, typename T::Fields>::value;


    namespace impl {

        template<ConstantString Name, typename T>
        consteval T field_type(Field<Name, T> const&);

    }


    // Checks if a SerialisableStruct has a field with a specified name.
    template<Struct S, ConstantString Name>
    struct HasField : std::bool_constant<requires { impl::field_type<Name>(std::declval<S>()); }> {};


    // Gets the type of the SerialisableStruct field with the specified name.
    // If there is no such field, a compile error occurs.
    template<Struct S, ConstantString Name>
    using FieldType = decltype(impl::field_type<Name>(std::declval<S>()));


    template<class Fields> requires IsFieldsList<Fields>::value
    struct FieldsFixedDataSize
        : SizeTConstant<FIXED_DATA_SIZE<typename Fields::Head::Type> + FieldsFixedDataSize<typename Fields::Tail>{}> {};

    template<>
    struct FieldsFixedDataSize<TypeList<>> : SizeTConstant<0> {};


    template<Struct T>
    struct FixedDataSize<T> : FieldsFixedDataSize<typename T::Fields> {};


    namespace impl {

        template<class Fields, ConstantString N> requires IsFieldsList<Fields>::value
        [[nodiscard]]
        consteval std::size_t named_field_offset() noexcept {
            static_assert(!std::same_as<Fields, TypeList<>>, "No field with the specified name");
            if constexpr (Fields::Head::NAME == N) {
                return 0;
            }
            else {
                return FIXED_DATA_SIZE<typename Fields::Head::Type> + named_field_offset<typename Fields::Tail, N>();
            }
        }


        template<class Fields, std::size_t I> requires IsFieldsList<Fields>::value && (I < Fields::SIZE)
        struct IndexedFieldOffset
            : SizeTConstant<
                FIXED_DATA_SIZE<typename Fields::Head::Type>
                + IndexedFieldOffset<typename Fields::Tail, I - 1>::value> {};

        template<class Fields>
        struct IndexedFieldOffset<Fields, 0> : SizeTConstant<0> {};

    }


    // Gets the offset of a field from the beginning of the fixed data section.
    // If there is no field with the specified name, a compile error occurs.
    template<Struct S, ConstantString N> requires HasField<S, N>::value
    static inline constexpr auto NAMED_FIELD_OFFSET = impl::named_field_offset<typename S::Fields, N>();

    // Gets the offset of a field from the beginning of the fixed data section.
    // If I is out of bounds, a compile error occurs.
    template<Struct S, std::size_t I> requires (I < S::Fields::SIZE)
    static inline constexpr auto INDEXED_FIELD_OFFSET = impl::IndexedFieldOffset<typename S::Fields, I>::value;


    template<class... Fs> requires (IsField<Fs>::value && ...)
    using FieldsSerialiseSourceTuple = NamedTuple<NamedTupleElement<Fs::NAME, SerialiseSource<typename Fs::Type>>...>;


    template<class Fields> requires IsFieldsList<Fields>::value
    struct FieldsSerialiseSource;

    template<class... Fs> requires (IsField<Fs>::value && ...)
    struct FieldsSerialiseSource<TypeList<Fs...>> : FieldsSerialiseSourceTuple<Fs...> {
        using FieldsSerialiseSourceTuple<Fs...>::FieldsSerialiseSourceTuple;
    };


    template<Struct S>
    class SerialiseSource<S> : public FieldsSerialiseSource<typename S::Fields> {
    public:
        using FieldsSerialiseSource<typename S::Fields>::FieldsSerialiseSource;
    };


    template<class Fields> requires IsFieldsList<Fields>::value
    struct FieldsSerialiser {
        SerialiseTarget operator()(auto const& source, SerialiseTarget target) const {
            using Head = Fields::Head;
            using HeadType = typename Head::Type;
            target = target.push_fixed_field<HeadType>([&source](SerialiseTarget field_target) {
                return Serialiser<HeadType>{}(source.get<Head::NAME>(), field_target);
            });
            return FieldsSerialiser<typename Fields::Tail>{}(source, target);
        }
    };

    template<>
    struct FieldsSerialiser<TypeList<>> {
        SerialiseTarget operator()(auto const& source, SerialiseTarget target) const {
            return target;
        }
    };


    template<Struct S>
    struct Serialiser<S> : FieldsSerialiser<typename S::Fields> {};


    template<Struct S>
    class Deserialiser<S> : public DeserialiserBase<S> {
    public:
        using DeserialiserBase<S>::DeserialiserBase;

        // Gets a field by name.
        template<ConstantString Name> requires HasField<S, Name>::value
        [[nodiscard]]
        auto get() const {
            constexpr auto offset = NAMED_FIELD_OFFSET<S, Name>;
            using FieldT = FieldType<S, Name>;
            return _get<offset, FieldT>();
        }

        // Gets a field by zero-based index.
        template<std::size_t Index> requires (Index < S::Fields::SIZE)
        [[nodiscard]]
        auto get() const {
            constexpr auto offset = INDEXED_FIELD_OFFSET<S, Index>;
            using FieldT = TypeListElement<typename S::Fields, Index>::Type;
            return _get<offset, FieldT>();
        }

    private:
        template<std::size_t Offset, Serialisable T>
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


    template<std::size_t I, serialpp::Struct S>
    struct tuple_element<I, serialpp::Deserialiser<S>>
        : type_identity<serialpp::AutoDeserialiseResult<typename serialpp::TypeListElement<typename S::Fields, I>::Type>> {};

}

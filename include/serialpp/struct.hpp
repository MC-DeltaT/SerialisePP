#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        Struct:
            Represented as the result of contiguously serialising each field of the struct.
    */


    // Specified a field of a serialisable struct.
    template<ConstantString Name, Serialisable Type>
    using Field = NamedTupleElement<Name, Type>;


    template<typename T>
    using IsField = IsNamedTupleElement<T>;


    // Base for compound structures that can contain arbitrary serialisable fields.
    // Alias or inherit from this to implement your own struct.
    // struct MyStruct : SerialisableStruct<
    //     Field<"foo", std::int32_t>,
    //     Field<"bar", Optional<std::uint64_t>>,
    //     Field<"qux", List<std::int8_t>>
    //  > {};
    template<class... Fs> requires (IsField<Fs>::value && ...)
    struct SerialisableStruct : NamedTuple<Fs...> {
        using Fields = NamedTuple<Fs...>::Elements;
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


    template<class T>
    concept Struct =
        IsFieldsList<typename T::Fields>::value && impl::IsDerivedFromSerialisableStruct<T, typename T::Fields>::value;


    template<class Fields> requires IsFieldsList<Fields>::value
    struct FieldsFixedDataSize
        : SizeTConstant<FIXED_DATA_SIZE<typename Fields::Head::Type> + FieldsFixedDataSize<typename Fields::Tail>{}> {};

    template<>
    struct FieldsFixedDataSize<TypeList<>> : SizeTConstant<0> {};


    template<Struct T>
    struct FixedDataSize<T> : FieldsFixedDataSize<typename T::Fields> {};


    template<class Fields, ConstantString N> requires IsFieldsList<Fields>::value
    consteval std::size_t field_offset() noexcept {
        static_assert(!std::same_as<Fields, TypeList<>>, "No field with the specified name");
        if constexpr (Fields::Head::NAME == N) {
            return 0;
        }
        else {
            return FIXED_DATA_SIZE<typename Fields::Head::Type> + field_offset<typename Fields::Tail, N>();
        }
    }

    // Gets the offset of a field from the beginning of the fixed data section.
    // If there is no field with the specified name, a compile error occurs.
    template<Struct S, ConstantString N> requires NamedTupleHasElement<S, N>::value
    static inline constexpr auto FIELD_OFFSET = field_offset<typename S::Fields, N>();


    template<class... Fs> requires (IsField<Fs>::value && ...)
    using FieldsSerialiseSourceTuple = NamedTuple<NamedTupleElement<Fs::NAME, SerialiseSource<typename Fs::Type>>...>;


    template<class Fields> requires IsFieldsList<Fields>::value
    struct FieldsSerialiseSource;

    template<class... Fs> requires (IsField<Fs>::value && ...)
    struct FieldsSerialiseSource<TypeList<Fs...>> : FieldsSerialiseSourceTuple<Fs...> {
        using FieldsSerialiseSourceTuple<Fs...>::FieldsSerialiseSourceTuple;
    };


    template<Struct S>
    struct SerialiseSource<S> : FieldsSerialiseSource<typename S::Fields> {
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
    struct Deserialiser<S> : DeserialiserBase {
        using DeserialiserBase::DeserialiserBase;

        // Gets a field by name.
        template<ConstantString Name>
        auto get() const {
            constexpr auto offset = FIELD_OFFSET<S, Name>;
            assert(offset <= fixed_data.size());
            using FieldType = NamedTupleElementType<S, Name>;
            auto deserialiser = Deserialiser<FieldType>{
                fixed_data.subspan(offset),
                variable_data
            };
            return auto_deserialise_scalar(deserialiser);
        }
    };

}

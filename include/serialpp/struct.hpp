#pragma once

#include <cassert>
#include <concepts>
#include <type_traits>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        Struct:
            Represented as the result of contiguously serialising each field of the struct.
    */


    template<Serialisable T>
    struct FieldBase {
        using Type = T;
    };


    template<typename T>
    concept Field = requires {
        typename T::Type;
        requires std::derived_from<T, FieldBase<typename T::Type>>;
        requires Serialisable<typename T::Type>;
    };


    namespace impl {
        template<typename T>
        struct FieldsList : std::false_type {};

        template<Field... Fs>
        struct FieldsList<TypeList<Fs...>> : std::true_type {};
    }

    template<typename T>
    concept FieldsList = impl::FieldsList<T>::value;


    template<FieldsList Fs>
    struct FieldsFixedDataSize
        : SizeTConstant<FIXED_DATA_SIZE<typename Fs::Head::Type> + FieldsFixedDataSize<typename Fs::Tail>{}> {};

    template<>
    struct FieldsFixedDataSize<TypeList<>> : SizeTConstant<0> {};



    template<class T>
    concept Struct = std::is_class_v<T> && FieldsList<typename T::Fields>;


    template<Struct T>
    struct FixedDataSize<T> : FieldsFixedDataSize<typename T::Fields> {};


    template<FieldsList Fs, class F>
    struct FieldOffset
        : SizeTConstant<FIXED_DATA_SIZE<typename Fs::Head::Type> + FieldOffset<typename Fs::Tail, F>{}> {};

    template<FieldsList Fs>
    struct FieldOffset<Fs, typename Fs::Head> : SizeTConstant<0> {};

    template<class Fields, class F>
    static inline constexpr auto FIELD_OFFSET = FieldOffset<Fields, F>::value;


    template<Field... Fs>
    using FieldsSerialiseSourceTuple = TaggedTuple<TaggedType<SerialiseSource<typename Fs::Type>, Fs>...>;


    template<FieldsList Fs>
    struct FieldsSerialiseSource;

    template<Field... Fs>
    struct FieldsSerialiseSource<TypeList<Fs...>> : FieldsSerialiseSourceTuple<Fs...> {
        using FieldsSerialiseSourceTuple<Fs...>::FieldsSerialiseSourceTuple;
    };


    template<Struct S>
    struct SerialiseSource<S> : FieldsSerialiseSource<typename S::Fields> {
        using FieldsSerialiseSource<typename S::Fields>::FieldsSerialiseSource;
    };


    template<FieldsList Fs>
    struct StructSerialiser {
        SerialiseTarget operator()(auto const& source, SerialiseTarget target) const {
            using Head = Fs::Head;
            using HeadType = typename Head::Type;
            target = target.push_fixed_field<HeadType>([&source](SerialiseTarget field_target) {
                return Serialiser<HeadType>{}(source.get<Head>(), field_target);
            });
            return StructSerialiser<typename Fs::Tail>{}(source, target);
        }
    };

    template<>
    struct StructSerialiser<TypeList<>> {
        SerialiseTarget operator()(auto const& source, SerialiseTarget target) const {
            return target;
        }
    };


    template<Struct S>
    struct Serialiser<S> : StructSerialiser<typename S::Fields> {};


    template<Struct S>
    struct Deserialiser<S> : DeserialiserBase {
        using DeserialiserBase::DeserialiserBase;

        // Gets the field with type F.
        template<Field F>
        auto get() const {
            constexpr auto offset = FIELD_OFFSET<typename S::Fields, F>;
            assert(offset <= fixed_data.size());
            auto deserialiser = Deserialiser<typename F::Type>{
                fixed_data.subspan(offset),
                variable_data
            };
            return auto_deserialise_scalar(deserialiser);
        }

        // Gets the field with type F (convenience function for use with field object).
        template<Field F>
        auto operator[](F const&) const {
            return get<F>();
        }
    };

}


// Defines a field for a struct.
// The field gets a field type called <name>_t, and a field object called <name>.
#define FIELD(name, ...) struct name##_t : ::serialpp::FieldBase<__VA_ARGS__> {} static inline constexpr name{}

// Defines the fields contained within a struct.
#define FIELDS(...) using Fields = decltype(::serialpp::make_type_list(__VA_ARGS__))


/*
    Example:

        struct MyStruct {
            FIELD(foo, std::int32_t);
            FIELD(bar, Optional<std::uint64_t>);
            FIELD(qux, List<std::int8_t>);

            FIELDS(foo, bar, qux);
        };
*/

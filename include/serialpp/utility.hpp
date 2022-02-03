#pragma once

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>


namespace serialpp {

    template<std::size_t V>
    using SizeTConstant = std::integral_constant<std::size_t, V>;


    template<typename T, typename R, typename... Args>
    concept Callable = std::invocable<T, Args...> && std::same_as<R, std::invoke_result_t<T, Args...>>;


    template<typename... Ts>
    struct TypeList;

    template<>
    struct TypeList<> {};

    template<typename T, typename... Ts>
    struct TypeList<T, Ts...> {
        using Head = T;
        using Tail = TypeList<Ts...>;
    };


    // Used to create a TypeList with types from a list of values.
    template<typename... Ts>
    TypeList<std::remove_cvref_t<Ts>...> make_type_list(Ts&&...);


    // Obtains the index of the first occurrence of a type T in a TypeList L (as an std::integral_constant).
    // If L does not contain T then a compile error will occur.
    template<class L, typename T>
    struct TypeListIndex
        : SizeTConstant<1 + TypeListIndex<typename L::Tail, T>{}> {};

    template<class L>
    struct TypeListIndex<L, typename L::Head> : SizeTConstant<0> {};

    // Obtains the index of the first occurrence of a type T in a TypeList L.
    template<class L, typename T>
    static inline constexpr auto TYPE_LIST_INDEX = TypeListIndex<L, T>::value;


    template<typename T, typename TagT>
    struct TaggedType {
        using Type = T;
        using Tag = TagT;
    };


    // Like std::tuple, but each element type is associated with a "tag" type which may be used as an indexer.
    template<class... Tags>
    struct TaggedTuple;

    template<typename... Types, typename... Tags>
    struct TaggedTuple<TaggedType<Types, Tags>...> : std::tuple<Types...> {
        using std::tuple<Types...>::tuple;

        // Gets an element by its tag type.
        template<class Tag>
        decltype(auto) get() const {
            using TagList = TypeList<Tags...>;
            constexpr auto index = TYPE_LIST_INDEX<TagList, Tag>;
            return std::get<index>(*this);
        }
    };

}

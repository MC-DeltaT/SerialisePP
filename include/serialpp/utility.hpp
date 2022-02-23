#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>


namespace serialpp {

    template<std::size_t Value>
    using SizeTConstant = std::integral_constant<std::size_t, Value>;


    template<typename T, typename Return, typename... Args>
    concept Callable = std::invocable<T, Args...> && std::same_as<Return, std::invoke_result_t<T, Args...>>;


    template<typename... Ts>
    struct TypeList;

    template<>
    struct TypeList<> {};

    template<typename T, typename... Ts>
    struct TypeList<T, Ts...> {
        using Head = T;
        using Tail = TypeList<Ts...>;
    };


    // Fixed-length string for use as template arguments at compile time.
    template<std::size_t N>
    struct ConstantString {
        char data[N] = {};

        constexpr ConstantString(char const (&str)[N + 1]) {
            std::copy_n(str, N, data);
        }

        [[nodiscard]]
        constexpr std::string_view string_view() const {
            return {data, N};
        }
    };

    // Deduction guide to trim off null terminator from a string literal.
    template<std::size_t N>
    ConstantString(char const (&)[N]) -> ConstantString<N - 1>;


    template<std::size_t N1, std::size_t N2>
    [[nodiscard]]
    constexpr bool operator==(ConstantString<N1> const& lhs, ConstantString<N2> const& rhs) {
        return lhs.string_view() == rhs.string_view();
    }


    // Element of a NamedTuple.
    template<ConstantString Name, typename T>
    struct NamedTupleElement {
        using Type = T;

        T value;

        static inline constexpr ConstantString NAME = Name;
    };


    template<typename T>
    struct IsNamedTupleElement : std::bool_constant<
        requires { requires std::derived_from<T, NamedTupleElement<T::NAME, typename T::Type>>; }
    > {};


    namespace impl {

        template<ConstantString Name, typename T>
        [[nodiscard]]
        constexpr T& get(NamedTupleElement<Name, T>& tuple) noexcept {
            return tuple.value;
        }

        template<ConstantString Name, typename T>
        [[nodiscard]]
        constexpr T const& get(NamedTupleElement<Name, T> const& tuple) noexcept {
            return tuple.value;
        }

    }


    // Checks if a NamedTuple has an element with a specified name.
    template<class Tuple, ConstantString Name>
    struct HasElement : std::bool_constant<requires { impl::get<Name>(std::declval<Tuple>()); }> {};


    namespace impl {

        // Silly class needed for NamedTuple on MSVC, where a base class name (Es) will shadow a template parameter
        // (pretty sure it's a bug).
        template<typename T>
        struct DeferredBase : T {};

    }


    // A tuple where each element is associated with a name given by a ConstantString.
    // Inspired by "Beyond struct: Meta-programming a struct Replacement in C++20" by John Bandela: https://youtu.be/FXfrojjIo80
    template<class... Es> requires (IsNamedTupleElement<Es>::value && ...)
    struct NamedTuple : impl::DeferredBase<Es>...  {
        using Elements = TypeList<Es...>;

        constexpr NamedTuple() = default;

        constexpr NamedTuple(typename Es::Type... elements) :
            impl::DeferredBase<Es>{std::move(elements)}...
        {}

        // Gets an element by name.
        template<ConstantString Name>
        [[nodiscard]]
        constexpr auto&& get() noexcept requires HasElement<NamedTuple, Name>::value {
            return impl::get<Name>(*this);
        }

        // Gets an element by name.
        template<ConstantString Name>
        [[nodiscard]]
        constexpr auto&& get() const noexcept requires HasElement<NamedTuple, Name>::value {
            return impl::get<Name>(*this);
        }
    };


    // Wrapper for a small value of any type (to avoid allocating on the heap), with support for visiting the value.
    template<std::size_t MaxSize, std::size_t Align, typename Visitor>
    class SmallAny {
    public:
        using VisitorType = Visitor;

        static constexpr std::size_t MAX_SIZE = MaxSize;
        static constexpr std::size_t ALIGN = Align;

        template<typename T, typename... Args>
            requires (sizeof(T) <= MaxSize) && (alignof(T) <= Align) && std::constructible_from<T, Args...>
        SmallAny(std::in_place_type_t<T>, Args&&... args) :
            _visitor_func{&_generic_visitor_func<T>}
        {
            new(_data) T(std::forward<Args>(args)...);
        }

        ~SmallAny() {
            _destruct();
        }

        SmallAny(SmallAny&& other) noexcept :
            _visitor_func{other._visitor_func}
        {
            // No need to move data if other was empty.
            if (_visitor_func) {
                std::memcpy(_data, other._data, sizeof(_data));
                other._destruct();
                other._visitor_func = nullptr;
            }
        }

        SmallAny(SmallAny const&) = delete;

        SmallAny& operator=(SmallAny&&) = delete;
        SmallAny& operator=(SmallAny const&) = delete;
        
        void visit(Visitor& visitor) {
            assert(_visitor_func);
            _visitor_func(_data, false, true, &visitor);
        }

        void visit(Visitor& visitor) const {
            assert(_visitor_func);
            _visitor_func(_data, true, true, &visitor);
        }

    private:
        alignas(Align) char _data[MaxSize];
        void (*_visitor_func)(void const*, bool, bool, Visitor*);

        void _destruct() noexcept {
            if (_visitor_func) {
                _visitor_func(_data, false, false, nullptr);
            }
        }

        template<typename T>
        static void _generic_visitor_func(void const* data, bool is_const, bool operation, Visitor* visitor) {
            if (is_const) {
                auto const p = static_cast<T const*>(data);
                if (operation) {
                    (*visitor)(*p);
                }
                else {
                    p->~T();
                }
            }
            else {
                auto const p = static_cast<T*>(const_cast<void*>(data));
                if (operation) {
                    (*visitor)(*p);
                }
                else {
                    p->~T();
                }
            }
        }
    };

}

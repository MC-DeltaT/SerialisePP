#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <functional>
#include <new>
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
    struct TypeList<> {
        static constexpr std::size_t SIZE = 0;
    };

    template<typename T, typename... Ts>
    struct TypeList<T, Ts...> {
        using Head = T;
        using Tail = TypeList<Ts...>;

        static constexpr std::size_t SIZE = 1 + sizeof...(Ts);
    };


    namespace impl {

        template<class TList, std::size_t Index> requires (Index < TList::SIZE)
        struct TypeListElement : TypeListElement<typename TList::Tail, Index - 1> {};

        template<class TList>
        struct TypeListElement<TList, 0> : std::type_identity<typename TList::Head> {};

    }

    // Gets the type at the specified index of a TypeList.
    // If Index is out of bounds, a compile error occurs.
    template<class TList, std::size_t Index> requires (Index < TList::SIZE)
    using TypeListElement = typename impl::TypeListElement<TList, Index>::type;


    // Fixed-length string for use as template arguments at compile time.
    // From "Beyond struct: Meta-programming a struct Replacement in C++20" by John Bandela: https://youtu.be/FXfrojjIo80
    template<std::size_t Size>
    struct ConstantString {
        char data[Size] = {};

        constexpr ConstantString(char const (&str)[Size + 1]) {
            std::copy_n(str, Size, data);
        }

        [[nodiscard]]
        constexpr std::string_view string_view() const {
            return {data, Size};
        }
    };

    // Deduction guide to trim off null terminator from a string literal.
    template<std::size_t Size>
    ConstantString(char const (&)[Size]) -> ConstantString<Size - 1>;


    template<std::size_t Size1, std::size_t Size2>
    [[nodiscard]]
    constexpr bool operator==(ConstantString<Size1> const& lhs, ConstantString<Size2> const& rhs) {
        return lhs.string_view() == rhs.string_view();
    }


    // Wrapper for a small value of any type (to avoid allocating on the heap), with support for visiting the value.
    template<std::size_t MaxSize, std::size_t Align, typename Visitor> requires (MaxSize >= 1) && (Align >= 1)
    class SmallAny {
    public:
        using VisitorType = Visitor;

        static constexpr std::size_t MAX_SIZE = MaxSize;
        static constexpr std::size_t ALIGN = Align;

        // Checks if this type can contain a particular type (i.e. satisfies size and alignment requirements).
        template<typename T>
        static constexpr bool can_contain() noexcept {
            return sizeof(T) <= MaxSize && alignof(T) <= Align;
        }

        template<typename T, typename... Args> requires (can_contain<T>()) && std::constructible_from<T, Args...>
        explicit SmallAny(std::in_place_type_t<T>, Args&&... args) :
            _visitor_func{&_generic_visitor_func<T>}
        {
            new(_data) T(std::forward<Args>(args)...);
        }

        // Construct using a provided function that does custom construction of the contained object.
        template<std::invocable<void*> F, typename T = std::remove_pointer_t<std::invoke_result_t<F, void*>>>
            requires (can_contain<T>())
        explicit SmallAny(F&& constructor) :
            _visitor_func{&_generic_visitor_func<T>}
        {
            std::invoke(std::forward<F>(constructor), static_cast<void*>(_data));
        }

        ~SmallAny() {
            _destruct();
        }

        SmallAny(SmallAny&& other) noexcept :
            _visitor_func{std::exchange(other._visitor_func, nullptr)}
        {
            // No need to move data if other was empty.
            if (_visitor_func) {
                std::memcpy(_data, other._data, sizeof(_data));
            }
        }

        SmallAny(SmallAny const&) = delete;

        SmallAny& operator=(SmallAny&&) = delete;
        SmallAny& operator=(SmallAny const&) = delete;

        void visit(Visitor& visitor) const {
            assert(_visitor_func);
            _visitor_func(_data, true, &visitor);
        }

    private:
        alignas(Align) std::byte _data[MaxSize];
        void (*_visitor_func)(void const*, bool, Visitor*);

        void _destruct() noexcept {
            if (_visitor_func) {
                _visitor_func(_data, false, nullptr);
            }
        }

        template<typename T>
        static void _generic_visitor_func(void const* data, bool operation, Visitor* visitor) {
            auto const p = static_cast<T const*>(data);
            if (operation) {
                (*visitor)(*p);
            }
            else {
                p->~T();
            }
        }
    };

}

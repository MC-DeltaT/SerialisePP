#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <functional>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>


namespace serialpp {

    // Fixed-length string for use as template arguments at compile time.
    // The trailing null character isn't stored. I.e. Size is the true size of the string in characters.
    // From "Beyond struct: Meta-programming a struct Replacement in C++20" by John Bandela: https://youtu.be/FXfrojjIo80
    template<std::size_t Size>
    struct constant_string {
        char data[Size] = {};

        constexpr constant_string(char const (&str)[Size + 1]) {
            std::copy_n(str, Size, data);
        }

        [[nodiscard]]
        constexpr std::string_view string_view() const noexcept {
            return {data, Size};
        }

        static constexpr std::size_t size = Size;
    };

    // Deduction guide to trim off null terminator from a string literal.
    template<std::size_t Size>
    constant_string(char const (&)[Size]) -> constant_string<Size - 1>;


    template<std::size_t Size1, std::size_t Size2>
    [[nodiscard]]
    constexpr bool operator==(constant_string<Size1> const& lhs, constant_string<Size2> const& rhs) noexcept {
        return lhs.string_view() == rhs.string_view();
    }


    template<typename... Ts>
    struct type_list;

    template<>
    struct type_list<> {
        static constexpr std::size_t size = 0;
    };

    template<typename T, typename... Ts>
    struct type_list<T, Ts...> {
        using head = T;
        using tail = type_list<Ts...>;

        static constexpr std::size_t size = 1 + sizeof...(Ts);
    };


    namespace detail {

        // Gets the type at the specified index of a type_list.
        // If Index is out of bounds, a compile error occurs.
        template<class TList, std::size_t Index> requires (Index < TList::size)
        struct type_list_element : type_list_element<typename TList::tail, Index - 1> {};

        template<class TList>
        struct type_list_element<TList, 0> : std::type_identity<typename TList::head> {};


        // Checks if a type_list contains a specified type.
        template<class TList, typename T>
        [[nodiscard]]
        consteval bool type_list_contains() noexcept {
            if constexpr (TList::size == 0) {
                return false;
            }
            else if constexpr (std::same_as<typename TList::head, T>) {
                return true;
            }
            else {
                return type_list_contains<typename TList::tail, T>();
            }
        }


        // Adds a type to the beginning of a type_list.
        template<typename T, class TList>
        struct type_list_prepend;

        template<typename T, typename... Ts>    // Can't just inherit from type_list, as template matching won't work.
        struct type_list_prepend<T, type_list<Ts...>> : std::type_identity<type_list<T, Ts...>> {};


        template<class TList1, class TList2>
        struct type_list_concat;

        template<typename... Ts1, typename... Ts2>
        struct type_list_concat<type_list<Ts1...>, type_list<Ts2...>>
            : std::type_identity<type_list<Ts1..., Ts2...>> {};


        template<std::size_t Value>
        using size_t_constant = std::integral_constant<std::size_t, Value>;


        template<typename T, typename Return, typename... Args>
        concept function = std::invocable<T, Args...> && std::same_as<Return, std::invoke_result_t<T, Args...>>;


        static inline constexpr bool is_little_endian = std::endian::native == std::endian::little;
        static inline constexpr bool is_big_endian = std::endian::native == std::endian::big;
        static inline constexpr bool is_mixed_endian = !is_little_endian && !is_big_endian;


        // Wrapper for a small value of any type (to avoid allocating on the heap), with support for visiting the value.
        template<std::size_t MaxSize, std::size_t Align, typename Visitor> requires (MaxSize >= 1) && (Align >= 1)
        class small_any {
        public:
            using visitor_type = Visitor;

            static constexpr std::size_t max_size = MaxSize;
            static constexpr std::size_t alignment = Align;

            // Checks if this type's storage is compatible with a given type.
            template<typename T>
            static consteval bool storage_compatible_with() noexcept {
                return sizeof(T) <= MaxSize && alignof(T) <= Align;
            }

            // Checks if this type can contain a given type.
            template<typename T>
            static consteval bool can_contain() noexcept {
                return std::is_object_v<T> && std::move_constructible<T> && std::invocable<Visitor, T const&>
                    && storage_compatible_with<T>();
            }

            // Default construct to contain nothing.
            constexpr small_any() noexcept :
                _data{},
                _visitor_func{nullptr}
            {}

            constexpr ~small_any() {
                _destruct();
            }

            small_any(small_any&& other) :
                _visitor_func{other._visitor_func}
            {
                if (other._visitor_func) {
                    other._visitor_func(other._data, _visit_operation::move_construct, _data);

                    // Do this last so we get strong exception guarantee.
                    other._visitor_func = nullptr;
                }
            }

            small_any(small_any const&) = delete;

            small_any& operator=(small_any&&) = delete;
            small_any& operator=(small_any const&) = delete;

            // Construct from a type and its constructor arguments.
            template<typename T, typename... Args> requires (can_contain<T>()) && std::constructible_from<T, Args...>
            void emplace(Args&&... args) noexcept(std::is_nothrow_constructible<T, Args...>) {
                _destruct();
                new(_data) T(std::forward<Args>(args)...);
                _visitor_func = &_generic_visitor_func<T>;
            }

            // Construct using a provided function that does custom construction of the contained object.
            template<std::invocable<void*> F, typename T = std::remove_pointer_t<std::invoke_result_t<F, void*>>>
                requires (can_contain<T>())
            void emplace(F&& constructor) noexcept(std::is_nothrow_invocable<F, void*>) {
                _destruct();
                std::invoke(std::forward<F>(constructor), static_cast<void*>(_data));
                _visitor_func = &_generic_visitor_func<T>;
            }

            bool empty() const noexcept {
                return _visitor_func == nullptr;
            }

            void visit(Visitor& visitor) const {
                if (_visitor_func) {
                    _visitor_func(_data, _visit_operation::visit, &visitor);
                }
            }

        private:
            enum class _visit_operation : unsigned char {
                destruct, move_construct, visit
            };

            alignas(Align) std::byte _data[MaxSize];
            void (*_visitor_func)(void const*, _visit_operation, void*);

            constexpr void _destruct() noexcept {
                if (_visitor_func) {
                    _visitor_func(_data, _visit_operation::destruct, nullptr);
                    _visitor_func = nullptr;
                }
            }

            template<typename T> requires std::invocable<Visitor, T const&>
            static void _generic_visitor_func(void const* data, _visit_operation operation, void* arg) {
                switch (operation) {
                case _visit_operation::destruct:
                    static_cast<T const*>(data)->~T();
                    break;
                case _visit_operation::move_construct:
                    // Will be inside the move constructor, thus data will always be nonconst.
                    new(arg) T{std::move(*static_cast<T*>(const_cast<void*>(data)))};
                    break;
                case _visit_operation::visit:
                    std::invoke(*static_cast<Visitor*>(arg), *static_cast<T const*>(data));
                    break;
                default:
                    break;
                }
            }
        };

    }

}

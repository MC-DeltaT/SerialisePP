#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>


namespace serialpp::test {

    struct lifecycle_data {
        std::size_t constructs = 0;
        std::size_t destructs = 0;
        std::size_t move_constructs = 0;
        std::size_t copy_constructs = 0;
        std::size_t move_assigns = 0;
        std::size_t copy_assigns = 0;

        [[nodiscard]]
        friend constexpr bool operator==(lifecycle_data const&, lifecycle_data const&) noexcept = default;
    };


    template<std::regular T>
    class lifecycle_observer {
    public:
        T value;

        template<typename... Args> requires std::constructible_from<T, Args...>
        constexpr lifecycle_observer(lifecycle_data& lifecycle, Args&&... args)
                noexcept(std::is_nothrow_constructible_v<T, Args...>) :
           _lifecycle{&lifecycle}, value{std::forward<Args>(args)...}
        {
            ++_lifecycle->constructs;
        }

        constexpr lifecycle_observer(lifecycle_observer&& other) noexcept(std::is_nothrow_move_constructible_v<T>) :
            _lifecycle{other._lifecycle}, value{std::move(other.value)}
        {
            ++_lifecycle->move_constructs;
        }

        constexpr lifecycle_observer(lifecycle_observer const& other)
                noexcept(std::is_nothrow_copy_constructible_v<T>) :
            _lifecycle{other._lifecycle}, value{other.value}
        {
            ++_lifecycle->copy_constructs;
        }

        constexpr ~lifecycle_observer() {
            ++_lifecycle->destructs;
        }

        constexpr lifecycle_observer& operator=(lifecycle_observer&& other)
                noexcept(std::is_nothrow_move_assignable_v<T>) {
            assert(_lifecycle == other._lifecycle);
            ++_lifecycle->move_assigns;
            value = std::move(other.value);
            return *this;
        }

        constexpr lifecycle_observer& operator=(lifecycle_observer const& other)
                noexcept(std::is_nothrow_copy_assignable_v<T>) {
            assert(_lifecycle == other._lifecycle);
            ++_lifecycle->copy_assigns;
            value = other.value;
            return *this;
        }

        constexpr operator T&() noexcept {
            return value;
        }

        constexpr operator T const&() const noexcept {
            return value;
        }

    private:
        lifecycle_data* _lifecycle;
    };

}

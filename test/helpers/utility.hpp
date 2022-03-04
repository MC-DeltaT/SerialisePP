#pragma once

#include <compare>
#include <concepts>
#include <optional>
#include <typeinfo>
#include <utility>

#include <serialpp/utility.hpp>


namespace serialpp::test {

    // Needs to be a concept to test failure condition.
    template<class Tuple, ConstantString N>
    concept CanGetNamedTuple = requires (Tuple t) {
        t.get<N>();
    };


    struct LifecycleData {
        std::size_t constructs = 0;
        std::size_t destructs = 0;
        std::size_t move_constructs = 0;
        std::size_t copy_constructs = 0;
        std::size_t move_assigns = 0;
        std::size_t copy_assigns = 0;

        [[nodiscard]]
        friend constexpr auto operator<=>(LifecycleData const&, LifecycleData const&) = default;
    };


    template<std::regular T>
    class LifecycleObserver {
    public:
        T value;

        template<typename... Args> requires std::constructible_from<T, Args...>
        explicit(sizeof...(Args) == 0) LifecycleObserver(LifecycleData& lifecycle, Args&&... args) :
           _lifecycle{&lifecycle}, value{std::forward<Args>(args)...}
        {
            ++_lifecycle->constructs;
        }

        LifecycleObserver(LifecycleObserver&& other) noexcept :
            _lifecycle{other._lifecycle}, value{std::move(other.value)}
        {
            ++_lifecycle->move_constructs;
        }

        LifecycleObserver(LifecycleObserver const& other) :
            _lifecycle{other._lifecycle}, value{other.value}
        {
            ++_lifecycle->copy_constructs;
        }

        ~LifecycleObserver() {
            ++_lifecycle->destructs;
        }

        LifecycleObserver& operator=(LifecycleObserver&& other) noexcept {
            // Assume _lifecycle == other._lifecycle
            ++_lifecycle->move_assigns;
            value = std::move(other.value);
            return *this;
        }

        LifecycleObserver& operator=(LifecycleObserver const& other) {
            // Assume _lifecycle == other._lifecycle
            ++_lifecycle->copy_assigns;
            value = other.value;
            return *this;
        }

        operator T&() noexcept {
            return value;
        }

        operator T const&() const noexcept {
            return value;
        }

    private:
        LifecycleData* _lifecycle;
    };


    template<typename T>
    struct MockSmallAnyVisitor {
        T const* value = nullptr;
        std::type_info const* type = nullptr;

        template<typename T>
        void operator()(T&& value) {
            this->value = &value;
            type = &typeid(T);
        }
    };

}


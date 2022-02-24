#pragma once

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


    class LifecycleObserver {
    public:
        std::optional<int> tag;

        LifecycleObserver(std::size_t& constructions, std::size_t& destructions, int tag) :
            _constructions{&constructions}, _destructions{&destructions}, tag{tag}
        {
            ++*_constructions;
        }

        LifecycleObserver(LifecycleObserver&& other) :
            _constructions{other._constructions}, _destructions{other._destructions}, tag{std::move(other.tag)}
        {
            ++*_constructions;
        }

        LifecycleObserver(LifecycleObserver const& other) :
            _constructions{other._constructions}, _destructions{other._destructions}, tag{other.tag}
        {
            ++*_constructions;
        }

        ~LifecycleObserver() {
            ++*_destructions;
        }

        LifecycleObserver& operator=(LifecycleObserver&&) = default;
        LifecycleObserver& operator=(LifecycleObserver const&) = default;

    private:
        std::size_t* _constructions;
        std::size_t* _destructions;
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


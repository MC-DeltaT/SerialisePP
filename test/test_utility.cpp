#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <new>
#include <optional>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include <serialpp/utility.hpp>

#include "helpers/lifecycle.hpp"
#include "helpers/mock_small_any_visitor.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block utility_tests = [] {

    test_case("constant_string construct") = [] {
        constexpr constant_string s{"hello world"};
        static_assert(sizeof(s.data) == 11);
        static_assert(std::equal(s.data, s.data + 11, "hello world"));
    };

    test_case("constant_string string_view()") = [] {
        constexpr constant_string s{"hello world"};
        static_assert(s.string_view() == std::string_view{"hello world"});
    };

    test_case("constant_string operator==()") = [] {
        constexpr constant_string s1{"foo bar"};
        constexpr constant_string s2{"foo bar"};
        constexpr constant_string s3{"qux qux"};
        constexpr constant_string s4{"string"};
        static_assert(s1 == s2);
        static_assert(!(s1 == s3));
        static_assert(!(s1 == s4));
    };


    test_case("constexpr_unique_ptr default construct") = [] {
        []() consteval {
            detail::constexpr_unique_ptr<int> pointer;
            test_assert(pointer.get() == nullptr);
            test_assert(pointer.operator->() == nullptr);
            test_assert(!pointer);
        }();
    };

    test_case("constexpr_unique_ptr construct destruct object") = [] {
        []() consteval {
            lifecycle_data lifecycle;
            {
                auto const o = new lifecycle_observer<int>{lifecycle, 42};
                detail::constexpr_unique_ptr<lifecycle_observer<int>> pointer{o};
                test_assert(pointer.get() == o);
                test_assert(pointer.operator->() == o);
                test_assert((*pointer).value == 42);
                test_assert(static_cast<bool>(pointer));
                test_assert(lifecycle == lifecycle_data{.constructs = 1});
            }
            test_assert(lifecycle == lifecycle_data{.constructs = 1, .destructs = 1});
        }();
    };

    test_case("constexpr_unique_ptr construct destruct array") = [] {
        []() consteval {
            lifecycle_data lifecycle1;
            lifecycle_data lifecycle2;
            {
                auto const o = new lifecycle_observer<int>[2]{{lifecycle1, 1}, {lifecycle2, 2}};
                detail::constexpr_unique_ptr<lifecycle_observer<int>[]> pointer{o};
                test_assert(pointer.get() == o);
                test_assert(lifecycle1 == lifecycle_data{.constructs = 1});
                test_assert(lifecycle2 == lifecycle_data{.constructs = 1});
            }
            test_assert(lifecycle1 == lifecycle_data{.constructs = 1, .destructs = 1});
            test_assert(lifecycle2 == lifecycle_data{.constructs = 1, .destructs = 1});
        }();
    };

    test_case("constexpr_unique_ptr move construct") = [] {
        []() consteval {
            lifecycle_data lifecycle;
            {
                auto const o = new lifecycle_observer<int>{lifecycle, 42};
                detail::constexpr_unique_ptr<lifecycle_observer<int>> pointer1{o};
                test_assert(pointer1.get() == o);
                test_assert(lifecycle == lifecycle_data{.constructs = 1});
                detail::constexpr_unique_ptr<lifecycle_observer<int>> pointer2{std::move(pointer1)};
                test_assert(lifecycle == lifecycle_data{.constructs = 1});
            }
            test_assert(lifecycle == lifecycle_data{.constructs = 1, .destructs = 1});
        }();
    };

    test_case("constexpr_unique_ptr move assign") = [] {
        []() consteval {
            lifecycle_data lifecycle1;
            lifecycle_data lifecycle2;
            {
                auto const o1 = new lifecycle_observer<int>{lifecycle1, 42};
                detail::constexpr_unique_ptr<lifecycle_observer<int>> pointer1{o1};
                test_assert(lifecycle1 == lifecycle_data{.constructs = 1});
                auto const o2 = new lifecycle_observer<int>{lifecycle2, 583};
                detail::constexpr_unique_ptr<lifecycle_observer<int>> pointer2{o2};
                test_assert(lifecycle2 == lifecycle_data{.constructs = 1});
                pointer1 = std::move(pointer2);
                test_assert(pointer1.get() == o2);
                test_assert(pointer1.operator->() == o2);
                test_assert((*pointer1).value == 583);
                test_assert(lifecycle1 == lifecycle_data{.constructs = 1, .destructs = 1});
            }
            test_assert(lifecycle1 == lifecycle_data{.constructs = 1, .destructs = 1});
            test_assert(lifecycle2 == lifecycle_data{.constructs = 1, .destructs = 1});
        }();
    };

    test_case("constexpr_unique_ptr reset() nullptr") = [] {
        []() consteval {
            lifecycle_data lifecycle;
            {
                auto const o = new lifecycle_observer<int>{lifecycle, 42};
                detail::constexpr_unique_ptr<lifecycle_observer<int>> pointer{o};
                test_assert(lifecycle == lifecycle_data{.constructs = 1});
                pointer.reset();
                test_assert(pointer.get() == nullptr);
                test_assert(pointer.operator->() == nullptr);
                test_assert(!pointer);
                test_assert(lifecycle == lifecycle_data{.constructs = 1, .destructs = 1});
            }
            test_assert(lifecycle == lifecycle_data{.constructs = 1, .destructs = 1});
        }();
    };

    test_case("constexpr_unique_ptr reset() new object") = [] {
        []() consteval {
            lifecycle_data lifecycle1;
            lifecycle_data lifecycle2;
            {
                auto const o1 = new lifecycle_observer<int>{lifecycle1, 42};
                detail::constexpr_unique_ptr<lifecycle_observer<int>> pointer{o1};
                test_assert(lifecycle1 == lifecycle_data{.constructs = 1});
                auto const o2 = new lifecycle_observer<int>{lifecycle2, 583};
                pointer.reset(o2);
                test_assert(pointer.get() == o2);
                test_assert(pointer.operator->() == o2);
                test_assert((*pointer).value == 583);
                test_assert(lifecycle1 == lifecycle_data{.constructs = 1, .destructs = 1});
                test_assert(lifecycle2 == lifecycle_data{.constructs = 1});
            }
            test_assert(lifecycle1 == lifecycle_data{.constructs = 1, .destructs = 1});
            test_assert(lifecycle2 == lifecycle_data{.constructs = 1, .destructs = 1});
        }();
    };


    static_assert(!detail::small_any<3, 8, mock_small_any_visitor<std::uint64_t>>::can_contain<std::uint64_t>());
    static_assert(!detail::small_any<8, 1, mock_small_any_visitor<std::uint64_t>>::can_contain<std::uint64_t>());
    static_assert(detail::small_any<8, 8, mock_small_any_visitor<std::uint64_t>>::can_contain<std::uint64_t>());

    test_case("small_any emplace() args") = [] {
        using value_type = lifecycle_observer<int>;
        lifecycle_data lifecycle;
        {
            detail::small_any<sizeof(value_type), alignof(value_type), mock_small_any_visitor<value_type>> any;
            any.emplace<value_type>(lifecycle, 13);
            test_assert(lifecycle == lifecycle_data{.constructs = 1});
        }
        test_assert(lifecycle == lifecycle_data{.constructs = 1, .destructs = 1});
    };

    test_case("small_any emplace() func") = [] {
        using value_type = lifecycle_observer<int>;
        lifecycle_data lifecycle;
        {
            detail::small_any<sizeof(value_type), alignof(value_type), mock_small_any_visitor<value_type>> any;
            any.emplace([&lifecycle] (void* data) {
                return new(data) value_type{lifecycle, 13};
            });
            test_assert(lifecycle == lifecycle_data{.constructs = 1});
        }
        test_assert(lifecycle == lifecycle_data{.constructs = 1, .destructs = 1});
    };

    test_case("small_any move construct mock") = [] {
        using value_type = lifecycle_observer<std::optional<int>>;
        lifecycle_data lifecycle;
        {
            detail::small_any<sizeof(value_type), alignof(value_type), mock_small_any_visitor<value_type>> any1;
            any1.emplace<value_type>(lifecycle, 13);
            test_assert(lifecycle == lifecycle_data{.constructs = 1});
            decltype(any1) any2{std::move(any1)};
            test_assert(lifecycle == lifecycle_data{.constructs = 1, .move_constructs = 1});
            decltype(any2) const any3{std::move(any2)};
            test_assert(lifecycle == lifecycle_data{.constructs = 1, .move_constructs = 2});

            mock_small_any_visitor<value_type> visitor;
            any3.visit(visitor);
            test_assert(visitor.value);
            test_assert(visitor.value->value.has_value());
            test_assert(visitor.value->value.value() == 13);
        }
        test_assert(lifecycle == lifecycle_data{.constructs = 1, .destructs = 1, .move_constructs = 2});
    };

    test_case("small_any move construct vector") = [] {
        using value_type = std::vector<long>;
        // Check move for a contained type which manages resources (i.e. will crash if move is broken).
        detail::small_any<sizeof(value_type), alignof(value_type), mock_small_any_visitor<value_type>> any1;
        any1.emplace<value_type>(std::initializer_list<long>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        decltype(any1) any2{std::move(any1)};
        decltype(any2) any3{std::move(any2)};
    };

    test_case("small_any visit()") = [] {
        using value_type = lifecycle_observer<std::optional<int>>;
        lifecycle_data lifecycle;
        {
            detail::small_any<sizeof(value_type), alignof(value_type), mock_small_any_visitor<value_type>> any;
            any.emplace<value_type>(lifecycle, 13);
            test_assert(lifecycle == lifecycle_data{.constructs = 1});

            mock_small_any_visitor<value_type> visitor;
            any.visit(visitor);
            test_assert(visitor.value);
            test_assert(visitor.value->value.has_value());
            test_assert(visitor.value->value.value() == 13);
            test_assert(visitor.type);
            test_assert(*visitor.type == typeid(value_type const&));
        }
        test_assert(lifecycle == lifecycle_data{.constructs = 1, .destructs = 1});
    };

};
}

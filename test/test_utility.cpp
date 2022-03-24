#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include <serialpp/utility.hpp>

#include "helpers/test.hpp"
#include "helpers/utility.hpp"


namespace serialpp::test {

    STEST_CASE(ConstantString_Construct) {
        constexpr ConstantString s{"hello world"};
        static_assert(sizeof(s.data) == 11);
        static_assert(std::equal(s.data, s.data + 11, "hello world"));
    }

    STEST_CASE(ConstantString_StringView) {
        constexpr ConstantString s{"hello world"};
        static_assert(s.string_view() == std::string_view{"hello world"});
    }

    STEST_CASE(ConstantString_Equality) {
        constexpr ConstantString s1{"foo bar"};
        constexpr ConstantString s2{"foo bar"};
        constexpr ConstantString s3{"qux qux"};
        constexpr ConstantString s4{"string"};
        static_assert(s1 == s2);
        static_assert(!(s1 == s3));
        static_assert(!(s1 == s4));
    }


    static_assert(!std::constructible_from<
        SmallAny<3, 8, MockSmallAnyVisitor<std::uint64_t>>,
        std::in_place_type_t<std::uint64_t>,
        std::uint64_t>);
    static_assert(!std::constructible_from<
        SmallAny<8, 1, MockSmallAnyVisitor<std::uint64_t>>,
        std::in_place_type_t<std::uint64_t>,
        std::uint64_t>);
    static_assert(std::constructible_from<
        SmallAny<8, 8, MockSmallAnyVisitor<std::uint64_t>>,
        std::in_place_type_t<std::uint64_t>,
        std::uint64_t>);

    STEST_CASE(SmallAny_ConstructDestruct) {
        LifecycleData lifecycle;
        {
            SmallAny<sizeof(LifecycleObserver<int>), alignof(LifecycleObserver<int>), MockSmallAnyVisitor<LifecycleObserver<int>>>
                const any{std::in_place_type<LifecycleObserver<int>>, lifecycle, 13};
            test_assert(lifecycle == LifecycleData{.constructs = 1});
        }
        test_assert(lifecycle == LifecycleData{.constructs = 1, .destructs = 1});
    }

    STEST_CASE(SmallAny_MoveConstruct1) {
        using ValueType = LifecycleObserver<std::optional<int>>;
        LifecycleData lifecycle;
        {
            SmallAny<sizeof(ValueType), alignof(ValueType), MockSmallAnyVisitor<ValueType>>
                any1{std::in_place_type<ValueType>, lifecycle, 13};
            test_assert(lifecycle == LifecycleData{.constructs = 1});
            decltype(any1) any2{std::move(any1)};
            test_assert(lifecycle == LifecycleData{.constructs = 1, .move_constructs = 1});
            decltype(any2) const any3{std::move(any2)};
            test_assert(lifecycle == LifecycleData{.constructs = 1, .move_constructs = 2});

            MockSmallAnyVisitor<ValueType> visitor;
            any3.visit(visitor);
            test_assert(visitor.value);
            test_assert(visitor.value->value.has_value());
            test_assert(visitor.value->value.value() == 13);
        }
        test_assert(lifecycle == LifecycleData{.constructs = 1, .destructs = 1, .move_constructs = 2});
    }

    STEST_CASE(SmallAny_MoveConstruct2) {
        using ValueType = std::vector<long>;
        // Check move for a contained type which manages resources (i.e. will crash if move is broken).
        SmallAny<sizeof(ValueType), alignof(ValueType), MockSmallAnyVisitor<ValueType>>
            any1{std::in_place_type<ValueType>, std::initializer_list<long>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
        decltype(any1) any2{std::move(any1)};
        decltype(any2) any3{std::move(any2)};
    }

    STEST_CASE(SmallAny_VisitNonConst) {
        using ValueType = LifecycleObserver<std::optional<int>>;
        LifecycleData lifecycle;
        {
            SmallAny<sizeof(ValueType), alignof(ValueType), MockSmallAnyVisitor<ValueType>>
                any{std::in_place_type<ValueType>, lifecycle, 13};
            test_assert(lifecycle == LifecycleData{.constructs = 1});

            MockSmallAnyVisitor<ValueType> visitor;
            any.visit(visitor);
            test_assert(visitor.value);
            test_assert(visitor.value->value.has_value());
            test_assert(visitor.value->value.value() == 13);
            test_assert(visitor.type);
            test_assert(*visitor.type == typeid(ValueType&));
        }
        test_assert(lifecycle == LifecycleData{.constructs = 1, .destructs = 1});
    }

    STEST_CASE(SmallAny_VisitConst) {
        using ValueType = LifecycleObserver<std::optional<int>>;
        LifecycleData lifecycle;
        {
            SmallAny<sizeof(ValueType), alignof(ValueType), MockSmallAnyVisitor<ValueType>>
                const any{std::in_place_type<ValueType>, lifecycle, 13};
            test_assert(lifecycle == LifecycleData{.constructs = 1});

            MockSmallAnyVisitor<ValueType> visitor;
            any.visit(visitor);
            test_assert(visitor.value);
            test_assert(visitor.value->value.has_value());
            test_assert(visitor.value->value.value() == 13);
            test_assert(visitor.type);
            test_assert(*visitor.type == typeid(ValueType const&));
        }
        test_assert(lifecycle == LifecycleData{.constructs = 1, .destructs = 1});
    }

}

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>

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


    STEST_CASE(NamedTuple_DefaultConstruct) {
        NamedTuple<
                NamedTupleElement<"foo", int>,
                NamedTupleElement<"bar", char>,
                NamedTupleElement<"qux", int>,
                NamedTupleElement<"my_str", std::string>>
            tuple{};

        test_assert(tuple.get<"foo">() == 0);
        test_assert(tuple.get<"bar">() == 0);
        test_assert(tuple.get<"qux">() == 0);
        test_assert(tuple.get<"my_str">() == std::string{});
    }

    STEST_CASE(NamedTuple_Get_Const) {
        NamedTuple<
                NamedTupleElement<"foo", int>,
                NamedTupleElement<"bar", char>,
                NamedTupleElement<"qux", int>,
                NamedTupleElement<"my_str", std::string>>
            const tuple{42, 'c', 56, "foo bar"};

        test_assert(tuple.get<"foo">() == 42);
        test_assert(tuple.get<"bar">() == 'c');
        test_assert(tuple.get<"qux">() == 56);
        test_assert(tuple.get<"my_str">() == std::string{"foo bar"});
    }

    STEST_CASE(NamedTuple_Get_Nonconst) {
        NamedTuple<
                NamedTupleElement<"foo", int>,
                NamedTupleElement<"bar", char>,
                NamedTupleElement<"qux", int>,
                NamedTupleElement<"my_str", std::string>>
            tuple{42, 'c', 56, "foo bar"};

        tuple.get<"bar">() = 'h';
        tuple.get<"my_str">() = "bar qux";

        test_assert(tuple.get<"foo">() == 42);
        test_assert(tuple.get<"bar">() == 'h');
        test_assert(tuple.get<"qux">() == 56);
        test_assert(tuple.get<"my_str">() == std::string{"bar qux"});
    }

    STEST_CASE(NamedTuple_Get_Nonexistent) {
        using Tuple = NamedTuple<
            NamedTupleElement<"foo", int>,
            NamedTupleElement<"bar", char>,
            NamedTupleElement<"qux", int>,
            NamedTupleElement<"my_str", std::string>>;

        static_assert(!CanGetNamedTuple<Tuple, "oh no">);
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
        std::size_t constructions = 0;
        std::size_t destructions = 0;
        {
            SmallAny<sizeof(LifecycleObserver), alignof(LifecycleObserver), MockSmallAnyVisitor<LifecycleObserver>>
                const any{std::in_place_type<LifecycleObserver>, constructions, destructions, 13};
            test_assert(constructions == 1);
            test_assert(destructions == 0);
        }
        test_assert(constructions == 1);
        test_assert(destructions == 1);
    }

    STEST_CASE(SmallAny_MoveConstruct) {
        std::size_t constructions = 0;
        std::size_t destructions = 0;
        {
            SmallAny<sizeof(LifecycleObserver), alignof(LifecycleObserver), MockSmallAnyVisitor<LifecycleObserver>>
                any1{std::in_place_type<LifecycleObserver>, constructions, destructions, 13};
            test_assert(constructions == 1);
            test_assert(destructions == 0);
            decltype(any1) const any2{std::move(any1)};
            test_assert(constructions == 1);
            test_assert(destructions == 1);

            MockSmallAnyVisitor<LifecycleObserver> visitor;
            any2.visit(visitor);
            test_assert(visitor.value);
            test_assert(visitor.value->tag.has_value());
            test_assert(visitor.value->tag.value() == 13);
        }
        test_assert(constructions == 1);
        test_assert(destructions == 2);
    }

    STEST_CASE(SmallAny_VisitNonConst) {
        std::size_t constructions = 0;
        std::size_t destructions = 0;
        {
            SmallAny<sizeof(LifecycleObserver), alignof(LifecycleObserver), MockSmallAnyVisitor<LifecycleObserver>>
                any{std::in_place_type<LifecycleObserver>, constructions, destructions, 13};
            test_assert(constructions == 1);
            test_assert(destructions == 0);

            MockSmallAnyVisitor<LifecycleObserver> visitor;
            any.visit(visitor);
            test_assert(visitor.value);
            test_assert(visitor.value->tag.has_value());
            test_assert(visitor.value->tag.value() == 13);
            test_assert(visitor.type);
            test_assert(*visitor.type == typeid(LifecycleObserver&));
        }
        test_assert(constructions == 1);
        test_assert(destructions == 1);
    }

    STEST_CASE(SmallAny_VisitConst) {
        std::size_t constructions = 0;
        std::size_t destructions = 0;
        {
            SmallAny<sizeof(LifecycleObserver), alignof(LifecycleObserver), MockSmallAnyVisitor<LifecycleObserver>>
                const any{std::in_place_type<LifecycleObserver>, constructions, destructions, 13};
            test_assert(constructions == 1);
            test_assert(destructions == 0);

            MockSmallAnyVisitor<LifecycleObserver> visitor;
            any.visit(visitor);
            test_assert(visitor.value);
            test_assert(visitor.value->tag.has_value());
            test_assert(visitor.value->tag.value() == 13);
            test_assert(visitor.type);
            test_assert(*visitor.type == typeid(LifecycleObserver const&));
        }
        test_assert(constructions == 1);
        test_assert(destructions == 1);
    }

}

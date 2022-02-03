#include <concepts>
#include <string>
#include <utility>

#include <serialpp/utility.hpp>

#include "test.hpp"


namespace serialpp::test {

    STEST_CASE(MakeTypeList) {
        int a = 42;
        char const b = 'h';
        constexpr long c = 1234L;
        std::string const& d = std::string{};
        static_assert(std::same_as<decltype(make_type_list(a, b, c, d)), TypeList<int, char, long, std::string>>);
    }


    static_assert(TYPE_LIST_INDEX<TypeList<int, char, float>, int> == 0);
    static_assert(TYPE_LIST_INDEX<TypeList<int, char, float>, float> == 2);
    static_assert(TYPE_LIST_INDEX<TypeList<char, int, int, char>, int> == 1);


    STEST_CASE(TaggedTuple_Get) {
        struct Tag1 {};
        struct Tag2 {};
        struct Tag3 {};
        struct Tag4 {};

        TaggedTuple<
            TaggedType<int, Tag1>, TaggedType<char, Tag2>, TaggedType<int, Tag3>, TaggedType<std::string, Tag4>>
            const tuple{42, 'c', 56, "foo bar"};
        
        test_assert(tuple.get<Tag1>() == 42);
        test_assert(tuple.get<Tag2>() == 'c');
        test_assert(tuple.get<Tag3>() == 56);
        test_assert(tuple.get<Tag4>() == std::string{"foo bar"});
    }

}

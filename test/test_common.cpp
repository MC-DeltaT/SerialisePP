#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block common_tests = [] {

    static_assert(!serialisable<std::hash<int>>);


    test_case("to_data_offset() valid") = [] {
        test_assert(detail::to_data_offset(123456) == 123456u);
    };

    test_case("to_data_offset() invalid") = [] {
        test_assert_throws<object_size_error>([] {
            (void)detail::to_data_offset(123'456'789'000ull);
        });
    };


    test_case("serialise_target subobject_fixed_data()") = [] {
        basic_serialise_buffer buffer;
        buffer.initialise(100);
        serialise_target const target{buffer, 70, 33, 8, 75};
        mutable_bytes_span const fixed_data = target.subobject_fixed_data();
        test_assert(fixed_data.data() == buffer.span().data() + 33);
        test_assert(fixed_data.size() == 8);
    };

    test_case("serialise_target relative_subobject_variable_offset()") = [] {
        basic_serialise_buffer buffer;
        buffer.initialise(100);
        serialise_target const target{buffer, 70, 33, 8, 75};
        test_assert(target.relative_subobject_variable_offset() == 5);
    };

    test_case("serialise_target enter_fixed_subobject()") = [] {
        basic_serialise_buffer buffer;
        buffer.initialise(20);
        serialise_target const target{buffer, 20, 5, 15, 20};
        auto const [subobject_target, context] = target.enter_fixed_subobject<mock_serialisable<9>>();

        test_assert(buffer.span().size() == 20);
        serialise_target const expected_subobject_target{buffer, 20, 5, 9, 20};
        test_assert(subobject_target == expected_subobject_target);
        decltype(target)::push_fixed_subobject_context const expected_context{5, 15, 9};
        test_assert(context == expected_context);
    };

    test_case("serialise_target exit_fixed_subobject()") = [] {
        basic_serialise_buffer buffer;
        buffer.initialise(20);
        serialise_target const target{buffer, 20, 5, 9, 20};
        decltype(target)::push_fixed_subobject_context const context{5, 15, 9};
        serialise_target<basic_serialise_buffer<>> const new_target = target.exit_fixed_subobject(context);

        test_assert(buffer.span().size() == 20);
        serialise_target const expected_new_target{buffer, 20, 14, 6, 20};
        test_assert(new_target == expected_new_target);
    };

    test_case("serialise_target push_fixed_subobject()") = [] {
        basic_serialise_buffer<> buffer;
        buffer.initialise(100);
        serialise_target const target{buffer, 60, 20, 10, 80};

        bool func_called = false;
        serialise_target<basic_serialise_buffer<>> const new_target = target.push_fixed_subobject<mock_serialisable<4>>(
            [&func_called, &buffer](serialise_target<basic_serialise_buffer<>> subobject_target) {
                func_called = true;
                serialise_target const expected_subobject_target{buffer, 60, 20, 4, 80};
                test_assert(subobject_target == expected_subobject_target);
                return subobject_target;
            });
        test_assert(func_called);

        serialise_target const expected_new_target{buffer, 60, 24, 6, 80};
        test_assert(new_target == expected_new_target);
    };

    test_case("serialise_target enter_variable_subobjects()") = [] {
        basic_serialise_buffer buffer;
        buffer.initialise(20);
        serialise_target const target{buffer, 20, 5, 15, 20};
        auto const [subobject_target, context] = target.enter_variable_subobjects<mock_serialisable<3>>(5);

        test_assert(buffer.span().size() == 35);
        serialise_target const expected_subobject_target{buffer, 20, 20, 15, 35};
        test_assert(subobject_target == expected_subobject_target);
        decltype(target)::push_variable_subobjects_context const expected_context{20, 5, 15};
        test_assert(context == expected_context);
    };

    test_case("serialise_target exit_variable_subobjects()") = [] {
        basic_serialise_buffer buffer;
        buffer.initialise(35);
        serialise_target const target{buffer, 20, 20, 15, 35};
        decltype(target)::push_variable_subobjects_context const context{20, 5, 15};
        serialise_target<basic_serialise_buffer<>> const new_target = target.exit_variable_subobjects(context);

        test_assert(buffer.span().size() == 35);
        serialise_target const expected_new_target{buffer, 20, 5, 15, 35};
        test_assert(new_target == expected_new_target);
    };

    test_case("serialise_target push_variable_subobjects()") = [] {
        basic_serialise_buffer<> buffer;
        buffer.initialise(45);
        serialise_target const target{buffer, 30, 7, 13, 40};

        bool func_called = false;
        serialise_target<basic_serialise_buffer<>> const new_target =
            target.push_variable_subobjects<mock_serialisable<8>>(6,
                [&func_called, &buffer](serialise_target<basic_serialise_buffer<>> subobject_target) {
                    func_called = true;
                    serialise_target const expected_subobject_target{buffer, 30, 40, 48, 88};
                    test_assert(subobject_target == expected_subobject_target);
                    return subobject_target;
                });
        test_assert(func_called);

        test_assert(buffer.span().size() == 93);

        serialise_target const expected_new_target{buffer, 30, 7, 13, 88};
        test_assert(new_target == expected_new_target);
    };

    test_case("serialise_target push nested subobjects") = [] {
        basic_serialise_buffer<> buffer;
        buffer.initialise(100);
        serialise_target const target{buffer, 50, 10, 50, 50};

        bool func1_called = false;
        serialise_target<basic_serialise_buffer<>> const new_target = target.push_fixed_subobject<mock_serialisable<8>>(
            [&func1_called, &buffer](serialise_target<basic_serialise_buffer<>> subobject_target) {
                func1_called = true;
                serialise_target const expected_subobject_target{buffer, 50, 10, 8, 50};
                test_assert(subobject_target == expected_subobject_target);

                bool func2_called = false;
                serialise_target<basic_serialise_buffer<>> const new_target =
                    subobject_target.push_variable_subobjects<mock_serialisable<4>>(2,
                    [&func2_called, &buffer](serialise_target<basic_serialise_buffer<>> subobject_target) {
                        func2_called = true;
                        serialise_target const expected_subobject_target{buffer, 50, 50, 8, 58};
                        test_assert(subobject_target == expected_subobject_target);

                        bool func3_called = false;
                        serialise_target<basic_serialise_buffer<>> const new_target =
                            subobject_target.push_fixed_subobject<mock_serialisable<2>>(
                            [&func3_called, &buffer](serialise_target<basic_serialise_buffer<>> subobject_target) {
                                func3_called = true;
                                serialise_target const expected_subobject_target{buffer, 50, 50, 2, 58};
                                test_assert(subobject_target == expected_subobject_target);

                                return subobject_target;
                            });
                        test_assert(func3_called);

                        serialise_target const expected_new_target{buffer, 50, 52, 6, 58};
                        test_assert(new_target == expected_new_target);

                        return new_target;
                    });
                test_assert(func2_called);

                serialise_target const expected_new_target{buffer, 50, 10, 8, 58};
                test_assert(new_target == expected_new_target);

                return new_target;
            });
        test_assert(func1_called);

        serialise_target const expected_new_target{buffer, 50, 18, 42, 58};
        test_assert(new_target == expected_new_target);
    };


    test_case("initialise_buffer()") = [] {
        basic_serialise_buffer buffer;
        serialise_target<basic_serialise_buffer<>> const target = initialise_buffer<mock_serialisable<546>>(buffer);
        test_assert(buffer.span().size() == 546);
        serialise_target const expected_target{buffer, 546, 0, 546, 546};
        test_assert(target == expected_target);
    };


    test_case("auto_deserialise() disabled") = [] {
        std::array<std::byte, 100> const buffer{};
        deserialiser<mock_serialisable<23>> const deser = deserialise<mock_serialisable<23>>(const_bytes_span{buffer});
        deserialiser<mock_serialisable<23>> const result = auto_deserialise(deser);
        test_assert(result == deser);
    };


    test_case("serialise()") = [] {
        basic_serialise_buffer buffer;
        serialise_source<mock_serialisable<5>> const source;
        serialise(source, buffer);

        test_assert(source.targets.size() == 1);
        serialise_target const expected_target{buffer, 5, 0, 5, 5};
        test_assert(source.targets.at(0) == expected_target);
    };


    test_case("deserialise()") = [] {
        std::array<std::byte, 10> const buffer{};
        deserialiser<mock_serialisable<7>> const deser = deserialise<mock_serialisable<7>>(buffer);
        const_bytes_span expected_fixed_data{buffer.data(), buffer.data() + 7};
        test_assert(bytes_view_same(deser._fixed_data, expected_fixed_data));
        const_bytes_span expected_variable_data{buffer.data() + 7, buffer.data() + 10};
        test_assert(bytes_view_same(deser._variable_data, expected_variable_data));
    };

    test_case("deserialise() buffer too small") = [] {
        std::array<std::byte, 10> const buffer{};
        test_assert_throws<fixed_buffer_size_error>([&buffer] {
            (void)deserialise<mock_serialisable<11>>(buffer);
        });
    };

};
}

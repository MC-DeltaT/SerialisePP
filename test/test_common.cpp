#include <cstddef>
#include <cstdint>

#include <serialpp/common.hpp>

#include "helpers/common.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {

    STEST_CASE(ToDataOffset_Valid) {
        test_assert(to_data_offset(102) == 102u);
    }


    STEST_CASE(SerialiseBuffer_Construct) {
        SerialiseBuffer buffer;
        auto const span = buffer.span();
        test_assert(span.empty());
    }

    STEST_CASE(SerialiseBuffer_InitialiseFor) {
        SerialiseBuffer buffer;
        auto const target = buffer.initialise<MockSerialisable<8>>();

        SerialiseTarget const expected_target{buffer, 8, 0, 8, 8};
        test_assert(target == expected_target);

        auto const span = buffer.span();
        test_assert(span.size() == 8);
    }

    STEST_CASE(SerialiseBuffer_Extend) {
        SerialiseBuffer buffer;

        buffer.extend(100);
        auto span = buffer.span();
        test_assert(span.size() == 100);

        span[0] = std::byte{10};
        span[99] = std::byte{42};
        buffer.extend(100);
        span = buffer.span();
        test_assert(span.size() == 200);
        test_assert(span[0] == std::byte{10});
        test_assert(span[99] == std::byte{42});
    }


    STEST_CASE(SerialiseTarget_FieldFixedData) {
        SerialiseBuffer buffer;
        buffer.extend(100);
        SerialiseTarget const target{buffer, 70, 33, 8, 75};
        auto const fixed_data = target.field_fixed_data();
        test_assert(fixed_data.data() == buffer.span().data() + 33);
        test_assert(fixed_data.size() == 8);
    }

    STEST_CASE(SerialiseTarget_RelativeFieldVariableOffset) {
        SerialiseBuffer buffer;
        buffer.extend(100);
        SerialiseTarget const target{buffer, 70, 33, 8, 75};
        test_assert(target.relative_field_variable_offset() == 5);
    }

    STEST_CASE(SerialiseTarget_PushFixedField) {
        SerialiseBuffer buffer;
        buffer.extend(100);
        SerialiseTarget const target{buffer, 60, 20, 10, 80};

        bool func_called = false;
        auto const new_target = target.push_fixed_field<MockSerialisable<4>>(
            [&func_called, &buffer](SerialiseTarget field_target) {
                func_called = true;
                SerialiseTarget const expected_field_target{buffer, 60, 20, 4, 80};
                test_assert(field_target == expected_field_target);
                return field_target;
            });
        test_assert(func_called);

        SerialiseTarget const expected_new_target{buffer, 60, 24, 6, 80};
        test_assert(new_target == expected_new_target);
    }

    STEST_CASE(SerialiseTarget_PushVariableField) {
        SerialiseBuffer buffer;
        buffer.extend(45);
        SerialiseTarget const target{buffer, 30, 7, 13, 40};

        bool func_called = false;
        auto const new_target = target.push_variable_field<MockSerialisable<8>>(
            [&func_called, &buffer](SerialiseTarget field_target) {
                func_called = true;
                SerialiseTarget const expected_field_target{buffer, 30, 40, 8, 48};
                test_assert(field_target == expected_field_target);
                return field_target;
            });
        test_assert(func_called);

        test_assert(buffer.span().size() == 48);

        SerialiseTarget const expected_new_target{buffer, 30, 7, 13, 48};
        test_assert(new_target == expected_new_target);
    }

    STEST_CASE(SerialiseTarget_PushNestedFields) {
        SerialiseBuffer buffer;
        buffer.extend(100);
        SerialiseTarget const target{buffer, 50, 10, 50, 50};

        bool func1_called = false;
        auto const new_target = target.push_fixed_field<MockSerialisable<8>>(
            [&func1_called, &buffer](SerialiseTarget field_target) {
                func1_called = true;
                SerialiseTarget const expected_field_target{buffer, 50, 10, 8, 50};
                test_assert(field_target == expected_field_target);

                bool func2_called = false;
                auto const new_target = field_target.push_variable_field<MockSerialisable<4>>(
                    [&func2_called, &buffer](SerialiseTarget field_target) {
                        func2_called = true;
                        SerialiseTarget const expected_field_target{buffer, 50, 50, 4, 54};
                        test_assert(field_target == expected_field_target);

                        bool func3_called = false;
                        auto const new_target = field_target.push_fixed_field<MockSerialisable<2>>(
                            [&func3_called, &buffer](SerialiseTarget field_target) {
                                func3_called = true;
                                SerialiseTarget const expected_field_target{buffer, 50, 50, 2, 54};
                                test_assert(field_target == expected_field_target);

                                return field_target;
                            });
                        test_assert(func3_called);

                        SerialiseTarget const expected_new_target{buffer, 50, 52, 2, 54};
                        test_assert(new_target == expected_new_target);

                        return new_target;
                    });
                test_assert(func2_called);

                SerialiseTarget const expected_new_target{buffer, 50, 10, 8, 54};
                test_assert(new_target == expected_new_target);

                return new_target;
            });
        test_assert(func1_called);

        SerialiseTarget const expected_new_target{buffer, 50, 18, 42, 54};
        test_assert(new_target == expected_new_target);
    }


    STEST_CASE(Serialise) {
        SerialiseBuffer buffer;
        SerialiseSource<MockSerialisable<5>> const source{67543};
        serialise(source, buffer);

        auto const serialised_source = Serialiser<MockSerialisable<5>>::source;
        test_assert(serialised_source == source);
        auto const target = Serialiser<MockSerialisable<5>>::target;
        SerialiseTarget const expected_target{buffer, 5, 0, 5, 5};
        test_assert(target == expected_target);
    }


    STEST_CASE(Deserialise) {
        std::array<std::byte, 10> const buffer{};
        auto const deserialiser = deserialise<MockSerialisable<7>>(buffer);
        ConstBytesView expected_fixed_data{buffer.data(), buffer.data() + 7};
        test_assert(bytes_view_same(deserialiser.fixed_data, expected_fixed_data));
        ConstBytesView expected_variable_data{buffer.data() + 7, buffer.data() + 10};
        test_assert(bytes_view_same(deserialiser.variable_data, expected_variable_data));
    }

}

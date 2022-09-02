#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/mock_serialisable.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block common_tests = [] {

    static_assert(!serialisable<std::hash<int>>);
    static_assert(serialisable<test::mock_serialisable<10, true>>);

    static_assert(!fixed_size_serialisable<std::hash<int>>);
    static_assert(!fixed_size_serialisable<test::mock_serialisable<10, true>>);
    static_assert(fixed_size_serialisable<test::mock_serialisable<10, false>>);

    static_assert(!variable_size_serialisable<std::hash<int>>);
    static_assert(!variable_size_serialisable<test::mock_serialisable<10, false>>);
    static_assert(variable_size_serialisable<test::mock_serialisable<10, true>>);


    static_assert(buffer_for<basic_buffer, mock_serialisable<16, false>>);
    static_assert(buffer_for<basic_buffer, mock_serialisable<16, true>>);
    static_assert(buffer_for<mutable_bytes_span, mock_serialisable<16, false>>);
    static_assert(!buffer_for<mutable_bytes_span, mock_serialisable<16, true>>);
    static_assert(!buffer_for<int, mock_serialisable<16, true>>);


    static_assert(std::same_as<
        deserialise_t<mock_serialisable<3, false, false>>, deserialiser<mock_serialisable<3, false, false>>>);
    static_assert(std::same_as<deserialise_t<mock_serialisable<3, false, true>>, std::size_t>);


    test_case("to_data_offset() valid") = [] {
        test_assert(detail::to_data_offset(123456) == 123456u);
    };

    test_case("to_data_offset() invalid") = [] {
        test_assert_throws<object_size_error>([] {
            (void)detail::to_data_offset(123'456'789'000ull);
        });
    };


    test_case("check_buffer_size_for() valid") = [] {
        std::array<std::byte, 100> buffer{};
        mutable_bytes_span const span{buffer};
        detail::check_buffer_size_for<mock_serialisable<59>>(span, 0);
        detail::check_buffer_size_for<mock_serialisable<100>>(span, 0);
        detail::check_buffer_size_for<mock_serialisable<59>>(span, 10);
        detail::check_buffer_size_for<mock_serialisable<59>>(span, 41);
        detail::check_buffer_size_for<mock_serialisable<0>>(mutable_bytes_span{}, 0);
    };

    test_case("check_buffer_size_for() invalid") = [] {
        std::array<std::byte, 100> buffer{};
        mutable_bytes_span const span{buffer};
        test_assert_throws<buffer_bounds_error>([span] {
            detail::check_buffer_size_for<mock_serialisable<101>>(span, 0);
        });
        test_assert_throws<buffer_bounds_error>([span] {
            detail::check_buffer_size_for<mock_serialisable<101>>(span, 12);
        });
        test_assert_throws<buffer_bounds_error>([] {
            detail::check_buffer_size_for<mock_serialisable<101>>({}, 1);
        });
    };


    test_case("with_buffer_for() variable_size_serialisable") = [] {
        basic_buffer buffer;
        with_buffer_for<mock_serialisable<6521, true>>(buffer, [](basic_buffer&) {});
    };

    test_case("with_buffer_for() fixed_size_serialisable buffer arg") = [] {
        basic_buffer buffer;
        with_buffer_for<mock_serialisable<2346, false>>(buffer, [](mutable_bytes_span) {});
    };

    test_case("with_buffer_for() fixed_size_serialisable span arg") = [] {
        mutable_bytes_span buffer;
        with_buffer_for<mock_serialisable<9867, false>>(buffer, [](mutable_bytes_span) {});
    };


    test_case("push_fixed_subobject()") = [] {
        basic_buffer buffer;
        buffer.initialise(100);

        bool func_called = false;
        std::size_t const new_fixed_offset = push_fixed_subobject<mock_serialisable<4>>(20,
            [&func_called, &buffer](std::size_t const subobject_fixed_offset) {
                func_called = true;
                test_assert(subobject_fixed_offset == 20);
            });
        test_assert(func_called);

        test_assert(new_fixed_offset == 24);
    };

    test_case("push_variable_subobjects()") = [] {
        basic_buffer buffer;
        buffer.initialise(45);

        bool func_called = false;
        push_variable_subobjects<mock_serialisable<8>>(6, buffer,
            [&func_called, &buffer](std::size_t const subobject_fixed_offset) {
                func_called = true;
                test_assert(subobject_fixed_offset == 45);
            });
        test_assert(func_called);

        test_assert(buffer.span().size() == 93);
    };

    test_case("push nested subobjects") = [] {
        basic_buffer buffer;
        buffer.initialise(100);

        bool func1_called = false;
        std::size_t const new_fixed_offset = push_fixed_subobject<mock_serialisable<8>>(10,
            [&func1_called, &buffer](std::size_t const subobject_fixed_offset) {
                func1_called = true;
                test_assert(subobject_fixed_offset == 10);

                bool func2_called = false;
                push_variable_subobjects<mock_serialisable<4>>(4, buffer,
                    [&func2_called, &buffer](std::size_t const subobject_fixed_offset) {
                        func2_called = true;
                        test_assert(subobject_fixed_offset == 100);

                        bool func3_called = false;
                        std::size_t const new_fixed_offset = push_fixed_subobject<mock_serialisable<2>>(
                            subobject_fixed_offset,
                            [&func3_called, &buffer](std::size_t const subobject_fixed_offset) {
                                func3_called = true;
                                test_assert(subobject_fixed_offset == 100);
                            });
                        test_assert(func3_called);

                        test_assert(new_fixed_offset == 102);
                    });
                test_assert(func2_called);

                test_assert(buffer.span().size() == 116);
            });
        test_assert(func1_called);

        test_assert(new_fixed_offset == 18);
        test_assert(buffer.span().size() == 116);
    };


    test_case("auto_deserialise() disabled") = [] {
        std::array<std::byte, 100> const buffer{};
        deserialiser<mock_serialisable<23, true, false>> const deser{const_bytes_span{buffer}, 0};
        deserialiser<mock_serialisable<23, true, false>> const result = auto_deserialise(deser);
        test_assert(result == deser);
    };

    test_case("auto_deserialise() enabled") = [] {
        std::array<std::byte, 100> const buffer{};
        deserialiser<mock_serialisable<23, true, true>> const deser{const_bytes_span{buffer}, 0};
        std::size_t const result = auto_deserialise(deser);
        test_assert(result == 46);
    };


    test_case("serialise()/3 variable_size_serialisable") = [] {
        basic_buffer buffer;
        buffer.initialise(100000);
        serialise_source<mock_serialisable<673, true>> const source;
        serialise(source, buffer, 76854);

        test_assert(source.buffers == std::vector{&buffer});
        test_assert(source.fixed_offsets == std::vector<std::size_t>{76854});
    };

    test_case("serialise()/3 fixed_size_serialisable buffer arg") = [] {
        basic_buffer buffer;
        buffer.initialise(1000);
        serialise_source<mock_serialisable<94, false>> const source;
        serialise(source, buffer, 547);

        test_assert(source.buffer_spans.size() == 1);
        test_assert(bytes_span_same(source.buffer_spans.at(0), buffer.span()));
        test_assert(source.fixed_offsets == std::vector<std::size_t>{547});
    };

    test_case("serialise()/3 fixed_size_serialisable span arg") = [] {
        basic_buffer buffer;
        buffer.initialise(1000);
        serialise_source<mock_serialisable<87, false>> const source;
        serialise(source, buffer.span(), 547);

        test_assert(source.buffer_spans.size() == 1);
        test_assert(bytes_span_same(source.buffer_spans.at(0), buffer.span()));
        test_assert(source.fixed_offsets == std::vector<std::size_t>{547});
    };

    test_case("serialise()/2") = [] {
        basic_buffer buffer;
        serialise_source<mock_serialisable<5>> const source;
        serialise(source, buffer);

        test_assert(source.buffers == std::vector{&buffer});
        test_assert(source.fixed_offsets == std::vector<std::size_t>{0});
    };


    test_case("deserialise()/2") = [] {
        std::array<std::byte, 20> const buffer{};
        deserialiser<mock_serialisable<7>> const deser = deserialise<mock_serialisable<7>>(const_bytes_span{buffer}, 6);
        test_assert(bytes_span_same(deser._buffer, const_bytes_span{buffer}));
        test_assert(deser._fixed_offset == 6);
    };

    test_case("deserialise()/2 buffer too small") = [] {
        std::array<std::byte, 10> const buffer{};
        test_assert_throws<buffer_bounds_error>([&buffer] {
            (void)deserialise<mock_serialisable<5>>(const_bytes_span{buffer}, 6);
        });
    };

    test_case("deserialise()/1") = [] {
        std::array<std::byte, 10> const buffer{};
        deserialiser<mock_serialisable<7>> const deser = deserialise<mock_serialisable<7>>(const_bytes_span{buffer});
        test_assert(bytes_span_same(deser._buffer, const_bytes_span{buffer}));
        test_assert(deser._fixed_offset == 0);
    };

    test_case("deserialise()/1 buffer too small") = [] {
        std::array<std::byte, 10> const buffer{};
        test_assert_throws<buffer_bounds_error>([&buffer] {
            (void)deserialise<mock_serialisable<11>>(const_bytes_span{buffer});
        });
    };

};
}

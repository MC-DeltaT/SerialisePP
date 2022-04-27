#include <cstddef>

#include <serialpp/buffer.hpp>
#include <serialpp/common.hpp>

#include "helpers/test.hpp"


namespace serialpp::test {
test_block buffer_tests = [] {

    static_assert(serialise_buffer<basic_serialise_buffer<>>);

    test_case("basic_serialise_buffer construct") = [] {
        basic_serialise_buffer buffer;
        mutable_bytes_span const span = buffer.span();
        test_assert(span.empty());
        test_assert(buffer.container().data() == span.data());
        test_assert(buffer.container().size() == span.size());
    };

    test_case("basic_serialise_buffer initialise()") = [] {
        basic_serialise_buffer buffer;
        buffer.initialise(4561);
        mutable_bytes_span const span = buffer.span();
        test_assert(span.size() == 4561);
        test_assert(buffer.container().data() == span.data());
        test_assert(buffer.container().size() == span.size());
    };

    test_case("basic_serialise_buffer extend()") = [] {
        basic_serialise_buffer buffer;

        buffer.initialise(100);
        mutable_bytes_span span = buffer.span();
        test_assert(span.size() == 100);

        span[0] = std::byte{10};
        span[99] = std::byte{42};
        buffer.extend(100);
        span = buffer.span();
        test_assert(span.size() == 200);
        test_assert(span[0] == std::byte{10});
        test_assert(span[99] == std::byte{42});
        test_assert(buffer.container().data() == span.data());
        test_assert(buffer.container().size() == span.size());
    };

};
}

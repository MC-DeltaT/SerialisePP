#include <cstddef>
#include <utility>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>

#include "helpers/buffer_utility.hpp"
#include "helpers/test.hpp"


namespace serialpp::test {
test_block buffers_tests = [] {

    static_assert(serialise_buffer<basic_buffer>);

    test_case("basic_buffer default construct") = [] {
        basic_buffer buffer;
        test_assert(buffer.capacity() >= 256);
        test_assert(buffer.span().empty());
    };

    test_case("basic_buffer construct") = [] {
        basic_buffer buffer{6493};
        test_assert(buffer.capacity() == 6493);
        test_assert(buffer.span().empty());
    };

    test_case("basic_buffer initialise() within capacity") = [] {
        basic_buffer buffer{100};
        std::size_t const capacity = buffer.capacity();
        std::byte const* const data = buffer.span().data();
        buffer.initialise(100);
        test_assert(buffer.capacity() == capacity);
        test_assert(buffer.span().data() == data);
    };

    test_case("basic_buffer initialise() exceed capacity") = [] {
        basic_buffer buffer{100};
        buffer.initialise(200);
        test_assert(buffer.capacity() >= 200);
        test_assert(buffer.span().size() == 200);
    };

    test_case("basic_buffer initialise() zero size") = [] {
        basic_buffer buffer;
        std::size_t const capacity = buffer.capacity();
        mutable_bytes_span const span1 = buffer.span();
        buffer.initialise(0);

        test_assert(buffer.capacity() == capacity);
        mutable_bytes_span const span2 = buffer.span();
        test_assert(bytes_span_same(span1, span2));
        test_assert(span2.empty());
    };

    test_case("basic_buffer extend() within capacity") = [] {
        basic_buffer buffer{150};
        buffer.initialise(100);
        std::size_t const capacity = buffer.capacity();
        mutable_bytes_span const span1 = buffer.span();
        test_assert(span1.size() == 100);
        span1[0] = std::byte{10};
        span1[99] = std::byte{42};
        buffer.extend(50);

        test_assert(buffer.capacity() == capacity);
        mutable_bytes_span const span2 = buffer.span();
        test_assert(span1.data() == span2.data());
        test_assert(span2.size() == 150);
        test_assert(span2[0] == std::byte{10});
        test_assert(span2[99] == std::byte{42});
    };

    test_case("basic_buffer extend() exceed capacity") = [] {
        basic_buffer buffer{150};
        buffer.initialise(100);
        mutable_bytes_span const span1 = buffer.span();
        test_assert(span1.size() == 100);
        span1[0] = std::byte{10};
        span1[99] = std::byte{42};
        buffer.extend(200);

        test_assert(buffer.capacity() >= 300);
        mutable_bytes_span const span2 = buffer.span();
        test_assert(span2.size() == 300);
        test_assert(span2[0] == std::byte{10});
        test_assert(span2[99] == std::byte{42});
    };

    test_case("basic_buffer extend() zero size") = [] {
        basic_buffer buffer;
        buffer.initialise(100);
        std::size_t const capacity = buffer.capacity();
        mutable_bytes_span const span1 = buffer.span();
        test_assert(span1.size() == 100);
        span1[0] = std::byte{10};
        span1[99] = std::byte{42};
        buffer.extend(0);

        test_assert(buffer.capacity() == capacity);
        mutable_bytes_span const span2 = buffer.span();
        test_assert(bytes_span_same(span1, span2));
        test_assert(span2[0] == std::byte{10});
        test_assert(span2[99] == std::byte{42});
    };

    test_case("basic_buffer move construct") = [] {
        basic_buffer buffer1;
        buffer1.initialise(100);
        mutable_bytes_span const span1 = buffer1.span();
        span1[0] = std::byte{10};
        span1[99] = std::byte{42};
        basic_buffer buffer2{std::move(buffer1)};

        mutable_bytes_span const span2 = buffer2.span();
        test_assert(bytes_span_same(span1, span2));
        test_assert(span2[0] == std::byte{10});
        test_assert(span2[99] == std::byte{42});
    };

    test_case("basic_buffer move assign") = [] {
        basic_buffer buffer1;
        buffer1.initialise(100);
        mutable_bytes_span const span1 = buffer1.span();
        span1[0] = std::byte{10};
        span1[99] = std::byte{42};
        basic_buffer buffer2;
        buffer2.initialise(1500);
        mutable_bytes_span span2 = buffer2.span();
        span2[0] = std::byte{1};
        span2[99] = std::byte{1};

        buffer2 = std::move(buffer1);
        span2 = buffer2.span();
        test_assert(bytes_span_same(span1, span2));
        test_assert(span2[0] == std::byte{10});
        test_assert(span2[99] == std::byte{42});
    };

};
}

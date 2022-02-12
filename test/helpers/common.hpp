#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include <serialpp/common.hpp>
#include <serialpp/utility.hpp>


namespace serialpp::test {

    template<std::size_t FixedSize>
    struct MockSerialisable {};


    template<std::size_t N>
    bool buffer_equal(SerialiseBuffer const& buffer, std::array<unsigned char, N> const& expected) {
        return std::ranges::equal(buffer.span(), std::as_bytes(std::span{expected}));
    }


    // Checks if two ConstBytesView view the same location and size.
    inline bool bytes_view_same(ConstBytesView v1, ConstBytesView v2) {
        return v1.data() == v2.data() && v1.size() == v2.size();
    }

}

namespace serialpp {

    template<std::size_t FixedSize>
    struct FixedDataSize<test::MockSerialisable<FixedSize>> : SizeTConstant<FixedSize> {};

    template<std::size_t FixedSize>
    struct SerialiseSource<test::MockSerialisable<FixedSize>> {
        int tag = 0;

        constexpr friend auto operator<=>(SerialiseSource const&, SerialiseSource const&) = default;
    };

    template<std::size_t FixedSize>
    struct Serialiser<test::MockSerialisable<FixedSize>> {
        SerialiseTarget operator()(SerialiseSource<test::MockSerialisable<FixedSize>> source,
                SerialiseTarget target) const {
            this->source = source;
            this->target = target;
            return target;
        }

        static inline SerialiseSource<test::MockSerialisable<FixedSize>> source;
        static inline std::optional<SerialiseTarget> target;
    };

    template<std::size_t FixedSize>
    struct Deserialiser<test::MockSerialisable<FixedSize>> : DeserialiserBase {
        using DeserialiserBase::DeserialiserBase;
    };

    template<std::size_t FixedSize>
    constexpr bool operator==(Deserialiser<test::MockSerialisable<FixedSize>> const& lhs,
            Deserialiser<test::MockSerialisable<FixedSize>> const& rhs) {
        return test::bytes_view_same(lhs.fixed_data, rhs.fixed_data)
            && test::bytes_view_same(lhs.variable_data, rhs.variable_data);
    }


    static_assert(Serialisable<test::MockSerialisable<10>>);

}

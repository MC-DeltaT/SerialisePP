#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <serialpp/common.hpp>
#include <serialpp/utility.hpp>


namespace serialpp::test {

    template<std::size_t FixedSize>
    struct MockSerialisable {};


    template<std::size_t N>
    [[nodiscard]]
    bool buffer_equal(SerialiseBuffer const& buffer, std::array<unsigned char, N> const& expected) {
        return std::ranges::equal(buffer.span(), std::as_bytes(std::span{expected}));
    }


    // Checks if two ConstBytesView view the same memory location and have the same size.
    [[nodiscard]]
    inline bool bytes_view_same(ConstBytesView v1, ConstBytesView v2) {
        return v1.data() == v2.data() && v1.size() == v2.size();
    }


    template<class R>
    [[nodiscard]]
    ConstBytesView as_const_bytes_view(R const& range) {
        return std::as_bytes(std::span{range});
    }

}

namespace serialpp {

    template<std::size_t FixedSize>
    struct FixedDataSize<test::MockSerialisable<FixedSize>> : SizeTConstant<FixedSize> {};

    template<std::size_t FixedSize>
    struct SerialiseSource<test::MockSerialisable<FixedSize>> {
        mutable std::vector<SerialiseTarget> targets;
    };

    template<std::size_t FixedSize>
    struct Serialiser<test::MockSerialisable<FixedSize>> {
        SerialiseTarget operator()(SerialiseSource<test::MockSerialisable<FixedSize>> const& source,
                SerialiseTarget target) const {
            source.targets.push_back(target);
            return target;
        }
    };

    template<std::size_t FixedSize>
    class Deserialiser<test::MockSerialisable<FixedSize>> : public DeserialiserBase<test::MockSerialisable<FixedSize>> {
    public:
        using DeserialiserBase<test::MockSerialisable<FixedSize>>::DeserialiserBase;

        using DeserialiserBase<test::MockSerialisable<FixedSize>>::_fixed_data;
        using DeserialiserBase<test::MockSerialisable<FixedSize>>::_variable_data;
    };

    template<std::size_t FixedSize>
    [[nodiscard]]
    constexpr bool operator==(Deserialiser<test::MockSerialisable<FixedSize>> const& lhs,
            Deserialiser<test::MockSerialisable<FixedSize>> const& rhs) {
        return test::bytes_view_same(lhs._fixed_data, rhs._fixed_data)
            && test::bytes_view_same(lhs._variable_data, rhs._variable_data);
    }


    static_assert(Serialisable<test::MockSerialisable<10>>);

}

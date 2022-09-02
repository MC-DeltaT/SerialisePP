#pragma once

#include <cstddef>
#include <vector>

#include <serialpp/buffers.hpp>
#include <serialpp/common.hpp>

#include "buffer_utility.hpp"


namespace serialpp::test {

    template<std::size_t FixedSize, bool Variable = true, bool AutoDeserialise = false>
    struct mock_serialisable {};

}

namespace serialpp {

    template<std::size_t FixedSize, bool Variable, bool AutoDeserialise>
    struct fixed_data_size<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>> {
        static constexpr std::size_t value = FixedSize;
    };

    template<std::size_t FixedSize, bool Variable, bool AutoDeserialise>
    struct serialise_source<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>> {
        mutable std::vector<basic_buffer*> buffers;
        mutable std::vector<mutable_bytes_span> buffer_spans;
        mutable std::vector<std::size_t> fixed_offsets;
    };

    template<std::size_t FixedSize, bool Variable, bool AutoDeserialise>
    struct serialiser<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>> {
        static constexpr void serialise(
                serialise_source<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>> const& source,
                serialise_buffer auto& buffer, std::size_t fixed_offset) {
            source.buffers.push_back(&buffer);
            source.fixed_offsets.push_back(fixed_offset);
        }

        static constexpr void serialise(
                serialise_source<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>> const& source,
                mutable_bytes_span buffer, std::size_t fixed_offset) requires (!Variable) {
            source.buffer_spans.push_back(buffer);
            source.fixed_offsets.push_back(fixed_offset);
        }
    };

    template<std::size_t FixedSize, bool Variable, bool AutoDeserialise>
    class deserialiser<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        using deserialiser_base::_buffer;
        using deserialiser_base::_fixed_offset;

        constexpr std::size_t value() const requires AutoDeserialise {
            return FixedSize * 2;
        }
    };

    template<std::size_t FixedSize, bool Variable, bool AutoDeserialise>
    [[nodiscard]]
    constexpr bool operator==(deserialiser<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>> const& lhs,
            deserialiser<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>> const& rhs) {
        return test::bytes_span_same(lhs._buffer, rhs._buffer) && lhs._fixed_offset == rhs._fixed_offset;
    }

    template<std::size_t FixedSize, bool Variable, bool AutoDeserialise>
    inline constexpr bool enable_auto_deserialise<test::mock_serialisable<FixedSize, Variable, AutoDeserialise>>
        = AutoDeserialise;

}

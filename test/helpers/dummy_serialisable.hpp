#pragma once

#include <cstddef>

#include <serialpp/common.hpp>
#include <serialpp/utility.hpp>


namespace serialpp::test {

    template<std::size_t FixedSize>
    struct DummySerialisable {};

}

namespace serialpp {

    template<std::size_t FixedSize>
    struct FixedDataSize<test::DummySerialisable<FixedSize>> : SizeTConstant<FixedSize> {};

    template<std::size_t FixedSize>
    struct SerialiseSource<test::DummySerialisable<FixedSize>> {};

    template<std::size_t FixedSize>
    struct Serialiser<test::DummySerialisable<FixedSize>> {
        SerialiseTarget operator()(SerialiseSource<test::DummySerialisable<FixedSize>>, SerialiseTarget) const;
    };

    template<std::size_t FixedSize>
    struct Deserialiser<test::DummySerialisable<FixedSize>> : DeserialiserBase {
        using DeserialiserBase::DeserialiserBase;
    };


    static_assert(Serialisable<test::DummySerialisable<10>>);

}

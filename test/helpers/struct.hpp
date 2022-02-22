#pragma once

#include <cstdint>

#include <serialpp/scalar.hpp>
#include <serialpp/struct.hpp>


namespace serialpp::test {

    struct BasicTestStruct : SerialisableStruct<
        Field<"a", std::int8_t>,
        Field<"foo", std::uint32_t>,
        Field<"my field", std::int16_t>,
        Field<"qux", std::uint64_t>
    > {};

}

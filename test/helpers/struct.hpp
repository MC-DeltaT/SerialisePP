#pragma once

#include <cstdint>

#include <serialpp/scalar.hpp>
#include <serialpp/struct.hpp>

#include "common.hpp"


namespace serialpp::test {

    struct BasicTestStruct : SerialisableStruct<
        Field<"a", std::int8_t>,
        Field<"foo", std::uint32_t>,
        Field<"my field", std::int16_t>,
        Field<"qux", std::uint64_t>
    > {};


    struct MockTestStruct : SerialisableStruct<
        Field<"magic", MockSerialisable<20>>,
        Field<"foo", MockSerialisable<10>>,
        Field<"field", MockSerialisable<5>>,
        Field<"qux", MockSerialisable<11>>,
        Field<"anotherone", MockSerialisable<2>>
    > {};

}

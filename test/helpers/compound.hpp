#pragma once

#include <cstdint>

#include <serialpp/common.hpp>
#include <serialpp/list.hpp>
#include <serialpp/optional.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/struct.hpp>


namespace serialpp::test {

    struct CompoundTestStruct1 : SerialisableStruct<
        Field<"list_u16", List<std::uint16_t>>,
        Field<"u8", std::uint8_t>,
        Field<"opt_i64", Optional<std::int64_t>>
    > {};

    struct CompoundTestStruct2 : SerialisableStruct<
        Field<"struct1", CompoundTestStruct1>,
        Field<"opt_struct1", Optional<CompoundTestStruct1>>,
        Field<"list_struct1", List<CompoundTestStruct1>>
    > {};

    struct CompoundTestStruct3 : SerialisableStruct<
        Field<"struct2", CompoundTestStruct2>,
        Field<"struct1", CompoundTestStruct1>
    > {};

}

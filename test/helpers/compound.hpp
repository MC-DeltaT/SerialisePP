#pragma once

#include <cstdint>

#include <serialpp/array.hpp>
#include <serialpp/common.hpp>
#include <serialpp/list.hpp>
#include <serialpp/optional.hpp>
#include <serialpp/pair.hpp>
#include <serialpp/scalar.hpp>
#include <serialpp/struct.hpp>
#include <serialpp/tuple.hpp>
#include <serialpp/variant.hpp>


namespace serialpp::test {

    struct CompoundTestStruct1 : SerialisableStruct<
        Field<"list_u16", List<std::uint16_t>>,
        Field<"u8", std::uint8_t>,
        Field<"opt_i64", Optional<std::int64_t>>
    > {};

    struct CompoundTestStruct2 : SerialisableStruct<
        Field<"struct1", CompoundTestStruct1>,
        Field<"opt_struct1", Optional<CompoundTestStruct1>>,
        Field<"list_struct1", List<CompoundTestStruct1>>,
        Field<"array_struct1", Array<CompoundTestStruct1, 2>>
    > {};

    struct CompoundTestStruct3 : SerialisableStruct<
        Field<"tuple", Tuple<
            Pair<CompoundTestStruct2, CompoundTestStruct1>, Variant<CompoundTestStruct2, CompoundTestStruct1>>>
    > {};

}

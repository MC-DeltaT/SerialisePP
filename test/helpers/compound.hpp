#pragma once

#include <cstdint>

#include <serialpp/serialpp.hpp>


namespace serialpp::test {

    struct compound_test_record1 : record<
        field<"darray_u16", dynamic_array<std::uint16_t>>,
        field<"u8", std::uint8_t>,
        field<"opt_i64", optional<std::int64_t>>
    > {};

    struct compound_test_record2 : record<
        field<"record1", compound_test_record1>,
        field<"opt_record1", optional<compound_test_record1>>,
        field<"darray_record1", dynamic_array<compound_test_record1>>,
        field<"sarray_record1", static_array<compound_test_record1, 2>>
    > {};

    struct compound_test_record3 : record<
        field<"tuple", tuple<
            pair<compound_test_record2, compound_test_record1>,
            variant<compound_test_record2, compound_test_record1>>>
    > {};

    struct constexpr_test_record : record<
        field<"i32", std::int32_t>,
        field<"pair_u16_i8", pair<std::uint16_t, std::int8_t>>,
        field<"tuple_u8_u16_u32", tuple<std::uint8_t, std::uint16_t, std::uint32_t>>,
        field<"sarray_u8", static_array<std::uint8_t, 4>>,
        field<"opt_i32", optional<std::int32_t>>,
        field<"var_u64_i8", variant<std::uint64_t, std::int8_t>>
    > {};

}

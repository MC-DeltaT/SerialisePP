#pragma once

#include <cstdint>

#include <serialpp/record.hpp>
#include <serialpp/scalar.hpp>

#include "common.hpp"


namespace serialpp::test {

    struct empty_test_record : record<> {};


    struct basic_test_record : record<
        field<"a", std::int8_t>,
        field<"foo", std::uint32_t>,
        field<"my field", std::int16_t>,
        field<"qux", std::uint64_t>
    > {};


    struct mock_test_record : record<
        field<"magic", mock_serialisable<20>>,
        field<"foo", mock_serialisable<10>>,
        field<"field", mock_serialisable<5>>
    > {};


    struct mock_derived_test_record : record<
        base<mock_test_record>,
        field<"qux", mock_serialisable<11>>,
        field<"anotherone", mock_serialisable<2>>
    > {};


    struct derived_empty_test_record : record<
        base<empty_test_record>
    > {};


    struct derived_test_record : record<
        base<basic_test_record>,
        field<"extra1", std::uint16_t>,
        field<"extra2", std::uint8_t>
    > {};


    struct more_derived_test_record : record<
        base<derived_test_record>,
        field<"really extra", std::uint32_t>
    > {};

}

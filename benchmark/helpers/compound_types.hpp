#pragma once

#include <cstdint>

#include <serialpp/serialpp.hpp>


namespace serialpp::benchmark {

	struct simple_record : record<
		field<"f32", float>,
		field<"u64", std::uint64_t>,
		field<"u32", std::uint32_t>
	> {};

	struct intermediate_record : record<
		field<"id", static_array<std::uint8_t, 16>>,
		field<"items", dynamic_array<simple_record>>,
		field<"m1", float>,
		field<"m2", std::int64_t>
	> {};

	struct complex_record : record<
		field<"o", optional<simple_record>>,
		field<"d", double>,
		field<"l", dynamic_array<intermediate_record>>,
		field<"v", variant<simple_record, intermediate_record>>
	> {};

}

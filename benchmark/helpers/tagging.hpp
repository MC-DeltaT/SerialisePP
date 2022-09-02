#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <serialpp/serialpp.hpp>


namespace serialpp::benchmark {

	template<serialisable T>
	inline constexpr bool is_pair = false;

	template<serialisable T1, serialisable T2>
	inline constexpr bool is_pair<pair<T1, T2>> = true;


	template<serialisable T>
	inline constexpr bool is_tuple = false;

	template<serialisable... Ts>
	inline constexpr bool is_tuple<tuple<Ts...>> = true;


	template<serialisable T>
	inline constexpr bool is_static_array = false;

	template<serialisable T, std::size_t Size>
	inline constexpr bool is_static_array<static_array<T, Size>> = true;


	template<serialisable T>
	inline constexpr bool is_optional = false;

	template<serialisable T>
	inline constexpr bool is_optional<optional<T>> = true;


	template<serialisable T>
	inline constexpr bool is_dynamic_array = false;

	template<serialisable T>
	inline constexpr bool is_dynamic_array<dynamic_array<T>> = true;


	template<serialisable T>
	inline constexpr bool is_variant = false;

	template<serialisable... Ts>
	inline constexpr bool is_variant<variant<Ts...>> = true;


	template<serialisable T>
	void add_tags_for_type(std::vector<std::string>& tags) {
		if constexpr (scalar<T>) {
			tags.emplace_back("scalar");
		}
		else if constexpr (is_pair<T>) {
			tags.emplace_back("pair");
		}
		else if constexpr (is_tuple<T>) {
			tags.emplace_back("tuple");
		}
		else if constexpr (is_static_array<T>) {
			tags.emplace_back("static_array");
		}
		else if constexpr (record_type<T>) {
			tags.emplace_back("record");
		}
		else if constexpr (is_optional<T>) {
			tags.emplace_back("optional");
		}
		else if constexpr (is_dynamic_array<T>) {
			tags.emplace_back("dynamic_array");
		}
		else if constexpr (is_variant<T>) {
			tags.emplace_back("variant");
		}

		if constexpr (variable_size_serialisable<T>) {
			tags.emplace_back("variable-size");
		}
		else if constexpr (is_pair<T> || is_tuple<T> || record_type<T> || is_static_array<T>) {
			tags.emplace_back("simple_compound");
		}
	}

}

#pragma once

#include <ranges>
#include <string>


namespace serialpp::benchmark {

	// Wraps a range and turns it into a "large" range for the purposes of dynamic_array's serialise_source.
	template<std::ranges::range R>
	struct large_range_wrapper {
		R range;
		char padding[32];

		constexpr auto begin() {
			return std::ranges::begin(range);
		}

		constexpr auto begin() const {
			return std::ranges::begin(range);
		}

		constexpr auto end() {
			return std::ranges::end(range);
		}

		constexpr auto end() const {
			return std::ranges::end(range);
		}

		constexpr auto size() requires std::ranges::sized_range<R> {
			return std::ranges::size(range);
		}

		constexpr auto size() const requires std::ranges::sized_range<R> {
			return std::ranges::size(range);
		}
	};


	// Gets the current local time as an ISO 8601 timestamp suitable for use in a file name.
	std::string get_timestamp();


	// Sets the process to run with high priority on a single processor, if possible.
	// Return value indicates if the operation was successful or not.
	bool pin_process();

}

#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "benchmark.hpp"
#include "utility.hpp"


namespace serialpp::benchmark {

	inline void display_config(std::vector<std::string> const& tag_filters, fp_seconds const test_time,
			std::size_t const batch_size, std::uint64_t const random_seed, std::string_view const csv_filename,
			bool const process_pinned, std::ostream& stream = std::cout) {
		stream << "Tag filters:";
		if (tag_filters.empty()) {
			stream << " <none>";
		}
		else {
			for (auto const& tag : tag_filters) {
				stream << std::format(" \"{}\"", tag);
			}
		}
		stream << '\n';
		stream << std::format("Test time: {} s\n", test_time.count());
		stream << std::format("Batch size: {} B\n", batch_size);
		stream << std::format("Random seed: {}\n", random_seed);
		stream << std::format("Process pinned: {}\n", process_pinned ? "yes" : "no");
		stream << std::format("CSV file: {}\n", csv_filename);
		stream << std::endl;
	}


	inline void display_results(std::ostream& stream, benchmark_result const& result,
			std::string_view const sample_unit) {
		auto const sec_per_sample = result.time.count() / result.samples;
		stream << std::format("  {0:.2e} s/{2} | {1:.2e} {2}/s\n", sec_per_sample, 1 / sec_per_sample, sample_unit);
		if (result.data_processed != 0) {
			auto const bytes_per_sample = result.data_processed / result.samples;
			auto const sec_per_byte = sec_per_sample / bytes_per_sample;
			stream << std::format("  {:.2e} s/B   | {:.2e} B/s   | {:.4} B/{}\n",
				sec_per_byte, 1 / sec_per_byte, bytes_per_sample, sample_unit);
		}
		stream << std::endl;
	}


	inline std::string make_csv_filename() {
		return std::format("serialisepp_benchmark-{}.csv", get_timestamp());
	}


	inline std::string csv_escape(std::string value) {
		bool quote = false;
		for (std::size_t i = 0; i < value.size(); ++i) {
			switch (value[i]) {
			case '"':
				value.insert(value.cbegin() + i, '"');
				++i;
				[[fallthrough]];
			case ',':
			case '\n':
			case '\r':
				quote = true;
				break;
			}
		}
		if (quote) {
			value.insert(value.cbegin(), '"');
			value.push_back('"');
		}
		return value;
	}


	inline void write_csv_header(std::ostream& stream) {
		stream << "name,time,samples,data_processed" << std::endl;
	}

	inline void write_csv_results(std::ostream& stream, std::string const benchmark_name,
			benchmark_result const& result) {
		stream
			<< std::format("{},{},{},{}",
				csv_escape(benchmark_name), result.time.count(), result.samples, result.data_processed)
			<< std::endl;
	}

}

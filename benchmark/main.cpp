#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ostream>
#include <random>
#include <string>
#include <vector>

#include <serialpp/serialpp.hpp>

#include "benchmarks.hpp"
#include "helpers/benchmark.hpp"
#include "helpers/output.hpp"
#include "helpers/utility.hpp"


namespace serialpp::benchmark {

	[[nodiscard]]
	inline bool filter_benchmark(benchmark_t const& benchmark, std::vector<std::string> const& tag_filter) {
		// Benchmark must have all the tags in the filter to be run.
		for (auto const& tag : tag_filter) {
			if (std::ranges::find(benchmark.metadata.tags, tag) == benchmark.metadata.tags.cend()) {
				return false;
			}
		}
		return true;
	}


	inline void run_benchmarks(benchmark_fixture& fixture, std::vector<benchmark_t> const& benchmarks,
			std::vector<std::string> const& tag_filter, std::ostream& csv_output,
			std::ostream& human_output = std::cout) {
		for (auto const& benchmark : benchmarks) {
			if (filter_benchmark(benchmark, tag_filter)) {
				human_output << benchmark.name << std::endl;
				auto const result = benchmark.function(fixture);
				display_results(human_output, result, benchmark.metadata.sample_unit);
				write_csv_results(csv_output, benchmark.name, result);
			}
		}
	}

}


int main(int argc, char** argv) {
	using namespace serialpp::benchmark;

	register_all_benchmarks();

	std::vector<std::string> tag_filters;
	for (int i = 1; i < argc; ++i) {
		tag_filters.emplace_back(argv[i]);
	}

#if NDEBUG || !(DEBUG || _DEBUG)
	constexpr fp_seconds test_time{5.0};
	constexpr std::size_t batch_size = 10'000'000;
	constexpr bool pin = true;
#else
	constexpr fp_seconds test_time{0.5};
	constexpr std::size_t batch_size = 100'000;
	constexpr bool pin = false;
#endif

	auto const random_seed = static_cast<std::uint64_t>(std::random_device{}());

	auto const csv_filename = make_csv_filename();

	auto const process_pinned = pin && pin_process();

	display_config(tag_filters, test_time, batch_size, random_seed, csv_filename, process_pinned);

	std::ofstream csv_file{csv_filename};
	if (!csv_file) {
		std::cerr << "Failed to open CSV output file" << std::endl;
		return EXIT_FAILURE;
	}
	write_csv_header(csv_file);

	benchmark_fixture fixture{test_time, batch_size, random_seed};
	auto benchmarks = registered_benchmarks();
	std::ranges::shuffle(benchmarks, fixture.random.get<0>());
	run_benchmarks(fixture, benchmarks, tag_filters, csv_file);
}

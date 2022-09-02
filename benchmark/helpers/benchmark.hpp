#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "random.hpp"


namespace serialpp::benchmark {

	using fp_seconds = std::chrono::duration<double>;


	struct benchmark_fixture {
		fp_seconds const test_time;		// Amount of time to run a single benchmark.
		std::size_t const batch_size;	// Number of bytes of data to process in a single batch of benchmark samples.
		random_state random;

		explicit constexpr benchmark_fixture(fp_seconds const test_time, std::size_t const batch_size,
				std::uint64_t const random_seed) :
			test_time{test_time},
			batch_size{batch_size},
			random{random_seed}
		{}

		[[nodiscard]]
		std::size_t get_samples_per_batch(double const bytes_per_sample) const {
			if (bytes_per_sample <= 0) {
				return batch_size;
			}
			else {
				return std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(batch_size / bytes_per_sample)));
			}
		}
	};


	template<std::invocable<> F1, std::invocable<std::size_t> F2>
	std::pair<fp_seconds, std::size_t> benchmark_impl(F1&& batch_setup, F2&& sample_func,
			std::size_t const samples_per_batch, fp_seconds const test_time) {
		std::size_t samples = 0;
		fp_seconds time{0};
		do {
			std::forward<F1>(batch_setup)();

			auto const t1 = std::chrono::steady_clock::now();
			for (std::size_t i = 0; i < samples_per_batch; ++i) {
				std::forward<F2>(sample_func)(std::size_t{i});
			}
			auto const t2 = std::chrono::steady_clock::now();

			samples += samples_per_batch;
			time += std::chrono::duration_cast<decltype(time)>(t2 - t1);
		} while (time < test_time);

		return {time, samples};
	}


	struct benchmark_result {
		fp_seconds time;			// Total time to process all samples.
		std::size_t samples;		// Total number of samples processed.
		double data_processed;		// Total number of bytes of useful data processed (can be 0).
	};


	struct benchmark_metadata {
		std::vector<std::string> tags;
		std::string sample_unit;		// Name of unit of a sample (e.g. "serialise").
	};


	struct benchmark_t {
		std::string name;
		benchmark_metadata metadata;
		std::function<benchmark_result(benchmark_fixture&)> function;
	};


	[[nodiscard]]
	inline std::vector<benchmark_t>& registered_benchmarks() {
		static std::vector<benchmark_t> benchmarks;
		return benchmarks;
	}


	inline void register_benchmark(benchmark_t benchmark) {
		registered_benchmarks().push_back(std::move(benchmark));
	}

}

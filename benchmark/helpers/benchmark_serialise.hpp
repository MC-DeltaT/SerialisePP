#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <serialpp/serialpp.hpp>

#include "benchmark.hpp"
#include "buffers.hpp"
#include "optimisation.hpp"
#include "random.hpp"
#include "random_source_generator.hpp"
#include "tagging.hpp"
#include "utility.hpp"


namespace serialpp::benchmark {

	template<serialisable T>
	benchmark_result benchmark_serialise_impl(benchmark_fixture& fixture, double const est_avg_data_size,
			std::size_t const max_serialised_size, auto&& batch_setup, auto&& source_generator) {
		auto const samples_per_batch = fixture.get_samples_per_batch(est_avg_data_size);

		double data_processed = 0;
		auto const batch_setup_wrapper = [&batch_setup, samples_per_batch, &data_processed] {
			data_processed += batch_setup(samples_per_batch);
		};

		preallocated_buffer buffer{max_serialised_size};
		make_side_effect(buffer.allocated_storage().data());

		auto const sample_func = [&source_generator, &buffer](std::size_t const i) {
			serialise_source<T> const& source = source_generator(i);
			serialise(source, buffer);
			// Repeated serialisations are dead stores.
			memory_fence();
		};

		auto const [time, samples] =
			benchmark_impl(batch_setup_wrapper, sample_func, samples_per_batch, fixture.test_time);

		return {time, samples, data_processed};
	}


	template<serialisable T>
	benchmark_result benchmark_serialise_only_impl(benchmark_fixture& fixture, auto source_generator) {
		std::vector<serialise_source<T>> sources;
		auto const batch_setup = [&fixture, &sources, &source_generator](std::size_t const samples) {
			double data_written = 0;
			random_vector_generate<fixed_size_serialisable<T>>(fixture.random, sources, samples,
				[&source_generator, &data_written](auto& random_engine) {
					return source_generator(random_engine, data_written);
				});
			return data_written;
		};
		auto const inner_source_generator = [&sources](std::size_t const i) -> serialise_source<T> const& {
			return sources[i];
		};

		return benchmark_serialise_impl<T>(fixture,
			source_generator.average_data_size(), source_generator.max_serialised_size(),
			batch_setup, inner_source_generator);
	}

	// Benchmarks serialise(source, buffer).
	// That is, this benchmark includes:
	//   - Initialising the buffer (excluding memory allocation)
	//   - Serialising the serialise_source
	// And does NOT include:
	//   - Buffer allocation (it's preallocated and never reallocates)
	//   - Constructing the serialise_source
	//   - Reading/using the buffer afterwards
	template<serialisable T>
	benchmark_t benchmark_serialise_only(std::string const name, random_source_generator<T> source_generator = {},
			std::vector<std::string> tags = {}) {
		tags.emplace_back("serialise");
		tags.emplace_back("serialise-only");
		add_tags_for_type<T>(tags);
		return {
			"serialise() only: " + name,
			{tags, "Ser"},
			[source_generator](benchmark_fixture& fixture) {
				return benchmark_serialise_only_impl<T>(fixture, source_generator);
			}
		};
	}


	template<bool LargeRange>
	struct serialise_full_dynamic_array_u32_generator {
		using element_type = std::uint32_t;

		using prep_type = std::conditional_t<LargeRange,
			large_range_wrapper<std::vector<element_type>>,
			std::vector<element_type>>;

		std::size_t min_size = 0;
		std::size_t max_size = 1000;
		[[no_unique_address]]
		random_source_generator<element_type> element_generator;

		[[nodiscard]]
		constexpr double average_data_size() const {
			return (min_size / 2.0 + max_size / 2.0) * element_generator.average_data_size();
		}

		[[nodiscard]]
		constexpr std::size_t max_serialised_size() const {
			return fixed_data_size_v<dynamic_array<element_type>>
				+ max_size * element_generator.max_serialised_size();
		}

		prep_type prepare(auto& random_engine, double& data_written) const {
			assert(min_size <= max_size);
			std::uniform_int_distribution<std::size_t> size_dist{min_size, max_size};
			auto const size = size_dist(random_engine);

			std::vector<element_type> elements(size);
			std::ranges::generate(elements, [this, &random_engine, &data_written] {
				return element_generator(random_engine, data_written).value;
			});

			if constexpr (LargeRange) {
				return large_range_wrapper{std::move(elements)};
			}
			else {
				static_assert(32 >= sizeof elements);
				return elements;
			}
		}

		serialise_source<dynamic_array<element_type>> operator()(prep_type&& prep) {
			return {std::move(prep)};
		}
	};


	// Benchmarks constructing serialise_source and serialise(source, buffer).
	// That is, this benchmark includes:
	//   - Initialising the buffer (excluding memory allocation)
	//   - Constructing the serialise_source
	//   - Serialising the serialise_source
	// And does NOT include:
	//   - Buffer allocation (it's preallocated and never reallocates)
	//   - Reading/using the buffer afterwards
	template<serialisable T, class G>
	benchmark_result benchmark_serialise_full_impl(benchmark_fixture& fixture, G source_generator) {
		std::vector<typename G::prep_type> preps;
		auto const batch_setup = [&fixture, &preps, &source_generator](std::size_t const samples) {
			double data_written = 0;
			random_vector_generate(fixture.random, preps, samples,
				[&source_generator, &data_written](auto& random_engine) {
					return source_generator.prepare(random_engine, data_written);
				});
			return data_written;
		};
		auto const inner_source_generator = [&preps, &source_generator](std::size_t const i) -> decltype(auto) {
			return source_generator(std::move(preps[i]));
		};

		return benchmark_serialise_impl<T>(fixture,
			source_generator.average_data_size(), source_generator.max_serialised_size(),
			batch_setup, inner_source_generator);
	}

	template<serialisable T, class G>
	benchmark_t benchmark_serialise_full(std::string const name, G source_generator = {},
			std::vector<std::string> tags = {}) {
		tags.emplace_back("serialise");
		tags.emplace_back("serialise-full");
		add_tags_for_type<T>(tags);
		return {
			"full serialise: " + name,
			{tags, "Ser"},
			[source_generator](benchmark_fixture& fixture) {
				return benchmark_serialise_full_impl<T>(fixture, source_generator);
			}
		};
	}

}

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <serialpp/serialpp.hpp>

#include "benchmark.hpp"
#include "buffers.hpp"
#include "deserialise_consumer.hpp"
#include "optimisation.hpp"
#include "random.hpp"
#include "random_source_generator.hpp"
#include "tagging.hpp"


namespace serialpp::benchmark {

	template<serialisable T>
	benchmark_result benchmark_deserialise_impl(benchmark_fixture& fixture, auto source_generator) {
		auto const est_avg_data_size = source_generator.average_data_size();
		auto const max_serialised_size = source_generator.max_serialised_size();
		auto const samples_per_batch = fixture.get_samples_per_batch(est_avg_data_size);

		std::vector<serialise_source<T>> sources;

		sequential_buffer buffer{max_serialised_size * samples_per_batch};
		make_side_effect(buffer.allocated_storage().data());

		std::vector<const_bytes_span> object_buffers;
		object_buffers.reserve(samples_per_batch);

		double data_processed = 0;
		auto const batch_setup =
			[&fixture, &source_generator, samples_per_batch, &sources, &buffer, &object_buffers, &data_processed] {
				random_vector_generate<fixed_size_serialisable<T>>(fixture.random, sources, samples_per_batch,
					[&source_generator, &data_processed](auto& random_engine) {
						return source_generator(random_engine, data_processed);
					});

				buffer.clear();
				object_buffers.clear();
				for (auto const& source : sources) {
					serialise(source, buffer);
					object_buffers.push_back(buffer.span());
				}
				// Prevent elision of the intermediate buffer.
				memory_fence();
			};

		make_side_effect(&deserialise_consume_sink);

		auto const sample_func = [&object_buffers](std::size_t const i) {
			deserialise_consumer<T>::consume(deserialise<T>(object_buffers[i]));
			// Repeated consumptions are dead stores.
			memory_fence();
		};

		auto const [time, samples] = benchmark_impl(batch_setup, sample_func, samples_per_batch, fixture.test_time);

		return {time, samples, data_processed};
	}

	// Benchmarks deserialisation.
	// That is, this benchmark includes:
	//   - Validating the buffer size
	//   - Constructing the deserialiser
	//   - Reading data from the deserialiser
	//   - Naively writing the data to memory
	// And does NOT include:
	//   - Reading/loading the buffer from somewhere
	//   - Performing calculations on the deserialised data
	template<serialisable T>
	benchmark_t benchmark_deserialise(std::string const name, random_source_generator<T> source_generator = {},
			std::vector<std::string> tags = {}) {
		tags.emplace_back("deserialise");
		add_tags_for_type<T>(tags);
		return {
			"deserialise: " + name,
			{tags, "Deser"},
			[source_generator](benchmark_fixture& fixture) {
				return benchmark_deserialise_impl<T>(fixture, source_generator);
			}
		};
	}

}

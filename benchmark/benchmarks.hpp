#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <serialpp/serialpp.hpp>

#include "helpers/benchmark.hpp"
#include "helpers/benchmark_deserialise.hpp"
#include "helpers/benchmark_serialise.hpp"
#include "helpers/compound_types.hpp"
#include "helpers/random_source_generator.hpp"


namespace serialpp::benchmark {

	template<serialisable T>
	void register_serialise_and_deserialise_benchmarks(std::string const name,
			random_source_generator<T> source_generator = {}, std::vector<std::string> tags = {}) {
		register_benchmark(benchmark_serialise_only(name, source_generator, tags));
		register_benchmark(benchmark_deserialise(name, source_generator, tags));
	}

	inline void register_all_benchmarks() {
		register_serialise_and_deserialise_benchmarks<null>("null (overhead baseline)");

		register_serialise_and_deserialise_benchmarks<std::uint8_t>("u8");
		register_serialise_and_deserialise_benchmarks<std::uint16_t>("u16");
		register_serialise_and_deserialise_benchmarks<std::uint32_t>("u32");
		register_serialise_and_deserialise_benchmarks<std::uint64_t>("u64");

		register_serialise_and_deserialise_benchmarks<std::int8_t>("i8");
		register_serialise_and_deserialise_benchmarks<std::int16_t>("i16");
		register_serialise_and_deserialise_benchmarks<std::int32_t>("i32");
		register_serialise_and_deserialise_benchmarks<std::int64_t>("i64");

		register_serialise_and_deserialise_benchmarks<float>("f32");
		register_serialise_and_deserialise_benchmarks<double>("f64");

		register_serialise_and_deserialise_benchmarks<pair<std::uint32_t, std::uint32_t>>("pair<u32, u32>");
		register_serialise_and_deserialise_benchmarks<pair<std::uint32_t, null>>("pair<u32, null>");

		register_serialise_and_deserialise_benchmarks<tuple<>>("tuple<>");
		register_serialise_and_deserialise_benchmarks<
			tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>>(
				"tuple<u32, u32, u32, u32, u32>");

		register_serialise_and_deserialise_benchmarks<static_array<std::uint8_t, 0>>("static_array<u8, 0>");
		register_serialise_and_deserialise_benchmarks<static_array<std::uint8_t, 8>>("static_array<u8, 8>");
		register_serialise_and_deserialise_benchmarks<static_array<std::uint8_t, 64>>("static_array<u8, 64>");
		register_serialise_and_deserialise_benchmarks<static_array<std::uint64_t, 8>>("static_array<u64, 8>");
		register_serialise_and_deserialise_benchmarks<static_array<std::uint64_t, 64>>("static_array<u64, 64>");
		register_serialise_and_deserialise_benchmarks<static_array<std::uint64_t, 1000>>("static_array<u64, 1000>");

		register_serialise_and_deserialise_benchmarks<optional<std::uint32_t>>(
			"optional<u32> prob=10%", {.value_prob=0.1});
		register_serialise_and_deserialise_benchmarks<optional<std::uint32_t>>(
			"optional<u32> prob=50%", {.value_prob=0.5});
		register_serialise_and_deserialise_benchmarks<optional<std::uint32_t>>(
			"optional<u32> prob=100%", {.value_prob=1.0});

		register_serialise_and_deserialise_benchmarks<variant<>>("variant<>");
		register_serialise_and_deserialise_benchmarks<variant<std::uint32_t, std::int32_t>>(
			"variant<u32, i32> prob=1:1");
		register_serialise_and_deserialise_benchmarks<variant<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t>>(
			"variant<u8, u16, u32, u64> prob=1:1:1:1");
		register_serialise_and_deserialise_benchmarks<variant<std::uint32_t, dynamic_array<std::uint32_t>>>(
			"variant<std::uint64_t, dynamic_array<std::uint64_t>> prob=9:1", {.value_probs={0.9, 0.1}});

		register_serialise_and_deserialise_benchmarks<dynamic_array<std::uint32_t>>(
			"dynamic_array<u32> size=0-20", {.min_size=0, .max_size=20});
		register_serialise_and_deserialise_benchmarks<dynamic_array<std::uint32_t>>(
			"dynamic_array<u32> size=100-500", {.min_size=100, .max_size=500});
		register_serialise_and_deserialise_benchmarks<dynamic_array<std::uint32_t>>(
			"dynamic_array<u32> size=1000-10000", {.min_size=1000, .max_size=10000});
		register_serialise_and_deserialise_benchmarks<dynamic_array<std::uint32_t>>(
			"dynamic_array<u32> size=1 small_range", {.min_size=1, .max_size=1, .large_range=false});
		register_serialise_and_deserialise_benchmarks<dynamic_array<std::uint32_t>>(
			"dynamic_array<u32> size=1 large_range", {.min_size=1, .max_size=1, .large_range=true});

		register_serialise_and_deserialise_benchmarks<simple_record>("simple_record");
		register_serialise_and_deserialise_benchmarks<intermediate_record>("intermediate_record");
		register_serialise_and_deserialise_benchmarks<complex_record>("complex_record");

		register_benchmark(benchmark_serialise_full<dynamic_array<std::uint32_t>>("dynamic_array<u32> size=0-20",
			serialise_full_dynamic_array_u32_generator<false>{.min_size=0, .max_size=20}));
		register_benchmark(benchmark_serialise_full<dynamic_array<std::uint32_t>>("dynamic_array<u32> size=100-500",
			serialise_full_dynamic_array_u32_generator<false>{.min_size=100, .max_size=500}));
		register_benchmark(benchmark_serialise_full<dynamic_array<std::uint32_t>>("dynamic_array<u32> size=1000-10000",
			serialise_full_dynamic_array_u32_generator<false>{.min_size=1000, .max_size=10000}));
		register_benchmark(benchmark_serialise_full<dynamic_array<std::uint32_t>>("dynamic_array<u32> size=1 small_range",
			serialise_full_dynamic_array_u32_generator<false>{1, 1}));
		register_benchmark(benchmark_serialise_full<dynamic_array<std::uint32_t>>("dynamic_array<u32> size=1 large_range",
			serialise_full_dynamic_array_u32_generator<true>{1, 1}));
	}

}

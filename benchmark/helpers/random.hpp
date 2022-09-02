#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>


namespace serialpp::benchmark {

	// Extremely fast xorshift random number generator.
	class xorshift_engine {
	public:
		// xorshift64 algorithm, code from https://en.wikipedia.org/wiki/Xorshift

		using result_type = std::uint64_t;

		explicit constexpr xorshift_engine(std::uint64_t const seed) noexcept :
			_state{seed | 1u}
		{}

		constexpr std::uint64_t operator()() noexcept {
			auto s = _state;
			s ^= s << 13;
			s ^= s >> 7;
			s ^= s << 17;
			_state = s;
			return s;
		}

		[[nodiscard]]
		static constexpr std::uint64_t min() noexcept {
			return 1;
		}

		[[nodiscard]]
		static constexpr std::uint64_t max() noexcept {
			return std::numeric_limits<std::uint64_t>::max();
		}

	private:
		std::uint64_t _state;
	};


	// Collection of four xorshift_engine instances.
	// This is useful for fast random generation, where loop dependencies on a single engine become real bottlenecks.
	class random_state {
	public:
		explicit constexpr random_state(std::uint64_t const seed) noexcept :
			_engines{{
				xorshift_engine{seed},
				xorshift_engine{~seed},
				xorshift_engine{seed + 1},
				xorshift_engine{~(seed + 1)}
			}}
		{}

		template<std::size_t I> requires (I < 4)
		[[nodiscard]]
		constexpr xorshift_engine& get() noexcept {
			return std::get<I>(_engines);
		}

		template<std::invocable<xorshift_engine&> F>
		constexpr void generate(std::size_t const count, F&& generator) {
			std::size_t i = 0;
			switch (count % 4) {
			case 0:
				while (i < count) {
					std::forward<F>(generator)(get<0>());
					++i;
					[[fallthrough]];
			case 3:
					std::forward<F>(generator)(get<1>());
					++i;
					[[fallthrough]];
			case 2:
					std::forward<F>(generator)(get<2>());
					++i;
					[[fallthrough]];
			case 1:
					std::forward<F>(generator)(get<3>());
					++i;
				}
				break;
			default:
				assert(false);
			}
		}

	private:
		std::array<xorshift_engine, 4> _engines;
	};


	template<bool FastAssign = false, typename T, std::invocable<xorshift_engine&> F>
	void random_vector_generate(random_state& random, std::vector<T>& vec, std::size_t const count, F&& generator) {
		if constexpr (std::semiregular<T> && FastAssign) {
			vec.resize(count);
			std::size_t i = 0;
			random.generate(count, [&vec, &generator, &i](auto& random_engine) {
				vec[i] = std::forward<F>(generator)(random_engine);
				++i;
			});
		}
		else {
			vec.clear();
			random.generate(count, [&vec, &generator](auto& random_engine) {
				vec.push_back(std::forward<F>(generator)(random_engine));
			});
		}
	}

}

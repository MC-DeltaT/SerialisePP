#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <random>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <serialpp/serialpp.hpp>

#include "utility.hpp"


namespace serialpp::benchmark {

	template<serialisable T>
	struct random_source_generator;

	template<>
	struct random_source_generator<null> {
		[[nodiscard]]
		static constexpr double average_data_size() noexcept {
			return 0;
		}

		[[nodiscard]]
		static constexpr std::size_t max_serialised_size() noexcept {
			return fixed_data_size_v<null>;
		}

		[[nodiscard]]
		constexpr serialise_source<null> operator()(auto&, double&) const noexcept {
			return {};
		}
	};

	template<>
	struct random_source_generator<std::byte> {
		[[nodiscard]]
		static constexpr double average_data_size() noexcept {
			return 1;
		}

		[[nodiscard]]
		static constexpr std::size_t max_serialised_size() noexcept {
			return fixed_data_size_v<std::byte>;
		}

		constexpr serialise_source<std::byte> operator()(auto& random_engine, double& data_written) const {
			data_written += 1;
			return {static_cast<std::byte>(random_engine())};
		}
	};

	template<std::unsigned_integral U>
	struct random_source_generator<U> {
		[[nodiscard]]
		static constexpr double average_data_size() noexcept {
			return sizeof(U);
		}

		[[nodiscard]]
		static constexpr std::size_t max_serialised_size() noexcept {
			return fixed_data_size_v<U>;
		}

		constexpr serialise_source<U> operator()(auto& random_engine, double& data_written) const {
			data_written += sizeof(U);
			return {static_cast<U>(random_engine())};
		}
	};

	template<std::signed_integral S>
	struct random_source_generator<S> {
		[[nodiscard]]
		static constexpr double average_data_size() noexcept {
			return sizeof(S);
		}

		[[nodiscard]]
		static constexpr std::size_t max_serialised_size() noexcept {
			return fixed_data_size_v<S>;
		}

		constexpr serialise_source<S> operator()(auto& random_engine, double& data_written) const {
			data_written += sizeof(S);
			return {std::bit_cast<S>(static_cast<std::make_unsigned_t<S>>(random_engine()))};
		}
	};

	template<floating_point F>
	struct random_source_generator<F> {
		[[nodiscard]]
		static constexpr double average_data_size() noexcept {
			return sizeof(F);
		}

		[[nodiscard]]
		static constexpr std::size_t max_serialised_size() noexcept {
			return fixed_data_size_v<F>;
		}

		constexpr serialise_source<F> operator()(auto& random_engine, double& data_written) const {
			data_written += sizeof(F);
			return {static_cast<F>(random_engine()) - random_engine.max() / 2};
		}
	};

	template<serialisable T1, serialisable T2>
	struct random_source_generator<pair<T1, T2>> {
		[[no_unique_address]]
		random_source_generator<T1> first;
		[[no_unique_address]]
		random_source_generator<T2> second;

		[[nodiscard]]
		constexpr double average_data_size() const {
			return first.average_data_size() + second.average_data_size();
		}

		[[nodiscard]]
		constexpr std::size_t max_serialised_size() const {
			return first.max_serialised_size() + second.max_serialised_size();
		}

		constexpr serialise_source<pair<T1, T2>> operator()(auto& random_engine, double& data_written) const {
			return {first(random_engine, data_written), second(random_engine, data_written)};
		}
	};

	template<serialisable... Ts>
	struct random_source_generator<tuple<Ts...>> {
		[[no_unique_address]]
		std::tuple<random_source_generator<Ts>...> elements;

		[[nodiscard]]
		constexpr double average_data_size() const {
			return std::apply([](auto const&... es) {
				return (es.average_data_size() + ... + 0.0);
			}, elements);
		}

		[[nodiscard]]
		constexpr std::size_t max_serialised_size() const {
			return std::apply([](auto const&... es) {
				return (es.max_serialised_size() + ... + std::size_t{0});
			}, elements);
		}

		constexpr serialise_source<tuple<Ts...>> operator()(auto& random_engine, double& data_written) const {
			return std::apply([&random_engine, &data_written](auto const&... es) {
				return serialise_source<tuple<Ts...>>{es(random_engine, data_written)...};
			}, elements);
		}
	};

	template<serialisable T, std::size_t Size>
	struct random_source_generator<static_array<T, Size>> {
		[[no_unique_address]]
		random_source_generator<T> element_generator;

		[[nodiscard]]
		constexpr double average_data_size() const {
			return element_generator.average_data_size() * Size;
		}

		[[nodiscard]]
		constexpr std::size_t max_serialised_size() const {
			return element_generator.max_serialised_size() * Size;
		}

		constexpr serialise_source<static_array<T, Size>> operator()(auto& random_engine, double& data_written) const {
			serialise_source<static_array<T, Size>> source;
			std::ranges::generate(source.elements, [this, &random_engine, &data_written] {
				return element_generator(random_engine, data_written);
			});
			return source;
		}
	};

	template<serialisable T>
	struct random_source_generator<dynamic_array<T>> {
		std::size_t min_size = 0;
		std::size_t max_size = 1000;
		[[no_unique_address]]
		random_source_generator<T> element_generator;
		bool large_range = false;

		[[nodiscard]]
		constexpr double average_data_size() const {
			return (min_size / 2.0 + max_size / 2.0) * element_generator.average_data_size();
		}

		[[nodiscard]]
		constexpr std::size_t max_serialised_size() const {
			return fixed_data_size_v<dynamic_array<T>> + max_size * element_generator.max_serialised_size();
		}

		serialise_source<dynamic_array<T>> operator()(auto& random_engine, double& data_written) const {
			assert(min_size <= max_size);
			std::uniform_int_distribution<std::size_t> size_dist{min_size, max_size};
			auto const size = size_dist(random_engine);

			std::vector<serialise_source<T>> elements;
			elements.reserve(size);
			std::generate_n(std::back_inserter(elements), size, [this, &random_engine, &data_written] {
				return element_generator(random_engine, data_written);
			});

			if (large_range) {
				return {large_range_wrapper{std::move(elements)}};
			}
			else {
				static_assert(32 >= sizeof elements);
				return {std::move(elements)};
			}
		}
	};

	template<serialisable T>
	struct random_source_generator<optional<T>> {
		double value_prob = 0.5;
		[[no_unique_address]]
		random_source_generator<T> value_generator;

		[[nodiscard]]
		constexpr double average_data_size() const {
			// We'll say an empty optional is 1 bit of information.
			return value_prob * value_generator.average_data_size() + (1.0 - value_prob) * 0.125;
		}

		[[nodiscard]]
		constexpr std::size_t max_serialised_size() const {
			return fixed_data_size_v<optional<T>> + (value_prob > 0) * value_generator.max_serialised_size();
		}

		serialise_source<optional<T>> operator()(auto& random_engine, double& data_written) const {
			std::uniform_real_distribution<double> has_value_dist{0, 1};
			auto const has_value = has_value_dist(random_engine) < value_prob;
			if (has_value) {
				return {value_generator(random_engine, data_written)};
			}
			else {
				data_written += 0.125;
				return {};
			}
		}
	};

	template<serialisable... Ts>
	struct random_source_generator<variant<Ts...>> {
		std::array<double, sizeof...(Ts)> value_probs = [] {
			decltype(value_probs) probs;
			std::ranges::fill(probs, 1.0 / probs.size());
			return probs;
		}();
		[[no_unique_address]]
		std::tuple<random_source_generator<Ts>...> value_generators;

		[[nodiscard]]
		constexpr double average_data_size() const {
			return [this] <std::size_t... I>(std::index_sequence<I...>) {
				return ((std::get<I>(value_probs) * std::get<I>(value_generators).average_data_size()) + ... + 0.0);
			}(std::make_index_sequence<sizeof...(Ts)>{});
		}

		[[nodiscard]]
		constexpr std::size_t max_serialised_size() const {
			auto const value_size = [this] <std::size_t... I>(std::index_sequence<I...>) {
				return std::max(
					{(std::get<I>(value_probs) > 0) * std::get<I>(value_generators).max_serialised_size()...,
						std::size_t{0}});
			}(std::make_index_sequence<sizeof...(Ts)>{});
			return fixed_data_size_v<variant<Ts...>> + value_size;
		}

		serialise_source<variant<Ts...>> operator()(auto& random_engine, double& data_written) const {
			if constexpr (sizeof...(Ts) == 0) {
				return {};
			}
			else {
				auto r = std::uniform_real_distribution<double>{0, 1}(random_engine);
				for (std::size_t i = 0; i < sizeof...(Ts); ++i) {
					if (r < value_probs.at(i)) {
						return _generate<0>(i, random_engine, data_written);
					}
					else {
						r -= value_probs.at(i);
					}
				}
				assert(false);
			}
		}

	private:
		template<std::size_t I> requires (I < sizeof...(Ts))
		constexpr serialise_source<variant<Ts...>> _generate(std::size_t const index, auto& random_engine,
				double& data_written) const {
			assert(index < sizeof...(Ts));
			if (I != index) {
				if constexpr (I + 1 < sizeof...(Ts)) {
					return _generate<I + 1>(index, random_engine, data_written);
				}
				else {
					// Unreachable.
				}
			}
			return serialise_source<variant<Ts...>>{
				std::in_place_index<I>, std::get<I>(value_generators)(random_engine, data_written)};
		}
	};

	template<class Fields>
	struct fields_random_generator_tuple;

	template<detail::field... Fs>
	struct fields_random_generator_tuple<type_list<Fs...>>
		: std::type_identity<std::tuple<random_source_generator<typename Fs::type>...>> {};

	template<record_type R>
	struct random_source_generator<R> {
		[[no_unique_address]]
		fields_random_generator_tuple<typename R::fields>::type fields;

		[[nodiscard]]
		constexpr double average_data_size() const {
			return std::apply([](auto const&... fs) {
				return (fs.average_data_size() + ... + 0.0);
			}, fields);
		}

		[[nodiscard]]
		constexpr std::size_t max_serialised_size() const {
			return std::apply([](auto const&... fs) {
				return (fs.max_serialised_size() + ... + std::size_t{0});
			}, fields);
		}

		constexpr serialise_source<R> operator()(auto& random_engine, double& data_written) const {
			return std::apply([&random_engine, &data_written](auto const&... fs) {
				return serialise_source<R>{fs(random_engine, data_written)...};
			}, fields);
		}
	};

}

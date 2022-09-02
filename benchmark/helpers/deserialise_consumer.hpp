#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <serialpp/serialpp.hpp>
#include <serialpp/utility.hpp>


namespace serialpp::benchmark {

	static std::uint64_t deserialise_consume_sink;


	template<serialisable T>
	struct deserialise_consumer;

	template<>
	struct deserialise_consumer<null> {
		[[nodiscard]]
		static constexpr std::uint64_t reduce(null) noexcept {
			return 0;
		}

		static constexpr void consume(null) noexcept {}
	};

	template<scalar S>
	struct deserialise_consumer<S> {
		[[nodiscard]]
		static constexpr std::uint64_t reduce(S const value) noexcept {
			if constexpr (floating_point<S>) {
				if constexpr (sizeof(S) == 4) {
					return std::bit_cast<std::uint32_t>(value);
				}
				else if constexpr (sizeof(S) == 8) {
					return std::bit_cast<std::uint64_t>(value);
				}
				else {
					static_assert(!floating_point<S>);
				}
			}
			else {
				return static_cast<std::uint64_t>(value);
			}
		}

		static void consume(S const value) noexcept {
			deserialise_consume_sink = reduce(value);
		}
	};

	template<serialisable T1, serialisable T2>
	struct deserialise_consumer<pair<T1, T2>> {
		using first_consumer = deserialise_consumer<T1>;
		using second_consumer = deserialise_consumer<T2>;

		[[nodiscard]]
		static constexpr std::uint64_t reduce(deserialiser<pair<T1, T2>> const& deser) {
			return first_consumer::reduce(deser.first()) + second_consumer::reduce(deser.second());
		}

		static void consume(deserialiser<pair<T1, T2>> const& deser) {
			deserialise_consume_sink = reduce(deser);
		}
	};

	template<serialisable... Ts>
	struct deserialise_consumer<tuple<Ts...>> {
		using element_consumers = type_list<deserialise_consumer<Ts>...>;

		[[nodiscard]]
		static constexpr std::uint64_t reduce(deserialiser<tuple<Ts...>> const& deser) {
			return [&deser]<std::size_t... Is>(std::index_sequence<Is...>) {
				return (detail::type_list_element<element_consumers, Is>::type::reduce(deser.get<Is>())
					+ ... + std::uint64_t{0});
			}(std::make_index_sequence<sizeof...(Ts)>{});
		}

		static void consume(deserialiser<tuple<Ts...>> const& deser) {
			if constexpr (sizeof...(Ts) > 0) {
				deserialise_consume_sink = reduce(deser);
			}
		}
	};

	template<serialisable T, std::size_t Size>
	struct deserialise_consumer<static_array<T, Size>> {
		using element_consumer = deserialise_consumer<T>;

		[[nodiscard]]
		static constexpr std::uint64_t reduce(deserialiser<static_array<T, Size>> const& deser) {
			std::uint64_t result = 0;
			for (auto const& element : deser.elements()) {
				result += element_consumer::reduce(element);
			}
			return result;
		}

		static void consume(deserialiser<static_array<T, Size>> const& deser) {
			if constexpr (Size > 0) {
				deserialise_consume_sink = reduce(deser);
			}
		}
	};

	template<serialisable T>
	struct deserialise_consumer<optional<T>> {
		using value_consumer = deserialise_consumer<T>;

		[[nodiscard]]
		static constexpr std::uint64_t reduce(deserialiser<optional<T>> const& deser) {
			if (deser.has_value()) {
				return value_consumer::reduce(*deser);
			}
			else {
				return 0;
			}
		}

		static void consume(deserialiser<optional<T>> const& deser) {
			if (deser.has_value()) {
				value_consumer::consume(*deser);
			}
		}
	};

	template<serialisable T>
	struct deserialise_consumer<dynamic_array<T>> {
		using element_consumer = deserialise_consumer<T>;

		[[nodiscard]]
		static constexpr std::uint64_t reduce(deserialiser<dynamic_array<T>> const& deser) {
			std::uint64_t result = 0;
			for (auto const& element : deser.elements()) {
				result += element_consumer::reduce(element);
			}
			return result;
		}

		static void consume(deserialiser<dynamic_array<T>> const& deser) {
			deserialise_consume_sink = reduce(deser);
		}
	};

	template<serialisable... Ts>
	struct variant_deserialise_consumer;

	template<>
	struct variant_deserialise_consumer<> {
		static void reduce() = delete;

		static void consume() = delete;
	};

	template<serialisable T, serialisable... Ts>
	struct variant_deserialise_consumer<T, Ts...> : variant_deserialise_consumer<Ts...> {
		using element_consumer = deserialise_consumer<T>;

		using variant_deserialise_consumer<Ts...>::reduce;
		using variant_deserialise_consumer<Ts...>::consume;

		[[nodiscard]]
		static constexpr std::uint64_t reduce(deserialise_t<T> const& d) {
			return element_consumer::reduce(d);
		}

		static void consume(deserialise_t<T> const& d) {
			return element_consumer::consume(d);
		}
	};

	template<serialisable... Ts>
	struct deserialise_consumer<variant<Ts...>> {
		using elements_consumer = variant_deserialise_consumer<Ts...>;

		[[nodiscard]]
		static constexpr std::uint64_t reduce(deserialiser<variant<Ts...>> const& deser) {
			if constexpr (sizeof...(Ts) == 0) {
				return 0;
			}
			else {
				return deser.visit([](auto const& deser) {
					return elements_consumer::reduce(deser);
				});
			}
		}

		static void consume(deserialiser<variant<Ts...>> const& deser) {
			deser.visit([](auto const& deser) {
				elements_consumer::consume(deser);
			});
		}
	};

	template<class Fields>
	struct fields_deserialise_consumer;

	template<detail::field... Fs>
	struct fields_deserialise_consumer<type_list<Fs...>>
		: std::type_identity<type_list<deserialise_consumer<typename Fs::type>...>> {};

	template<record_type R>
	struct deserialise_consumer<R> {
		using field_consumers = fields_deserialise_consumer<typename R::fields>::type;

		[[nodiscard]]
		static constexpr std::uint64_t reduce(deserialiser<R> const& deser) {
			return [&deser]<std::size_t... Is>(std::index_sequence<Is...>) {
				return (detail::type_list_element<field_consumers, Is>::type::reduce(deser.get<Is>())
					+ ... + std::uint64_t{0});
			}(std::make_index_sequence<R::fields::size>{});
		}

		static void consume(deserialiser<R> const& deser) {
			deserialise_consume_sink = reduce(deser);
		}
	};

}

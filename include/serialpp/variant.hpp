#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>

#include "common.hpp"
#include "scalar.hpp"
#include "utility.hpp"


namespace serialpp {

    // TODO: optimisation for 0 and 1 types?

    /*
        Variant:
            Fixed data is a type index (VariantIndexType) indicating which type is contained, and an offset to the
            variable data (DataOffset).
            Variable data is the contained value.

            If the set of possible types is empty, then the type index and offset are not significant.
    */


    using VariantIndexType = std::uint8_t;

    inline static constexpr std::size_t MAX_VARIANT_TYPES = std::numeric_limits<VariantIndexType>::max();


    // Serialisable type that holds exactly one instance of a type from a set of possible types.
    // Can have up to MAX_VARIANT_TYPES types (including zero).
    template<Serialisable... Ts> requires (sizeof...(Ts) <= MAX_VARIANT_TYPES)
    struct Variant {
        using IndexType = VariantIndexType;
    };


    template<Serialisable... Ts>
    struct FixedDataSize<Variant<Ts...>>
        : SizeTConstant<FIXED_DATA_SIZE<VariantIndexType> + FIXED_DATA_SIZE<DataOffset>> {};


    template<>
    class SerialiseSource<Variant<>> : public std::variant<std::monostate> {
    public:
        using std::variant<std::monostate>::variant;
    };

    template<Serialisable... Ts>
    class SerialiseSource<Variant<Ts...>> : public std::variant<SerialiseSource<Ts>...> {
    public:
        // TODO: is having the in_place_ constructors explicit good for us?
        using std::variant<SerialiseSource<Ts>...>::variant;
    };


    template<Serialisable... Ts>
    struct Serialiser<Variant<Ts...>> {
        SerialiseTarget operator()(SerialiseSource<Variant<Ts...>> const& source, SerialiseTarget target) const {
            if (source.valueless_by_exception()) {
                throw std::invalid_argument{"source cannot be valueless"};
            }

            auto const index = source.index();
            auto const relative_variable_offset = target.relative_field_variable_offset();
            
            target = target.push_fixed_field<VariantIndexType>([index](SerialiseTarget index_target) {
                assert(index <= MAX_VARIANT_TYPES);
                return Serialiser<VariantIndexType>{}(static_cast<VariantIndexType>(index), index_target);
            }).push_fixed_field<DataOffset>([relative_variable_offset](SerialiseTarget offset_target) {
                return Serialiser<DataOffset>{}(to_data_offset(relative_variable_offset), offset_target);
            });

            if constexpr (sizeof...(Ts) > 0) {
                return std::visit([&target] <Serialisable T> (SerialiseSource<T> const& value_source) {
                    return target.push_variable_fields<T>(1, [&value_source](SerialiseTarget value_target) {
                        return Serialiser<T>{}(value_source, value_target);
                    });
                }, source);
            }
            else {
                return target;
            }
        }
    };


    template<Serialisable... Ts>
    class Deserialiser<Variant<Ts...>> : public DeserialiserBase<Variant<Ts...>> {
    public:
        using DeserialiserBase<Variant<Ts...>>::DeserialiserBase;

        // Gets the zero-based index of the contained type.
        [[nodiscard]]
        std::size_t index() const requires (sizeof...(Ts) > 0) {
            return Deserialiser<VariantIndexType>{this->_fixed_data, this->_variable_data}.value();
        }

        // If I == index(), gets the contained value. Otherwise, throws std::bad_variant_access.
        template<std::size_t I> requires (I < sizeof...(Ts))
        [[nodiscard]]
        auto get() const {
            if (I == index()) {
                return _get<I>();
            }
            else {
                throw std::bad_variant_access{};
            }
        }

        // Invokes the specified function with the contained value as the argument.
        template<typename F> requires (std::invocable<F, AutoDeserialiseScalarResult<Ts>> && ...)
        decltype(auto) visit(F&& visitor) const {
            if constexpr (sizeof...(Ts) > 0) {
                return _visit<0>(std::forward<F>(visitor), index());
            }
        }

    private:
        [[nodiscard]]
        DataOffset _offset() const requires (sizeof...(Ts) > 0) {
            return Deserialiser<DataOffset>{
                this->_fixed_data.subspan(FIXED_DATA_SIZE<VariantIndexType>),
                this->_variable_data
            }.value();
        }

        // Gets the contained value by index.
        template<std::size_t I> requires (I < sizeof...(Ts))
        auto _get() const {
            using T = TypeListElement<I, TypeList<Ts...>>;
            auto const offset = _offset();
            this->_check_variable_offset(offset);
            Deserialiser<T> const deserialiser{
                this->_variable_data.subspan(offset),
                this->_variable_data
            };
            return auto_deserialise_scalar(deserialiser);
        }

        template<std::size_t I, typename F>
            requires (sizeof...(Ts) > 0) && (std::invocable<F, AutoDeserialiseScalarResult<Ts>> && ...)
        decltype(auto) _visit(F&& visitor, std::size_t index) const {
            assert(index < sizeof...(Ts));
            if (I == index) {
                return std::invoke(std::forward<F>(visitor), _get<I>());
            }
            else if constexpr (I + 1 < sizeof...(Ts)) {
                return _visit<I + 1>(std::forward<F>(visitor), index);
            }
            // Needed to silence no return value warnings. Will never actually be executed.
            throw std::logic_error{"Deserialiser<Variant>::_visit() is broken"};
        }
    };

}

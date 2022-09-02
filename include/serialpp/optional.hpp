#pragma once

#include <cassert>
#include <cstddef>
#include <optional>

#include "common.hpp"
#include "scalar.hpp"


namespace serialpp {

    /*
        optional:
            Fixed data is an offset (data_offset_t) that indicates both if there is a contained value, and the offset of
            the value (if present).
            If the offset is 0, then there is no value and no variable data is present.
            If the offset is > 0, then the value starts at byte [offset - 1].
    */


    // Serialisable type which contains either zero or one instance of a type.
    template<serialisable T>
    struct optional {
        using value_type = T;
    };


    template<serialisable T>
    struct fixed_data_size<optional<T>> : fixed_data_size<data_offset_t> {};


    template<serialisable T>
    class serialise_source<optional<T>> : public std::optional<serialise_source<T>> {
    public:
        using std::optional<serialise_source<T>>::optional;
    };


    template<serialisable T>
    struct serialiser<optional<T>> {
        static constexpr void serialise(serialise_source<optional<T>> const& source, serialise_buffer auto& buffer,
                std::size_t fixed_offset) {
            if (source.has_value()) {
                auto const variable_offset = buffer.span().size();
                auto const offset = detail::to_data_offset(variable_offset + 1);
                fixed_offset = push_fixed_subobject<data_offset_t>(fixed_offset,
                    detail::bind_serialise(serialise_source<data_offset_t>{offset}, buffer));
                push_variable_subobjects<T>(1, buffer, detail::bind_serialise(source.value(), buffer));
            }
            else {
                fixed_offset = push_fixed_subobject<data_offset_t>(fixed_offset,
                    detail::bind_serialise(serialise_source<data_offset_t>{0}, buffer));
            }
        }
    };


    template<serialisable T>
    class deserialiser<optional<T>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        // Checks if the optional contains a value.
        [[nodiscard]]
        constexpr bool has_value() const {
            // Note: does not check if the offset is valid.
            return _value_offset() > 0;
        }

        // Gets the contained value. has_value() must be true.
        [[nodiscard]]
        constexpr deserialise_t<T> operator*() const {
            auto offset = _value_offset();
            assert(offset > 0);
            offset -= 1;
            return deserialise<T>(_buffer, offset);
        }

        // Gets the contained value. If has_value() is false, throws std::bad_optional_access.
        [[nodiscard]]
        constexpr deserialise_t<T> value() const {
            if (has_value()) {
                return **this;
            }
            else {
                throw std::bad_optional_access{};
            }
        }

    private:
        // Offset from start of variable data section to contained value, plus 1.
        // 0 indicates no contained value, i.e. empty optional.
        [[nodiscard]]
        constexpr data_offset_t _value_offset() const {
            return deserialise<data_offset_t>(_buffer, _fixed_offset);
        }
    };

}

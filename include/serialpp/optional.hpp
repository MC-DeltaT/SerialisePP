#pragma once

#include <cassert>
#include <optional>

#include "common.hpp"
#include "scalar.hpp"


namespace serialpp {

    /*
        optional:
            Fixed data is an offset (data_offset_t) that indicates both if there is a contained value, and the variable
            offset of the value (if present).
            If the offset is 0, then there is no value and no variable data is present.
            If the offset is > 0, then the value starts at byte [offset - 1] of the variable data section.
    */


    // Serialisable type which contains either zero or one instance of a type.
    template<serialisable T>
    struct optional {};


    template<serialisable T>
    struct fixed_data_size<optional<T>> : fixed_data_size<data_offset_t> {};


    template<serialisable T>
    class serialise_source<optional<T>> : public std::optional<serialise_source<T>> {
    public:
        using std::optional<serialise_source<T>>::optional;
    };


    template<serialisable T>
    struct serialiser<optional<T>> {
        template<serialise_buffer Buffer>
        constexpr serialise_target<Buffer> operator()(serialise_source<optional<T>> const& source,
                serialise_target<Buffer> target) const {
            if (source.has_value()) {
                auto const relative_variable_offset = target.relative_subobject_variable_offset();
                return target.push_fixed_subobject<data_offset_t>([relative_variable_offset](auto offset_target) {
                    auto const offset = detail::to_data_offset(relative_variable_offset + 1);
                    return serialiser<data_offset_t>{}(offset, offset_target);
                }).push_variable_subobjects<T>(1, [&source](auto variable_target) {
                    return variable_target.push_fixed_subobject<T>([&source](auto value_target) {
                        return serialiser<T>{}(source.value(), value_target);
                    });
                });
            }
            else {
                return target.push_fixed_subobject<data_offset_t>([](auto target) {
                    return serialiser<data_offset_t>{}(0, target);
                });
            }
        }
    };


    template<serialisable T>
    class deserialiser<optional<T>> : public deserialiser_base {
    public:
        using deserialiser_base::deserialiser_base;

        constexpr deserialiser(const_bytes_span fixed_data, const_bytes_span variable_data) :
            deserialiser_base{no_fixed_buffer_check, fixed_data, variable_data}
        {
            check_fixed_buffer_size<optional<T>>(_fixed_data);
        }

        // Checks if the optional contains a value.
        [[nodiscard]]
        constexpr bool has_value() const {
            // Note: does not check if the offset is valid.
            return _value_offset() > 0;
        }

        // Gets the contained value. has_value() must be true.
        [[nodiscard]]
        constexpr auto_deserialise_t<T> operator*() const {
            auto offset = _value_offset();
            assert(offset > 0);
            offset -= 1;
            _check_variable_offset(offset);
            deserialiser<T> const deser{
                _variable_data.subspan(offset, fixed_data_size_v<T>),
                _variable_data
            };

            return auto_deserialise(deser);
        }

        // Gets the contained value. If has_value() is false, throws std::bad_optional_access.
        [[nodiscard]]
        constexpr auto_deserialise_t<T> value() const {
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
            return deserialiser<data_offset_t>{no_fixed_buffer_check, _fixed_data, _variable_data}.value();
        }
    };

}

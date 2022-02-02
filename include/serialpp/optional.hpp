#pragma once

#include <cassert>
#include <optional>

#include "common.hpp"
#include "scalar.hpp"


namespace serialpp {

    /*
        Optional:
            Fixed data is an offset (DataOffset) that indicates both if there is a contained value, and the variable
            offset of the value (if present).
            If the offset is 0, then there is no value and no variable data is present.
            If the offset is > 0, then the value starts at byte [offset - 1] of the variable data section.
    */


    // Serialisable type which contains either 0 or 1 instance of a type.
    template<typename T>
    struct Optional {};

    template<typename T>
    struct FixedDataSize<Optional<T>> : FixedDataSize<DataOffset> {};

    template<typename T>
    struct SerialiseSource<Optional<T>> : std::optional<SerialiseSource<T>> {};

    template<typename T>
    struct Serialiser<Optional<T>> {
        SerialiseTarget operator()(SerialiseSource<Optional<T>> const& source, SerialiseTarget target) const {
            if (source.has_value()) {
                assert(target.field_variable_offset >= target.fixed_size);
                auto const relative_variable_offset = target.field_variable_offset - target.fixed_size;
                return target.push_fixed_field<DataOffset>([relative_variable_offset](SerialiseTarget offset_target) {
                    return Serialiser<DataOffset>{}(relative_variable_offset + 1, offset_target);
                }).push_variable_field<T>([&source](SerialiseTarget value_target) {
                    return Serialiser<T>{}(source.value(), value_target);
                });
            }
            else {
                return target.push_fixed_field<DataOffset>([](SerialiseTarget target) {
                    return Serialiser<DataOffset>{}(0, target);
                });
            }
        }
    };

    template<typename T>
    struct Deserialiser<Optional<T>> : DeserialiserBase {
        using DeserialiserBase::DeserialiserBase;

        // Checks if the Optional contains a value.
        bool has_value() const {
            return _value_offset() > 0;
        }

        // Gets the contained value. has_value() must be true.
        auto value() const {
            auto offset = _value_offset();
            assert(offset > 0);
            offset -= 1;
            assert(offset <= variable_data.size());
            Deserialiser<T> const deserialiser{
                variable_data.subspan(offset),
                variable_data
            };

            return auto_deserialise_scalar(deserialiser);
        }

    private:
        // Offset from start of variable data section to contained value, plus 1.
        // 0 indicates no contained value, i.e. empty optional.
        DataOffset _value_offset() const {
            return Deserialiser<DataOffset>{fixed_data}.value();
        }
    };

}

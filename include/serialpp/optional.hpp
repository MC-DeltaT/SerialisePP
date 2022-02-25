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
    template<Serialisable T>
    struct Optional {};


    template<Serialisable T>
    struct FixedDataSize<Optional<T>> : FixedDataSize<DataOffset> {};


    template<Serialisable T>
    struct SerialiseSource<Optional<T>> : std::optional<SerialiseSource<T>> {
        using std::optional<SerialiseSource<T>>::optional;
    };


    template<Serialisable T>
    struct Serialiser<Optional<T>> {
        SerialiseTarget operator()(SerialiseSource<Optional<T>> const& source, SerialiseTarget target) const {
            if (source.has_value()) {
                auto const relative_variable_offset = target.relative_field_variable_offset();
                return target.push_fixed_field<DataOffset>([relative_variable_offset](SerialiseTarget offset_target) {
                    auto const offset = to_data_offset(relative_variable_offset + 1);
                    return Serialiser<DataOffset>{}(offset, offset_target);
                }).push_variable_fields<T>(1, [&source](SerialiseTarget variable_target) {
                    return variable_target.push_fixed_field<T>([&source](SerialiseTarget value_target) {
                        return Serialiser<T>{}(source.value(), value_target);
                    });
                });
            }
            else {
                return target.push_fixed_field<DataOffset>([](SerialiseTarget target) {
                    return Serialiser<DataOffset>{}(0, target);
                });
            }
        }
    };


    template<Serialisable T>
    class Deserialiser<Optional<T>> : public DeserialiserBase<Optional<T>> {
    public:
        using DeserialiserBase<Optional<T>>::DeserialiserBase;

        // Checks if the Optional contains a value.
        [[nodiscard]]
        bool has_value() const {
            // Note: does not check if the offset is valid.
            return _value_offset() > 0;
        }

        // Gets the contained value. has_value() must be true.
        [[nodiscard]]
        auto operator*() const {
            auto offset = _value_offset();
            assert(offset > 0);
            offset -= 1;
            this->_check_variable_offset(offset);
            Deserialiser<T> const deserialiser{
                this->_variable_data.subspan(offset),
                this->_variable_data
            };

            return auto_deserialise_scalar(deserialiser);
        }

        // Gets the contained value. If has_value() is false, throws std::bad_optional_access.
        [[nodiscard]]
        auto value() const {
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
        DataOffset _value_offset() const {
            return Deserialiser<DataOffset>{this->_fixed_data, this->_variable_data}.value();
        }
    };

}

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "common.hpp"
#include "scalar.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        List:
            Fixed data is a element count (ListSizeType) and an offset (DataOffset).
            If the element count is > 0, then that many elements are contained starting at the offset in the variable
            data section.
            If the element count is 0, then the offset is unused and no variable data is present.
    */


    using ListSizeType = std::uint16_t;


    // Safely casts to ListSizeType.
    ListSizeType to_list_size(std::size_t offset) {
        assert(std::cmp_less_equal(offset, std::numeric_limits<ListSizeType>::max()));
        return static_cast<ListSizeType>(offset);
    }


    // Variable-length homogeneous array. Can hold up to 2^16 - 1 elements.
    template<typename T>
    struct List {
        using SizeType = ListSizeType;
    };

    template<typename T>
    struct FixedDataSize<List<T>>
        : SizeTConstant<FIXED_DATA_SIZE<ListSizeType> + FIXED_DATA_SIZE<DataOffset>> {};

    template<typename T>
    class SerialiseSource<List<T>> {
    public:
        // TODO: does this really need type erasure?

        template<class R>
        SerialiseSource(R&& range) :
            _range{std::make_unique<RangeWrapper<std::remove_cvref_t<R>>>(std::forward<R>(range))}
        {}

    private:
        struct Visitor {
            SerialiseTarget target;

            template<class R>
            std::pair<SerialiseTarget, std::size_t> operator()(R& range) const {
                std::size_t count = 0;
                auto target = this->target;
                for (auto const& element : range) {
                    target = target.push_variable_field<T>([&element](SerialiseTarget element_target) {
                        return Serialiser<T>{}(element, element_target);
                    });
                    ++count;
                }
                return {target, count};
            }
        };

        struct RangeWrapperBase {
            virtual ~RangeWrapperBase() = default;

            virtual std::pair<SerialiseTarget, std::size_t> visit(Visitor const& visitor) = 0;
        };

        template<class R>
        struct RangeWrapper : RangeWrapperBase {
            R range;

            RangeWrapper(R range) :
                range{std::move(range)}
            {}

            std::pair<SerialiseTarget, std::size_t> visit(Visitor const& visitor) override {
                return visitor(range);
            }
        };

        std::unique_ptr<RangeWrapperBase> _range;

        friend struct Serialiser<List<T>>;
    };

    template<typename T>
    struct Serialiser<List<T>> {
        SerialiseTarget operator()(SerialiseSource<List<T>> const& source, SerialiseTarget target) const {
            assert(target.field_variable_offset >= target.fixed_size);
            auto const relative_variable_offset = target.field_variable_offset - target.fixed_size;

            typename SerialiseSource<List<T>>::Visitor const visitor{target};
            auto const [new_target, element_count] = source._range->visit(visitor);

            return new_target.push_fixed_field<ListSizeType>([element_count](SerialiseTarget size_target) {
                return Serialiser<ListSizeType>{}(to_list_size(element_count), size_target);
            }).push_fixed_field<DataOffset>([relative_variable_offset](SerialiseTarget offset_target) {
                return Serialiser<DataOffset>{}(to_data_offset(relative_variable_offset), offset_target);
            });
        }
    };

    template<typename T>
    struct Deserialiser<List<T>> : DeserialiserBase {
        using DeserialiserBase::DeserialiserBase;

        // Gets the number of elements in the List.
        std::size_t size() const {
            return Deserialiser<ListSizeType>{fixed_data, variable_data}.value();
        }

        // Deserialises the element at the index. index must be < size().
        auto operator[](std::size_t index) const {
            assert(index < size());
            auto const offset = _offset();
            Deserialiser<T> const deserialiser{
                variable_data.subspan(offset + FIXED_DATA_SIZE<T> * index),
                variable_data
            };
            return auto_deserialise_scalar(deserialiser);
        }

        // TODO: iterators

    private:
        DataOffset _offset() const {
            return Deserialiser<DataOffset>{
                fixed_data.subspan(FIXED_DATA_SIZE<ListSizeType>),
                variable_data
            }.value();
        }
    };

}

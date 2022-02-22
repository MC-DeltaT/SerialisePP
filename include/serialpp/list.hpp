#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

#include "common.hpp"
#include "scalar.hpp"
#include "utility.hpp"


namespace serialpp {

    /*
        List:
            Fixed data is an element count (ListSizeType) and an offset (DataOffset).
            If the element count is > 0, then that many elements are contained starting at the offset in the variable
            data section.
            If the element count is 0, then the offset is unused and no variable data is present.
    */

    // TODO: maybe put list size in variable data? Could use variable integer encoding to allow larger size


    using ListSizeType = std::uint16_t;


    // Safely casts to ListSizeType.
    [[nodiscard]]
    inline ListSizeType to_list_size(std::size_t offset) {
        assert(std::cmp_less_equal(offset, std::numeric_limits<ListSizeType>::max()));
        return static_cast<ListSizeType>(offset);
    }


    // Serialisable variable-length homogeneous array. Can hold up to 2^16 - 1 elements.
    template<Serialisable T>
    struct List {
        using SizeType = ListSizeType;
    };


    template<Serialisable T>
    struct FixedDataSize<List<T>>
        : SizeTConstant<FIXED_DATA_SIZE<ListSizeType> + FIXED_DATA_SIZE<DataOffset>> {};


    template<Serialisable T>
    class SerialiseSource<List<T>> {
    public:
        // TODO: does this really need type erasure?
        // TODO: optimisation for specific range types
        // TODO: copyable

        SerialiseSource() :     // TODO: don't allocate?
            SerialiseSource{std::ranges::views::empty<SerialiseSource<T>>}
        {}

        template<class R>
            requires std::ranges::forward_range<R> && std::ranges::sized_range<R>
                && std::convertible_to<std::ranges::range_reference_t<R>, SerialiseSource<T> const&>
        SerialiseSource(R&& range) :
            _range{std::make_unique<RangeWrapper<std::remove_cvref_t<R>>>(std::forward<R>(range))}
        {}

        template<std::size_t N>
        SerialiseSource(SerialiseSource<T> (&& elements)[N]) :
            _range{std::make_unique<RangeWrapper<std::array<SerialiseSource<T>, N>>>(std::to_array(std::move(elements)))}
        {}

    private:
        struct Visitor {
            SerialiseTarget target;

            template<class R>
            std::pair<SerialiseTarget, std::size_t> operator()(R&& range) const {
                std::size_t count = std::ranges::size(range);
                // Keep track of real count if for some bizarre reason range size is wrong.
                std::size_t actual_count = 0;
                auto const target = this->target.push_variable_fields<T>(count,
                    [&range, count, &actual_count](SerialiseTarget variable_target) {
                        auto element_it = std::ranges::cbegin(range);
                        auto const end_it = std::ranges::cend(range);
                        while (actual_count < count && element_it != end_it) {
                            auto const& element = *element_it;
                            variable_target = variable_target.push_fixed_field<T>(
                                [&element](SerialiseTarget element_target) {
                                    return Serialiser<T>{}(element, element_target);
                                });
                            ++actual_count;
                            ++element_it;
                        }
                        return variable_target;
                    });
                return {target, actual_count};
            }
        };

        struct RangeWrapperBase {
            virtual ~RangeWrapperBase() = default;

            virtual std::pair<SerialiseTarget, std::size_t> visit(Visitor const& visitor) = 0;
        };

        template<class R>
        struct RangeWrapper : RangeWrapperBase {
            R range;

            template<typename... Args>
            RangeWrapper(Args&&... args) :
                range{std::forward<Args>(args)...}
            {}

            std::pair<SerialiseTarget, std::size_t> visit(Visitor const& visitor) override {
                return visitor(range);
            }
        };

        std::unique_ptr<RangeWrapperBase> _range;

        friend struct Serialiser<List<T>>;
    };

    template<Serialisable T>
    struct Serialiser<List<T>> {
        SerialiseTarget operator()(SerialiseSource<List<T>> const& source, SerialiseTarget target) const {
            auto const relative_variable_offset = target.relative_field_variable_offset();

            typename SerialiseSource<List<T>>::Visitor const visitor{target};
            auto const [new_target, element_count] = source._range->visit(visitor);

            return new_target.push_fixed_field<ListSizeType>([element_count](SerialiseTarget size_target) {
                return Serialiser<ListSizeType>{}(to_list_size(element_count), size_target);
            }).push_fixed_field<DataOffset>([relative_variable_offset](SerialiseTarget offset_target) {
                return Serialiser<DataOffset>{}(to_data_offset(relative_variable_offset), offset_target);
            });
        }
    };

    template<Serialisable T>
    struct Deserialiser<List<T>> : DeserialiserBase {
        using DeserialiserBase::DeserialiserBase;

        // Gets the number of elements in the List.
        [[nodiscard]]
        std::size_t size() const {
            return Deserialiser<ListSizeType>{fixed_data, variable_data}.value();
        }

        // Deserialises the element at the specified index. index must be < size().
        [[nodiscard]]
        auto operator[](std::size_t index) const {
            assert(index < size());
            auto const offset = _offset();
            Deserialiser<T> const deserialiser{
                variable_data.subspan(offset + FIXED_DATA_SIZE<T> * index, FIXED_DATA_SIZE<T>),
                variable_data
            };
            return auto_deserialise_scalar(deserialiser);
        }

        // Gets a view of the elements.
        [[nodiscard]]
        auto elements() const {
            return std::ranges::views::iota(std::size_t{0}, size())
                | std::ranges::views::transform([this](std::size_t index) {
                    return (*this)[index];
                });
        }

    private:
        [[nodiscard]]
        DataOffset _offset() const {
            return Deserialiser<DataOffset>{
                fixed_data.subspan(FIXED_DATA_SIZE<ListSizeType>),
                variable_data
            }.value();
        }
    };

}

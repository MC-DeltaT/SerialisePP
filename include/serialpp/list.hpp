#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

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


    template<typename R, typename T>
    concept ListSerialiseSourceRange = std::ranges::forward_range<R> && std::ranges::sized_range<R>
        && std::convertible_to<std::ranges::range_reference_t<R>, SerialiseSource<T> const&>;


    template<Serialisable T>
    class SerialiseSource<List<T>> {
    public:
        // TODO: copyable

        // Constructs with 0 elements.
        SerialiseSource() :
            _range{EmptyRange{}}
        {}

        // Constructs from the elements of a braced-init-list.
        template<std::size_t N>
        SerialiseSource(SerialiseSource<T> (&& elements)[N]) :
            _range{EmptyRange{}}
        {
            _initialise_from_generic_range(std::to_array(std::move(elements)));
        }

        // Constructs from the elements of a range.
        // If the range is safe to view, then a view is taken (i.e. this object won't own the elements).
        // Otherwise the range is moved or copied from.
        template<class R> requires ListSerialiseSourceRange<R, T>
        SerialiseSource(R&& range) :
            _range{EmptyRange{}}
        {
            if constexpr (std::ranges::viewable_range<R>) {
                _initialise_from_generic_range(std::ranges::ref_view(range));
            }
            else {
                _initialise_from_generic_range(std::forward<R>(range));
            }
        }

    private:
        struct SerialiseVisitResult {
            SerialiseTarget new_target;
            std::size_t element_count;
        };

        struct SerialiseVisitorImpl {
            SerialiseTarget target;

            template<typename R>
            SerialiseVisitResult operator()(R&& range) const {
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

        // Tag for no/empty range (i.e. from default construction).
        struct EmptyRange {};

        struct SmallRangeVisitor {
            SerialiseVisitorImpl const& visitor;
            std::optional<SerialiseVisitResult> result;     // Can't default construct SerialiseTarget

            template<typename T>
            void operator()(T&& range) {
                result = visitor(range);
            }
        };

        // Probably most importantly, std::vector is 24 bytes on GCC, Clang, and MSVC.
        using SmallRangeWrapper = SmallAny<24, 8, SmallRangeVisitor>;

        struct LargeRangeWrapperBase {
            virtual ~LargeRangeWrapperBase() = default;

            virtual SerialiseVisitResult visit(SerialiseVisitorImpl const& visitor) const = 0;
        };

        template<class R>
        struct LargeRangeWrapper : LargeRangeWrapperBase {
            R range;

            template<typename... Args>
            LargeRangeWrapper(Args&&... args) :
                range{std::forward<Args>(args)...}
            {}

            SerialiseVisitResult visit(SerialiseVisitorImpl const& visitor) const override {
                return visitor(range);
            }
        };

        struct SerialiseVisitor : SerialiseVisitorImpl {
            SerialiseVisitResult operator()(EmptyRange) const noexcept {
                return {this->target, 0};       // Weirdly, target isn't found by unqualified lookup in MSVC.
            }

            SerialiseVisitResult operator()(SmallRangeWrapper const& wrapper) const {
                SmallRangeVisitor visitor{*this};
                wrapper.visit(visitor);
                return visitor.result.value();
            }

            SerialiseVisitResult operator()(std::unique_ptr<LargeRangeWrapperBase> const& wrapper) const {
                assert(wrapper);
                return wrapper->visit(*this);
            }
        };

        // The most common range types are small, so we can often avoid allocating on the heap.
        std::variant<
            EmptyRange,
            SmallRangeWrapper,
            std::unique_ptr<LargeRangeWrapperBase>
        > _range;

        template<typename R>
        void _initialise_from_generic_range(R&& range) {
            using RangeType = std::remove_cvref_t<R>;
            // If we're owning the elements and it's empty, just store EmptyRange instead.
            if (!std::ranges::borrowed_range<R> && std::ranges::empty(range)) {
                // _range already has EmptyRange.
                assert(std::holds_alternative<EmptyRange>(_range));
            }
            // If the range object is small enough, put it in the stack-allocated buffer.
            else if constexpr (std::constructible_from<SmallRangeWrapper, std::in_place_type_t<RangeType>, R&&>) {
                // Use emplace() rather than assign to avoid a move, which isn't free for SmallRangeWrapper.
                _range.emplace<SmallRangeWrapper>(std::in_place_type<RangeType>, std::forward<R>(range));
            }
            // Otherwise have to dynamically allocate space to type-erase the range.
            else {
                _range.emplace<std::unique_ptr<LargeRangeWrapperBase>>(
                    new LargeRangeWrapper<RangeType>{std::forward<R>(range)});
            }
        }

        SerialiseVisitResult _visit(SerialiseVisitor const& visitor) const {
            return std::visit(visitor, _range);
        }

        friend struct Serialiser<List<T>>;
    };

    template<Serialisable T>
    struct Serialiser<List<T>> {
        SerialiseTarget operator()(SerialiseSource<List<T>> const& source, SerialiseTarget target) const {
            auto const relative_variable_offset = target.relative_field_variable_offset();

            typename SerialiseSource<List<T>>::SerialiseVisitor const visitor{target};
            auto const [new_target, element_count] = source._visit(visitor);

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

        // Checks if the List contains no elements.
        [[nodiscard]]
        bool empty() const {
            return size() == 0;
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

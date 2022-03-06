#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <stdexcept>
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

    static inline constexpr std::size_t MAX_LIST_SIZE = std::numeric_limits<ListSizeType>::max();


    // Safely casts to ListSizeType.
    [[nodiscard]]
    inline ListSizeType to_list_size(std::size_t offset) {
        assert(std::cmp_less_equal(offset, std::numeric_limits<ListSizeType>::max()));
        return static_cast<ListSizeType>(offset);
    }


    // Serialisable variable-length homogeneous array. Can hold up to MAX_LIST_SIZE elements.
    template<Serialisable T>
    struct List {
        using SizeType = ListSizeType;

        static constexpr auto MAX_SIZE = MAX_LIST_SIZE;
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
        // TODO: copyable?

        // Constructs with zero elements.
        SerialiseSource() :
            _range{EmptyRange{}}
        {}

        // Constructs from the elements of a braced-init-list.
        template<std::size_t N> requires (N <= MAX_LIST_SIZE)
        SerialiseSource(SerialiseSource<T> (&& elements)[N]) :
            _range{EmptyRange{}}
        {
            if constexpr (N > 0) {
                // We initialise here directly from the elements to avoid unnecessary moves.
                using RangeType = std::array<SerialiseSource<T>, N>;
                // If the array is small enough, put it in the stack-allocated buffer.
                if constexpr (SmallRangeWrapper::template can_contain<RangeType>()) {
                    [this, &elements] <std::size_t... Is> (std::index_sequence<Is...>) {
                        // Use emplace() rather than assign to avoid a move, which isn't free for SmallRangeWrapper.
                        _range.emplace<SmallRangeWrapper>([&elements](void* data) {
                            return new(data) RangeType{{std::move(elements[Is])...}};
                        });
                    }(std::make_index_sequence<N>{});
                }
                // Otherwise have to dynamically allocate space to type-erase the range.
                else {
                    [this, &elements] <std::size_t... Is> (std::index_sequence<Is...>) {
                        _range.emplace<std::unique_ptr<LargeRangeWrapperBase>>(
                            new LargeRangeWrapper<RangeType>{std::move(elements[Is])...});
                    }(std::make_index_sequence<N>{});
                }
            }
        }

        // Constructs from the elements of a range. The range must have <= MAX_LIST_SIZE elements.
        // The range is stored within an instance of std::ranges::views::all.
        template<std::ranges::viewable_range R> requires ListSerialiseSourceRange<std::ranges::views::all_t<R>, T>
        SerialiseSource(R&& range) :
            _range{EmptyRange{}}
        {
            assert(std::size(range) <= MAX_LIST_SIZE);

            // We initialise here directly from the original range to avoid unnecessary moves.
            using RangeType = std::ranges::views::all_t<R>;
            // If we're owning the elements and it's empty, just store EmptyRange instead.
            if (!std::ranges::borrowed_range<R> && std::ranges::empty(range)) {
                // _range already has EmptyRange.
                assert(std::holds_alternative<EmptyRange>(_range));
            }
            // If the range object is small enough, put it in the stack-allocated buffer.
            else if constexpr (SmallRangeWrapper::template can_contain<RangeType>()) {
                // Use emplace() rather than assign to avoid a move, which isn't free for SmallRangeWrapper.
                _range.emplace<SmallRangeWrapper>(std::in_place_type<RangeType>, std::forward<R>(range));
            }
            // Otherwise have to dynamically allocate space to type-erase the range.
            else {
                _range.emplace<std::unique_ptr<LargeRangeWrapperBase>>(
                    new LargeRangeWrapper<RangeType>{std::forward<R>(range)});
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

        template<typename R>
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
                return {this->target, 0};
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

        // TODO: can we integrate the small range wrapper into the outer variant to avoid extra bookkeeping?
        
        // The most common range types are small, so we can often avoid allocating on the heap.
        std::variant<
            EmptyRange,
            SmallRangeWrapper,
            std::unique_ptr<LargeRangeWrapperBase>
        > _range;

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
    class Deserialiser<List<T>> : public DeserialiserBase<List<T>> {
    public:
        using DeserialiserBase<List<T>>::DeserialiserBase;

        // Gets the number of elements in the List.
        [[nodiscard]]
        std::size_t size() const {
            return Deserialiser<ListSizeType>{this->_fixed_data, this->_variable_data}.value();
        }

        // Checks if the List contains zero elements.
        [[nodiscard]]
        bool empty() const {
            return size() == 0;
        }

        // Gets the element at the specified index. index must be < size().
        [[nodiscard]]
        auto operator[](std::size_t index) const {
            assert(index < size());
            auto const base_offset = _offset();
            auto const element_offset = base_offset + FIXED_DATA_SIZE<T> * index;
            this->_check_variable_offset(element_offset);
            Deserialiser<T> const deserialiser{
                this->_variable_data.subspan(element_offset, FIXED_DATA_SIZE<T>),
                this->_variable_data
            };
            return auto_deserialise(deserialiser);
        }

        // Gets the element at the specified index. Throws std::out_of_range if index is out of bounds.
        [[nodiscard]]
        auto at(std::size_t index) const {
            auto const size = this->size();
            if (index < size) {
                return (*this)[index];
            }
            else {
                throw std::out_of_range{std::format("List index {} is out of bounds for size {}", index, size)};
            }
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
                this->_fixed_data.subspan(FIXED_DATA_SIZE<ListSizeType>, FIXED_DATA_SIZE<DataOffset>),
                this->_variable_data
            }.value();
        }
    };

}

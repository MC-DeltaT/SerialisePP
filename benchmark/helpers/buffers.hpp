#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

#include <serialpp/common.hpp>
#include <serialpp/utility.hpp>


namespace serialpp::benchmark {

	// Like basic_buffer but with fixed capacity; never reallocates.
	class preallocated_buffer {
    public:
        explicit constexpr preallocated_buffer(std::size_t const capacity) :
			// Zero-initialise to force load memory into the process.
			_data{new std::byte[capacity]{}},
			_capacity{capacity},
			_used{0}
        {}

		// Move and copy not needed.
		preallocated_buffer(preallocated_buffer&&) = delete;
		preallocated_buffer& operator=(preallocated_buffer&&) = delete;

        // Sets the size in bytes of the buffer, ready for a new serialisation.
        // If size is greater than the capacity, std::bad_alloc is thrown.
        // Returns the new value of span().
        constexpr mutable_bytes_span initialise(std::size_t const size) {
            if (_capacity < size) {
				throw std::bad_alloc{};
            }
			else {
				_used = size;
				return span();
			}
        }

        // Extends the buffer by count bytes, while maintaining the previous content.
        // If the new size is greater than the capacity, std::bad_alloc is thrown.
        // Returns the new value of span().
        constexpr mutable_bytes_span extend(std::size_t const count) {
            auto const new_used = _used + count;
            if (_capacity < new_used) {
				throw std::bad_alloc{};
            }
			else {
				_used = new_used;
				return span();
			}
        }

        [[nodiscard]]
        constexpr mutable_bytes_span span() noexcept {
            return {_data.get(), _used};
        }

        // Gets a view of the allocated storage.
        [[nodiscard]]
        constexpr mutable_bytes_span allocated_storage() noexcept {
            return {_data.get(), _capacity};
        }

    private:
        detail::constexpr_unique_ptr<std::byte[]> _data;
		std::size_t _capacity;
		std::size_t _used;
    };


    // serialise_buffer implementation that enables multiple sequential serialises into the one buffer.
    // Like preallocated_buffer, capacity is fixed.
    class sequential_buffer {
    public:
        explicit constexpr sequential_buffer(std::size_t const capacity) :
		    _buffer{capacity},
            _current_start{0}
        {}

        // Move and copy not needed.
		sequential_buffer(sequential_buffer&&) = delete;
		sequential_buffer& operator=(sequential_buffer&&) = delete;

        // Sets up for a new serialisation, without clearing the existing content from previous serialisations.
        // If size is greater than the capacity, std::bad_alloc is thrown.
        // Returns the new value of span().
        constexpr mutable_bytes_span initialise(std::size_t const size) {
            _current_start = _buffer.span().size();
            _buffer.extend(size);
            return span();
        }

        // Extends the buffer for the current serialisation by count bytes, while maintaining the existing content.
        // If the new size is greater than the capacity, std::bad_alloc is thrown.
        // Returns the new value of span().
        constexpr mutable_bytes_span extend(std::size_t const count) {
            _buffer.extend(count);
            return span();
        }

        // Returns the buffer for the current serialisation (i.e. does not include previous serialisations).
        [[nodiscard]]
        constexpr mutable_bytes_span span() noexcept {
            return _buffer.span().subspan(_current_start);
        }

       // Gets a view of the allocated storage.
        [[nodiscard]]
        constexpr mutable_bytes_span allocated_storage() noexcept {
            return _buffer.allocated_storage();
        }

        // Sets the buffer size to zero, clearing all previous serialisations.
        constexpr void clear() noexcept {
            _buffer.initialise(0);
            _current_start = 0;
        }

    private:
        preallocated_buffer _buffer;
        std::size_t _current_start;     // Index of the start of current serialisation in _buffer.
    };

}

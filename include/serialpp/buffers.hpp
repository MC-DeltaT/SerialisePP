#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <utility>

#include "common.hpp"
#include "utility.hpp"


namespace serialpp {

    // Basic serialise_buffer implementation that uses the frestore (new/delete) for storage.
    // Reallocations are done as necessary to extend the buffer while serialising.
    class basic_buffer {
    public:
        // capacity is the number of bytes to preallocate (similar to std::vector's capacity).
        // If preload is true, force the memory to be loaded into the process now. Otherwise, it may not be loaded until
        // the buffer is written to.
        explicit constexpr basic_buffer(std::size_t const capacity = 4096, bool const preload = true) :
            _data{new std::byte[capacity]},
            _capacity{capacity},
            _used{0}
        {
            if (preload) {
                // Initialising the memory forces the OS to load it in now, rather than later during serialisation.
                // This reduces latency when serialising into this buffer for the first time.
                std::fill_n(_data.get(), _capacity, std::byte{0});
            }
        }

        constexpr basic_buffer(basic_buffer&& other) noexcept :
            _data{std::exchange(other._data, nullptr)},
            _capacity{std::exchange(other._capacity, 0)},
            _used{std::exchange(other._used, 0)}
        {}

        constexpr basic_buffer& operator=(basic_buffer other) noexcept {
            swap(*this, other);
            return *this;
        }

        // Sets the size in bytes of the buffer, ready for a new serialisation.
        // If size is greater than the current capacity, the buffer is reallocated and any view of the buffer previously
        // acquired from span() will be invalidated. The newly allocated memory is not initialised.
        // Returns the new value of span().
        constexpr mutable_bytes_span initialise(std::size_t const size) {
            if (_capacity < size) {
                _data.reset(new std::byte[size]);
                _capacity = size;
            }
            _used = size;
            return span();
        }

        // Extends the buffer by count bytes, while maintaining the previous content.
        // If the new size is greater than the current capacity, the buffer is reallocated and any view of the buffer
        // previously acquired from span() will be invalidated. The new extended section of memory is not initialised.
        // Returns the new value of span().
        constexpr mutable_bytes_span extend(std::size_t const count) {
            auto const new_used = _used + count;
            if (_capacity < new_used) {
                auto const new_capacity = static_cast<std::size_t>(new_used * 1.5);
                detail::constexpr_unique_ptr<std::byte[]> new_data{new std::byte[new_capacity]};
                assert(_data || _used == 0);
                std::copy_n(_data.get(), _used, new_data.get());
                // Take ownership of new data last - strong exception guarantee.
                _data = std::move(new_data);
                _capacity = new_capacity;
            }
            _used = new_used;
            return span();
        }

        [[nodiscard]]
        constexpr const_bytes_span span() const noexcept {
            return {_data.get(), _used};
        }

        [[nodiscard]]
        constexpr mutable_bytes_span span() noexcept {
            return {_data.get(), _used};
        }

        [[nodiscard]]
        constexpr std::size_t capacity() const noexcept {
            return _capacity;
        }

        friend constexpr void swap(basic_buffer& first, basic_buffer& second) noexcept {
            using std::swap;
            swap(first._data, second._data);
            swap(first._capacity, second._capacity);
            swap(first._used, second._used);
        }

    private:
        detail::constexpr_unique_ptr<std::byte[]> _data;
        std::size_t _capacity;      // Number of bytes allocated.
        std::size_t _used;          // Number of bytes used at the start of the allocated memory.
    };


    namespace detail {

        // A runtime polymorphic interface for serialise_buffer, which is useful in some scenarios.
        class virtual_buffer {
        public:
            virtual constexpr mutable_bytes_span initialise(std::size_t size) = 0;

            virtual constexpr mutable_bytes_span extend(std::size_t count) = 0;

            [[nodiscard]]
            virtual constexpr mutable_bytes_span span() noexcept = 0;
        };


        // virtual_buffer implementation that wraps an existing serialise_buffer instance.
        template<serialise_buffer Buffer>
        class virtual_buffer_impl final : public virtual_buffer {
        public:
            constexpr virtual_buffer_impl(Buffer& buffer) noexcept :
                _buffer{buffer}
            {}

            constexpr mutable_bytes_span initialise(std::size_t size) final {
                return _buffer.initialise(size);
            }

            constexpr mutable_bytes_span extend(std::size_t count) final {
                return _buffer.extend(count);
            }

            [[nodiscard]]
            constexpr mutable_bytes_span span() noexcept final {
                return _buffer.span();
            }

        private:
            Buffer& _buffer;
        };


        // Wrapper for virtual_buffer that devirtualises part of the interface for performance improvements.
        class devirtualised_virtual_buffer {
        public:
            explicit constexpr devirtualised_virtual_buffer(std::derived_from<virtual_buffer> auto& base) noexcept :
                _base{base},
                _span{base.span()}
            {}

            constexpr mutable_bytes_span initialise(std::size_t const size) {
                return _span = _base.initialise(size);
            }

            constexpr mutable_bytes_span extend(std::size_t const count) {
                return _span = _base.extend(count);
            }

            [[nodiscard]]
            constexpr mutable_bytes_span span() noexcept {
                return _span;
            }

        private:
            virtual_buffer& _base;

            // Accessing the buffer's data span is common, so we can avoid virtual calls and gain performance by storing
            // it here.
            // Must ensure that this is updated every time initialise() or extend() is called.
            mutable_bytes_span _span;
        };

    }

}

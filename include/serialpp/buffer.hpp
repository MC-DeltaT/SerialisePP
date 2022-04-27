#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "common.hpp"


namespace serialpp {

    // Basic serialise_buffer implementation that uses an std::vector for the underlying storage.
    // Allocations are done as necessary to extend the buffer while serialising, via Allocator.
    template<class Allocator = std::allocator<std::byte>>
    class basic_serialise_buffer {
    public:
        using allocator_type = Allocator;
        using container_type = std::vector<std::byte, Allocator>;

        explicit constexpr basic_serialise_buffer(std::size_t reserved_size = 4096,
                Allocator const& allocator = Allocator{}) :
            _data(allocator)
        {
            _data.reserve(reserved_size);
        }

        // Accesses the underlying container.
        [[nodiscard]]
        constexpr container_type& container() noexcept {
            return _data;
        }

        // Accesses the underlying container.
        [[nodiscard]]
        constexpr container_type const& container() const noexcept {
            return _data;
        }

        [[nodiscard]]
        constexpr mutable_bytes_span span() noexcept {
            return {_data};
        }

        [[nodiscard]]
        constexpr const_bytes_span span() const noexcept {
            return {_data};
        }

        // Initialises the buffer to have the specified size. Any previous content may be erased.
        constexpr void initialise(std::size_t size) {
            _data.assign(size, std::byte{});
        }

        // Adds more space to the end of the buffer, without modifying the existing data.
        constexpr void extend(std::size_t count) {
            _data.resize(_data.size() + count);
        }

    private:
        container_type _data;
    };

}

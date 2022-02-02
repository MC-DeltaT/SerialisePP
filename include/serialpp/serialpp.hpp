#pragma once

#include <cassert>

#include "common.hpp"
#include "list.hpp"
#include "optional.hpp"
#include "scalar.hpp"
#include "struct.hpp"


namespace serialpp {

    // Serialises an entire object.
    template<Serialisable T>
    void serialise(SerialiseSource<T> const& source, SerialiseTarget target) {
        Serialiser<T>{}(source, target);
    }


    // Obtains a deserialiser for a type.
    // buffer should contain exactly an instance of T (i.e. you can only use this function to deserialise the result of
    // serialising an entire object with serialise()).
    template<Serialisable T>
    Deserialiser<T> deserialise(ConstBytesView buffer) {
        constexpr auto fixed_size = FIXED_DATA_SIZE<T>;
        assert(buffer.size() >= fixed_size);
        return Deserialiser<T>{buffer.first(fixed_size), buffer.subspan(fixed_size)};
    }

}


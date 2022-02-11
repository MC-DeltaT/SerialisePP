#pragma once

#include <serialpp/utility.hpp>


namespace serialpp::test {

    // Needs to be a concept to test failure condition.
    template<class Tuple, ConstantString N>
    concept CanGetNamedTuple = requires (Tuple t) {
        t.get<N>();
    };

}


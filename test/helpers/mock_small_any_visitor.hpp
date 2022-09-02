#pragma once

#include <typeinfo>


namespace serialpp::test {


    template<typename T>
    struct mock_small_any_visitor {
        T const* value = nullptr;
        std::type_info const* type = nullptr;

        template<typename T>
        void operator()(T&& value) {
            this->value = &value;
            type = &typeid(T);
        }
    };

}


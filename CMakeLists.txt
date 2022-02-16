cmake_minimum_required(VERSION 3.22)

project(SerialisePP CXX)

add_subdirectory(libraries/SimpleTest)

add_library(SerialisePP INTERFACE)
target_include_directories(SerialisePP INTERFACE include/)

add_executable(SerialisePPTest
    test/test_common.cpp
    test/test_list.cpp
    test/test_optional.cpp
    test/test_scalar.cpp
    test/test_struct.cpp
    test/test_utility.cpp
)
target_include_directories(SerialisePPTest PRIVATE test/)
target_link_libraries(SerialisePPTest PRIVATE SerialisePP SimpleTest)
target_compile_features(SerialisePPTest PUBLIC cxx_std_23)
set_target_properties(SerialisePPTest PROPERTIES CXX_EXTENSIONS OFF)

cmake_minimum_required(VERSION 3.22)

project(SerialisePP VERSION 1.0.0 LANGUAGES CXX)
# TODO: update version when making changes.

add_library(SerialisePP INTERFACE)
target_include_directories(SerialisePP INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    $<INSTALL_INTERFACE:include>)
# We don't use C++23 features, but certain C++20 features are only implemented in some compilers' "latest" Standard
# mode, which currently corresponds to C++23.
target_compile_features(SerialisePP INTERFACE cxx_std_23)
set_target_properties(SerialisePP PROPERTIES CXX_EXTENSIONS OFF)

# Only build test code if specifically requested.
option(SERIALISEPP_BUILD_TEST "Build test executable" OFF)
if(SERIALISEPP_BUILD_TEST)
    message("Configuring to build SerialisePPTest")

    find_package(SimpleTest 1.1.0 REQUIRED)

    add_executable(SerialisePPTest
        test/test_buffer.cpp
        test/test_common.cpp
        test/test_compound.cpp
        test/test_dynamic_array.cpp
        test/test_optional.cpp
        test/test_pair.cpp
        test/test_record.cpp
        test/test_scalar.cpp
        test/test_static_array.cpp
        test/test_tuple.cpp
        test/test_utility.cpp
        test/test_variant.cpp
    )
    target_include_directories(SerialisePPTest PRIVATE test/)
    target_link_libraries(SerialisePPTest PRIVATE SerialisePP SimpleTest::SimpleTest)
    target_compile_features(SerialisePPTest PUBLIC cxx_std_23)
    set_target_properties(SerialisePPTest PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Don't configure installation if we're part of another project.
if(PROJECT_IS_TOP_LEVEL)
    # Set up targets for installation.
    include(GNUInstallDirs)
    install(TARGETS SerialisePP EXPORT "${PROJECT_NAME}Targets")
    install(EXPORT "${PROJECT_NAME}Targets" NAMESPACE "${PROJECT_NAME}::" DESTINATION cmake)
    # Copy source files.
    install(DIRECTORY include/ DESTINATION include)

    # Set up Config.cmake and ConfigVersion.cmake files for usage with find_package().
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/Config.cmake.in" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION cmake)
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" COMPATIBILITY SameMajorVersion)
    install(FILES
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION cmake)
endif()

# Serialise++ Importing/Install Guide

## CMake - `find_package()`

The recommended method is by installing and using `find_package()`, which will handle finding Serialise++ and checking version compatibility for you.

First, use CMake to install Serialise++. Navigate to the Serialise++ repo directory, then:

```bash
mkdir build
cd build
cmake ..
# Admin/root privileges may be required for this step.
cmake --install .
```

This installs the Serialise++ source code and exported CMake targets to your system (e.g. on Windows: `C:/Program Files/SerialisePP`, on Linux: `/usr/local`).

(A custom install location can be specified with the `--prefix` install option, and then pointing CMake to that location via `SerialisePP_ROOT` or `CMAKE_PREFIX_PATH`.)

Then in your project's `CMakeLists.txt`:

```cmake
find_package(SerialisePP 1.0.0 REQUIRED)

# For a library:
add_executable(MyLib my_lib.cpp)
target_link_libraries(MyLib PUBLIC SerialisePP::SerialisePP)

# For an executable:
add_executable(MyApp my_app.cpp)
target_link_libraries(MyApp PRIVATE SerialisePP::SerialisePP)
```

## CMake - `add_subdirectory()`

```cmake
add_subdirectory(path/to/SerialisePP)
```

Targets will be available without the `SerialisePP::` namespace.

This is not recommended, especially for library projects, as there is no protection against duplicate imports or incompatible versions.

## Manual

Add `path/to/SerialisePP/include` to your include directories.

# Serialise++: A simple and efficient serialisation library for C++

by Reece Jones

## Overview

Serialise++ is an easy-to-use library for efficient, generic serialisation of C++ values into bytes.
It uses a serialisation format inspired by [Cap'n Proto](https://capnproto.org/), to enable serialisation/deserialisation without copying large amount of data between C++ values and bytes buffers.
But unlike Cap'n Proto, Serialise++ enables definition of serialisable types in C++, with no extra code generation required!

## Features

 - Supported types:
   - Integers
   - Floating point numbers
   - Booleans
   - Static arrays
   - Dynamic arrays
   - Optional
   - User-defined structs
 - Extensible serialisation for custom types
 - On-the-fly serialisation and deserialisation - no data duplication!
 - Fully contained within C++ (no separate message/type definition, no code generation)

(Support for more types is likely to come in the future, e.g. strings, pairs).

## Limitations

Serialise++ is designed for fairly small messages. Currently there is a hard limit of ~2<sup>16</sup> bytes of variable-sized data.
As such, Lists can store at most 2<sup>16</sup> elements.  

That said, Serialise++ is not designed for situations where literally every bit of serialised data counts. There is currently no built-in support for bit-packing or variable length integers.

In situations where serialised size and serialisation latency are required to be ultra-optimised, Serialise++ probably isn't appropriate. Some performance must be forgone in exchange for generality, flexibility, and usability.

## Requirements

 - C++20
 - CMake 3.22 or newer (unless you want to build manually)
 - Platform with 8-bit bytes
 - Platform with IEEE-754 floats and nonmixed endianness (for floating point number support)

## Dependencies

 - [SimpleTest](https://github.com/MC-DeltaT/SimpleTest)

Included as Git submodules in `libraries/`.

## Usage

### CMake

Add the Serialise++ project root:

```cmake
add_subdirectory(path/to/SerialisePP)
```

Then add Serialise++ to your CMake target:

```cmake
target_link_libraries(MyApp SerialisePP)
```

### Without CMake

Add `path/to/SerialisePP/include` to your include directories.

### Code

Please see [ProgrammingGuide.md](ProgrammingGuide.md) for detailed information.

## Tests

The library has unit tests made with [SimpleTest](https://github.com/MC-DeltaT/SimpleTest).
The test code is contained in `test/` and the CMake target is `SerialisePPTest`.

## Serialisation Format

Please see [SerialisationFormat.md](SerialisationFormat.md) for details on how C++ values are represented as bytes.
